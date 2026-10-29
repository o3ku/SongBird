#include "services/SubscriptionUpdateService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "common/LocalInboundPorts.h"
#include "common/UserAgent.h"
#include "services/ServiceTimeouts.h"
#include "services/SubscriptionUpdateFailureSummary.h"
#include "services/SubscriptionUpdateOrder.h"
#include "subscription/ShareUrlParser.h"
#include "subscription/SubscriptionContentParser.h"

namespace {
constexpr int kSubscriptionDownloadTimeoutMs = ServiceTimeouts::kDefaultNetworkTimeoutMs;
constexpr int kCancellationPollIntervalMs = ServiceTimeouts::kCancellationPollIntervalMs;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

// Filename-safe form of the subscription identity for the raw backup: Windows-invalid
// characters collapse to '_', the id suffix keeps two subscriptions whose remarks
// collide from overwriting each other's archive.
QString buildRawBackupFileName(const SubItem& item)
{
    static const QString kInvalidChars = QStringLiteral("<>:\"/\\|?*");
    QString cleaned;
    for (const QChar ch : item.remarks) {
        if (kInvalidChars.contains(ch) || ch.unicode() < 0x20) {
            cleaned += QLatin1Char('_');
        } else {
            cleaned += ch;
        }
    }
    cleaned = cleaned.left(64).trimmed();
    while (cleaned.endsWith(QLatin1Char('.'))) {
        cleaned.chop(1);
    }
    if (cleaned.isEmpty()) {
        cleaned = item.id;
    }
    return QStringLiteral("%1_%2.raw").arg(cleaned, item.id.left(8));
}

// Best-effort archive of the payload exactly as received. A write failure is logged
// and swallowed: archiving must never fail an update that otherwise succeeded, which
// is also why this runs after the download loop but before parsing -- an unparseable
// payload is the one a user is most likely to want the original bytes of.
void writeRawSubscriptionBackup(const QString& directoryPath, const SubItem& item, const QString& content)
{
    QDir directory(directoryPath);
    if (!directory.exists() && !QDir().mkpath(directoryPath)) {
        qWarning() << "SubscriptionUpdateService: cannot create raw backup directory" << directoryPath;
        return;
    }

    QSaveFile file(directory.filePath(buildRawBackupFileName(item)));
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "SubscriptionUpdateService: cannot open raw backup file" << file.fileName();
        return;
    }
    file.write(content.toUtf8());
    if (!file.commit()) {
        qWarning() << "SubscriptionUpdateService: failed to commit raw backup file" << file.fileName();
    }
}

bool currentThreadInterruptionRequested()
{
    QThread* const currentThread = QThread::currentThread();
    return currentThread != nullptr && currentThread->isInterruptionRequested();
}

// Surfaces nodes the parser had to drop, so a shrinking server count is
// traceable instead of silent.
QString appendSkippedSummary(const QString& message, const SubscriptionParseReport& report)
{
    return report.hasSkippedNodes()
        ? QStringLiteral("%1 (%2)").arg(message, report.skippedSummary())
        : message;
}

// When nothing could be parsed at all, the skipped summary and the per-pass notes
// are the only clue as to why, so attach them to the failure instead of discarding
// them. The summary leads, because it is the answer to the question the user actually
// asks ("the airport is broken" and "these protocols are unsupported" need different
// next steps); the notes are the trail behind it.
//
// The trail goes on its own line, and the skip list stays in front of it. Two
// callers put the summary on the reason line themselves -- appendSkippedSummary()
// on success, and this function on failure -- so the split is what keeps the
// server panel, which shows only the first line, reading the reason and the
// protocols rather than a paragraph of `pass N clash count=0`.
QString appendParseNotes(const QString& message, const SubscriptionParseReport& report)
{
    QString detail = message;
    if (report.hasSkippedNodes()) {
        detail = QStringLiteral("%1 %2").arg(detail, report.skippedSummary());
    }
    return report.notes.isEmpty()
        ? detail
        : QStringLiteral("%1\n%2").arg(detail, report.notes.join(QStringLiteral(" | ")));
}

// The fail-streak answers one question: has this URL been unreachable? Only a
// transport failure is evidence for it. A subscription that downloads fine and then
// fails to parse is reachable and permanently unparseable -- retrying returns the
// same bytes -- so counting it would mark a working subscription unstable.
void recordDownloadFailure(Config& config, const SubItem& item)
{
    for (SubItem& sub : config.collection().subscriptions) {
        if (sub.id == item.id) {
            sub.failStreak = item.failStreak + 1;
            break;
        }
    }
}

