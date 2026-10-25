#include "services/SubscriptionUpdateService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "common/UserAgent.h"
#include "services/ServiceTimeouts.h"
#include "services/SubscriptionUpdateOrder.h"
#include "subscription/ShareUrlParser.h"
#include "subscription/SubscriptionContentParser.h"
#include "subscription/SubscriptionUsageParser.h"

namespace {
QString subscriptionDisplayName(const SubItem& item)
{
    return item.remarks.trimmed().isEmpty() ? item.url.trimmed() : item.remarks.trimmed();
}

constexpr int kSubscriptionDownloadTimeoutMs = ServiceTimeouts::kDefaultNetworkTimeoutMs;
constexpr int kCancellationPollIntervalMs = ServiceTimeouts::kCancellationPollIntervalMs;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

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

// When nothing could be parsed at all, the per-pass notes are the only clue as
// to why, so attach them to the failure instead of discarding them.
QString appendParseNotes(const QString& message, const SubscriptionParseReport& report)
{
    return report.notes.isEmpty()
        ? message
        : QStringLiteral("%1 [%2]").arg(message, report.notes.join(QStringLiteral(" | ")));
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


SubscriptionUpdateService::SubscriptionUpdateService(
    IConfigRepository& repository,
    SubscriptionService& subscriptionService,
    QNetworkAccessManager& networkAccessManager)
    : repository_(repository)
    , subscriptionService_(subscriptionService)
    , networkAccessManager_(networkAccessManager)
{
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
        return OperationResult::fail(appendParseNotes(
            QCoreApplication::translate("SubscriptionUpdateService", "No supported share URL or subscription payload was detected."), report));
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
        if (progressCallback) {
            progressCallback(completedCount, items.size(), result.message);
        }
    }

    QStringList lines;
    lines.append(QStringLiteral("Updated %1 of %2 subscription(s), imported %3 server(s).")
                     .arg(updatedCount)
                     .arg(items.size())
                     .arg(importedCount));

    for (const QString& line : successLines) {
        lines.append(QStringLiteral("[OK] %1").arg(line));
    }

    for (const QString& line : failureLines) {
        lines.append(QStringLiteral("[FAIL] %1").arg(line));
    }

    return failureLines.isEmpty()
        ? OperationResult::ok(lines.join(QStringLiteral("\n")))
        : OperationResult::fail(lines.join(QStringLiteral("\n")));
}

OperationResult SubscriptionUpdateService::updateSingle(Config& config, const SubItem& item, int* importedCount, bool useProxy)
{
    if (importedCount != nullptr) {
        *importedCount = 0;
    }

    const QString displayName = subscriptionDisplayName(item);
    if (!item.enabled) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 is disabled.").arg(displayName));
    }

    if (item.url.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 has an empty URL.").arg(displayName));
    }
    if (currentThreadInterruptionRequested()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
    }

    const int proxyPort = useProxy ? config.localPort + 1 : 0;

    // Retry loop: up to kMaxDownloadAttempts attempts with back-off between failures.
    // A 304 Not Modified response short-circuits the loop immediately.
    QString content;
    QString newEtag;
    QString newLastModified;
    QString newUserInfo;
    OperationResult downloadResult = OperationResult::fail(QString());
    for (int attempt = 0; attempt < kMaxDownloadAttempts; ++attempt) {
        if (currentThreadInterruptionRequested()) {
            return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
        }

        bool notModified = false;
        downloadResult = downloadText(
            item.url, item.userAgent, proxyPort,
            item.etag, item.lastModified,
            &content, &newEtag, &newLastModified, &notModified, &newUserInfo);

        if (notModified) {
            // The server confirmed the cached copy is still current. Update the
            // fail-streak to 0 and persist the (possibly refreshed) headers, but
            // do not replace the server list: nothing changed.
            for (SubItem& sub : config.collection().subscriptions) {
                if (sub.id == item.id) {
                    sub.etag = newEtag.isEmpty() ? item.etag : newEtag;
                    sub.lastModified = newLastModified.isEmpty() ? item.lastModified : newLastModified;
                    sub.failStreak = 0;
                    const auto usage = SubscriptionUsageParser::parse(newUserInfo, QDateTime::currentSecsSinceEpoch());
                    if (usage) {
                        sub.usage = *usage;
                    }
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
            for (SubItem& sub : config.collection().subscriptions) {
                if (sub.id == item.id) {
                    sub.failStreak = item.failStreak + 1;
                    break;
                }
            }
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

    const SubscriptionParseReport report = SubscriptionContentParser::parseManyWithReport(content);
    const QList<VmessItem> servers = report.items;
    if (servers.isEmpty()) {
        return OperationResult::fail(appendParseNotes(
            QCoreApplication::translate("SubscriptionUpdateService", "%1 returned no supported servers.").arg(displayName), report));
    }
    if (currentThreadInterruptionRequested()) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 update cancelled.").arg(displayName));
    }

    // Apply metadata before replacing servers: replaceSubscriptionServers saves
    // the whole config, including the subscription state companion file.
    for (SubItem& sub : config.collection().subscriptions) {
        if (sub.id == item.id) {
            sub.etag = newEtag;
            sub.lastModified = newLastModified;
            sub.failStreak = 0;
            const auto usage = SubscriptionUsageParser::parse(newUserInfo, QDateTime::currentSecsSinceEpoch());
            if (usage) {
                sub.usage = *usage;
            }
            break;
        }
    }

    const OperationResult replaceResult = subscriptionService_.replaceSubscriptionServers(config, item.id, servers);
    if (!replaceResult.success) {
        return OperationResult::fail(QCoreApplication::translate("SubscriptionUpdateService", "%1 save failed: %2").arg(displayName, replaceResult.message));
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
    bool* outNotModified,
    QString* outUserInfo)
{
    if (outUserInfo != nullptr) {
        outUserInfo->clear();
    }
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
    if (outUserInfo != nullptr) {
        *outUserInfo = QString::fromUtf8(reply->rawHeader("Subscription-Userinfo")).trimmed();
    }
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