void clearFailureStreak(Config& config, const SubItem& item)
{
    if (item.failStreak == 0) {
        return;
    }
    for (SubItem& sub : config.collection().subscriptions) {
        if (sub.id == item.id) {
            sub.failStreak = 0;
            break;
        }
    }
}

// Back-off intervals between retry attempts (index 0 = after first failure).
// Each value is in milliseconds. The final attempt has no trailing wait.
constexpr int kRetryIntervalMs[] = {300, 800};
constexpr int kMaxDownloadAttempts = 3;

// Blocks the calling thread for the given number of milliseconds while remaining
// responsive to shutdown interruption. Returns false if interrupted.
bool interruptibleSleep(int ms)
{
    constexpr int kSliceMs = 50;
    int remaining = ms;
    while (remaining > 0) {
        if (currentThreadInterruptionRequested()) {
            return false;
        }
        QThread::msleep(static_cast<unsigned long>(qMin(remaining, kSliceMs)));
        remaining -= kSliceMs;
    }
    return !currentThreadInterruptionRequested();
}

} // namespace

SubscriptionUpdateService::SubscriptionUpdateService(
    IConfigRepository& repository,
    SubscriptionService& subscriptionService,
    QNetworkAccessManager& networkAccessManager)
    : repository_(repository)
    , subscriptionService_(subscriptionService)
    , networkAccessManager_(networkAccessManager)
{
}


void SubscriptionUpdateService::setRawBackupDirectory(const QString& directoryPath)
{
    rawBackupDirectory_ = directoryPath;
}
OperationResult SubscriptionUpdateService::updateAll(Config& config, bool useProxy)
{
    QList<SubItem> items;
    for (const SubItem& item : config.collection().subscriptions) {
        if (!item.enabled || item.url.trimmed().isEmpty()) {
            continue;
        }
        items.append(item);
    }

    if (items.isEmpty()) {
        return OperationResult::ok(QCoreApplication::translate("SubscriptionUpdateService", "No enabled subscriptions needed updating."));
    }

    return updateInternal(config, items, useProxy);
}

OperationResult SubscriptionUpdateService::updateByIds(Config& config, const QStringList& subscriptionIds, bool useProxy)
{
    QStringList normalizedIds;
    for (const QString& id : subscriptionIds) {
        const QString trimmed = id.trimmed();
        if (!trimmed.isEmpty() && !normalizedIds.contains(trimmed)) {
            normalizedIds.append(trimmed);
        }
    }

    if (normalizedIds.isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "No subscriptions were selected for updating."));
    }

    QList<SubItem> items;
    for (const SubItem& item : config.collection().subscriptions) {
        if (normalizedIds.contains(item.id)) {
            items.append(item);
        }
    }

    if (items.isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "The selected subscriptions could not be found."));
    }

    return updateInternal(config, items, useProxy);
}

OperationResult SubscriptionUpdateService::updateAllWithProgress(
    Config& config,
    bool useProxy,
    const ProgressCallback& progressCallback)
{
    QList<SubItem> items;
    for (const SubItem& item : config.collection().subscriptions) {
        if (!item.enabled || item.url.trimmed().isEmpty()) {
            continue;
        }
        items.append(item);
    }

    if (items.isEmpty()) {
        return OperationResult::ok(QCoreApplication::translate("SubscriptionUpdateService", "No enabled subscriptions needed updating."));
    }

    return updateInternal(config, items, useProxy, progressCallback);
}

OperationResult SubscriptionUpdateService::importFromText(Config& config, const QString& text)
{
    const SubscriptionParseReport report = SubscriptionContentParser::parseManyWithReport(text);
    QList<VmessItem> servers = report.items;
    if (servers.isEmpty()) {
        servers = ShareUrlParser::parseMany(text);
    }

    if (servers.isEmpty()) {
        // The skipped types are why, when every node was a protocol this build does
        // not support; appendParseNotes() leads with them, on the line above the trail.
        return OperationResult::fail(appendParseNotes(
            QCoreApplication::translate("SubscriptionUpdateService", "No supported share URL or subscription payload was detected."),
            report));
    }

    for (VmessItem& item : servers) {
        item.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        config.collection().servers.append(item);
    }

    if (config.currentIndexId.isEmpty() && !config.collection().servers.isEmpty()) {
        config.currentIndexId = config.collection().servers.constFirst().indexId;
    }

    if (!repository_.save(config)) {
        return repository_.saveFailureResult(QCoreApplication::translate("SubscriptionUpdateService", "Failed to save imported servers."));
    }

    return OperationResult::ok(
        appendSkippedSummary(QCoreApplication::translate("SubscriptionUpdateService", "Imported %1 server(s) from text input.").arg(servers.size()), report));
}

OperationResult SubscriptionUpdateService::updateInternal(
    Config& config,
    const QList<SubItem>& items,
    bool useProxy,
    const ProgressCallback& progressCallback)
{
    int updatedCount = 0;
    int importedCount = 0;
    QStringList successLines;
    QStringList failureLines;
    // Which subscription each failure line belongs to. The line itself is prose for the log
    // panel -- it leads with the display name, which two subscriptions can share -- so the UI
    // that needs "why did *this* one fail" is handed the id instead of parsing it back out.
    QList<OperationResult::SubjectFailure> subjectFailures;
    int completedCount = 0;

    // Walk the batch in an order that spreads subscriptions sharing a host apart and defers the ones
    // that keep failing. The loop itself stays serial -- config is shared mutable state and
    // parallelising it would be a much larger change -- but the order still decides how the servers
    // see the traffic, which is what the grouping is for.
    const QList<SubItem> orderedItems = SubscriptionUpdateOrder::orderForUpdate(items);

    for (const SubItem& item : orderedItems) {
        // Honor cancellation requested by AppBootstrap::waitForBackgroundThreads()
        // at shutdown so we don't keep downloading subscriptions while the app
        // is trying to exit. Per-request timeouts already bound the in-flight
        // call below; this just stops scheduling new ones.
        if (currentThreadInterruptionRequested()) {
            failureLines.append(QStringLiteral("Subscription update cancelled by shutdown."));
            break;
        }

        int currentImportedCount = 0;
        const OperationResult result = updateSingle(config, item, &currentImportedCount, useProxy);
        completedCount++;
        if (result.success) {
            updatedCount++;
            importedCount += currentImportedCount;
            successLines.append(result.message);
            if (progressCallback) {
                progressCallback(completedCount, items.size(), result.message);
            }
            continue;
        }

        failureLines.append(result.message);
        subjectFailures.append({item.id, result.message});
        if (progressCallback) {
            progressCallback(completedCount, items.size(), result.message);
        }
    }

    const QString totalsLine = QStringLiteral("Updated %1 of %2 subscription(s), imported %3 server(s).")
                                   .arg(updatedCount)
                                   .arg(items.size())
                                   .arg(importedCount);
    // One subscription, nothing imported: the totals say nothing the reason does not,
    // and the empty server panel shows the first line of this message only. With the
    // totals in front, that panel read "Updated 0 of 1 subscription(s), imported 0
    // server(s)." -- a restatement of "there are no servers" -- while the protocols it
    // could not read sat on the line behind it. So the reason takes the front line and
    // the totals move behind it. A batch keeps the totals in front: there the counts
    // are how the user finds which subscription failed, and there is more than one
    // line to read anyway.
    const bool leadWithReason = items.size() == 1 && failureLines.size() == 1;

    QStringList lines;
    if (!leadWithReason) {
        lines.append(totalsLine);
    }
    for (const QString& line : successLines) {
        lines.append(QStringLiteral("[OK] %1").arg(line));
    }
    for (const QString& line : failureLines) {
        lines.append(QStringLiteral("[FAIL] %1").arg(line));
    }
    if (leadWithReason) {
        lines.append(totalsLine);
    }

    if (failureLines.isEmpty()) {
        return OperationResult::ok(lines.join(QStringLiteral("\n")));
    }

    OperationResult result = OperationResult::fail(lines.join(QStringLiteral("\n")));
    result.subjectFailures = subjectFailures;
    return result;
}

OperationResult SubscriptionUpdateService::updateSingle(Config& config, const SubItem& item, int* importedCount, bool useProxy)
{
    if (importedCount != nullptr) {
        *importedCount = 0;
    }

    const QString displayName = SubscriptionUpdateFailureSummary::displayName(item);
    if (!item.enabled) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 is disabled.").arg(displayName));
    }

    if (item.url.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 has an empty URL.").arg(displayName));
    }
    if (currentThreadInterruptionRequested()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
    }

    const int proxyPort = useProxy ? LocalInboundPorts::httpPort(config) : 0;

    // Retry loop: up to kMaxDownloadAttempts attempts with back-off between failures.
    // A 304 Not Modified response short-circuits the loop immediately.
    QString content;
    QString newEtag;
    QString newLastModified;
    OperationResult downloadResult = OperationResult::fail(QString());
    for (int attempt = 0; attempt < kMaxDownloadAttempts; ++attempt) {
        if (currentThreadInterruptionRequested()) {
            return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
        }

        bool notModified = false;
        downloadResult = downloadText(
            item.url, item.userAgent, proxyPort,
            item.etag, item.lastModified,
            &content, &newEtag, &newLastModified, &notModified);

        if (notModified) {
            // The server confirmed the cached copy is still current. Update the
            // fail-streak to 0 and persist the (possibly refreshed) headers, but
            // do not replace the server list: nothing changed.
            const qint64 confirmedAtMs = QDateTime::currentMSecsSinceEpoch();
            for (SubItem& sub : config.collection().subscriptions) {
                if (sub.id == item.id) {
                    sub.etag = newEtag.isEmpty() ? item.etag : newEtag;
                    sub.lastModified = newLastModified.isEmpty() ? item.lastModified : newLastModified;
                    sub.failStreak = 0;
                    // A 304 is a successful check, and the timestamp answers "how stale is what
                    // I am showing". Leaving it alone would make an hourly-refreshed
                    // subscription look untouched since the last time the document actually
                    // changed, which is precisely the question this field exists for.
                    sub.lastUpdatedAtMs = confirmedAtMs;
                    break;
                }
            }
            if (!repository_.save(config)) {
                // Non-fatal: the in-memory state is already correct.
            }
            return OperationResult::ok(QCoreApplication::translate("SubscriptionUpdateService", "%1 is up to date (not modified).").arg(displayName));
        }

        if (downloadResult.success) {
            break;
        }

        // Transient failure: update fail-streak now so it persists even if we
        // eventually give up. Persist after last attempt to avoid redundant saves.
        const bool lastAttempt = (attempt + 1 >= kMaxDownloadAttempts);
        if (lastAttempt) {
            recordDownloadFailure(config, item);
            repository_.save(config);
            break;
        }

        // Back-off before the next attempt.
        if (!interruptibleSleep(kRetryIntervalMs[attempt])) {
            return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
        }
    }

    if (!downloadResult.success) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 download failed: %2").arg(displayName, downloadResult.message));
    }
    if (currentThreadInterruptionRequested()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
    }
    if (!rawBackupDirectory_.trimmed().isEmpty() && !content.trimmed().isEmpty()) {
        writeRawSubscriptionBackup(rawBackupDirectory_.trimmed(), item, content);
    }

    const SubscriptionParseReport report = SubscriptionContentParser::parseManyWithReport(content);

    const QList<VmessItem> servers = report.items;
    if (servers.isEmpty()) {
        // The content arrived whole and was not a document this build can read, so the
        // failure is permanent: retrying the same URL returns the same bytes, and
        // counting it as a download failure marked a perfectly reachable subscription
        // unstable after three updates. The streak measures the transport, not the
        // parse, so it is reset -- this subscription answered.
        clearFailureStreak(config, item);

        // Same reason as the text-import path: a document full of unsupported
        // protocols fails here, and "returned no supported servers" without naming
        // the protocols it did contain is a message the user cannot act on.
        return OperationResult::fail(appendParseNotes(
            QCoreApplication::translate("SubscriptionUpdateService", "%1 returned no supported servers.").arg(displayName),
            report));
    }
    if (currentThreadInterruptionRequested()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
    }

    const OperationResult replaceResult = subscriptionService_.replaceSubscriptionServers(config, item.id, servers);
    if (!replaceResult.success) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 save failed: %2").arg(displayName, replaceResult.message));
    }

    // Persist the new caching headers, the refresh time and reset the fail-streak.
    const qint64 downloadedAtMs = QDateTime::currentMSecsSinceEpoch();
    for (SubItem& sub : config.collection().subscriptions) {
        if (sub.id == item.id) {
            sub.etag = newEtag;
            sub.lastModified = newLastModified;
            sub.failStreak = 0;
            sub.lastUpdatedAtMs = downloadedAtMs;
            break;
        }
    }

    if (importedCount != nullptr) {
        *importedCount = 0;
        for (const VmessItem& server : config.collection().servers) {
            if (server.subId == item.id) {
                (*importedCount)++;
            }
        }
    }

    const int finalImportedCount = importedCount == nullptr ? servers.size() : *importedCount;
    return OperationResult::ok(appendSkippedSummary(
        QCoreApplication::translate("SubscriptionUpdateService", "%1 imported %2 server(s).").arg(displayName).arg(finalImportedCount), report));
}

OperationResult SubscriptionUpdateService::downloadText(
    const QString& url,
    const QString& userAgent,
    int proxyPort,
    const QString& ifNoneMatch,
    const QString& ifModifiedSince,
    QString* content,
    QString* outEtag,
    QString* outLastModified,
    bool* outNotModified)
{
    if (content == nullptr) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "Download target buffer is null."));
    }
    if (currentThreadInterruptionRequested()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "Subscription download cancelled."));
    }

    const QUrl parsedUrl = QUrl::fromUserInput(url.trimmed());
    if (!parsedUrl.isValid() || parsedUrl.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "Subscription URL is invalid."));
    }

    QNetworkRequest request{parsedUrl};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    const QString resolvedUA = resolveSubscriptionUserAgent(userAgent);
    request.setRawHeader("User-Agent", resolvedUA.toUtf8());
    if (!ifNoneMatch.trimmed().isEmpty()) {
        request.setRawHeader("If-None-Match", ifNoneMatch.trimmed().toUtf8());
    }
    if (!ifModifiedSince.trimmed().isEmpty()) {
        request.setRawHeader("If-Modified-Since", ifModifiedSince.trimmed().toUtf8());
    }

    const QNetworkProxy previousProxy = networkAccessManager_.proxy();
    if (proxyPort > 0) {
        networkAccessManager_.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, kLoopbackAddress, proxyPort));
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    QTimer cancellationTimer;
    timeoutTimer.setSingleShot(true);
    cancellationTimer.setInterval(kCancellationPollIntervalMs);
    bool timedOut = false;
    bool cancelled = false;
    QNetworkReply* reply = networkAccessManager_.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        if (!currentThreadInterruptionRequested()) {
            return;
        }
        cancelled = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(kSubscriptionDownloadTimeoutMs);
    cancellationTimer.start();
    loop.exec();
    timeoutTimer.stop();
    cancellationTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (cancelled) {
        networkAccessManager_.setProxy(previousProxy);
        reply->deleteLater();
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "Subscription download cancelled."));
    }

    if (timedOut) {
        networkAccessManager_.setProxy(previousProxy);
        reply->deleteLater();
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "Subscription download timed out."));
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = reply->errorString();
        networkAccessManager_.setProxy(previousProxy);
        reply->deleteLater();
        return statusCode > 0
            ? OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "HTTP %1: %2").arg(statusCode).arg(error))
            : OperationResult::fail(error);
    }

    // Capture caching headers before reading the body so callers can persist
    // them even when notModified == true (body is empty on 304).
    const QString etag = QString::fromUtf8(reply->rawHeader("ETag")).trimmed();
    const QString lastModified = QString::fromUtf8(reply->rawHeader("Last-Modified")).trimmed();
    if (outEtag != nullptr) {
        *outEtag = etag;
    }
    if (outLastModified != nullptr) {
        *outLastModified = lastModified;
    }

    // 304 Not Modified: body is intentionally empty; this is a success path.
    if (statusCode == 304) {
        networkAccessManager_.setProxy(previousProxy);
        reply->deleteLater();
        if (outNotModified != nullptr) {
            *outNotModified = true;
        }
        return OperationResult::ok(QCoreApplication::translate("SubscriptionUpdateService", "Subscription not modified."));
    }
    if (outNotModified != nullptr) {
        *outNotModified = false;
    }

    *content = QString::fromUtf8(reply->readAll()).trimmed();
    networkAccessManager_.setProxy(previousProxy);
    reply->deleteLater();

    if (statusCode >= 400) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "HTTP %1").arg(statusCode));
    }

    if (content->isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "Subscription response is empty."));
    }

    return OperationResult::ok(QCoreApplication::translate("SubscriptionUpdateService", "Subscription downloaded."));
}
