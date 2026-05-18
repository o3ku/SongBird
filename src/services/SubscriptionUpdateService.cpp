#include "services/SubscriptionUpdateService.h"

#include <algorithm>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include "subscription/ShareUrlParser.h"
#include "subscription/SubscriptionContentParser.h"

namespace {
QString subscriptionDisplayName(const SubItem& item)
{
    return item.remarks.trimmed().isEmpty() ? item.url.trimmed() : item.remarks.trimmed();
}

constexpr int kSubscriptionDownloadTimeoutMs = 30000;
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

QString summarizeContentPrefix(const QString& text)
{
    QString summary = text.left(120);
    summary.replace(QChar('\r'), QChar(' '));
    summary.replace(QChar('\n'), QChar(' '));
    return summary;
}
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
    for (const SubItem& item : config.subscriptions) {
        if (!item.enabled || item.url.trimmed().isEmpty()) {
            continue;
        }
        items.append(item);
    }

    if (items.isEmpty()) {
        return OperationResult::ok(QStringLiteral("No enabled subscriptions needed updating."));
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
        return OperationResult::fail(QStringLiteral("No subscriptions were selected for updating."));
    }

    QList<SubItem> items;
    for (const SubItem& item : config.subscriptions) {
        if (normalizedIds.contains(item.id)) {
            items.append(item);
        }
    }

    if (items.isEmpty()) {
        return OperationResult::fail(QStringLiteral("The selected subscriptions could not be found."));
    }

    return updateInternal(config, items, useProxy);
}

OperationResult SubscriptionUpdateService::importFromText(Config& config, const QString& text)
{
    QList<VmessItem> servers = SubscriptionContentParser::parseMany(text);
    if (servers.isEmpty()) {
        servers = ShareUrlParser::parseMany(text);
    }

    if (servers.isEmpty()) {
        return OperationResult::fail(QStringLiteral("No supported share URL or subscription payload was detected."));
    }

    int maxSort = 0;
    for (const VmessItem& item : config.servers) {
        maxSort = std::max(maxSort, item.sort);
    }

    for (VmessItem& item : servers) {
        item.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.sort = maxSort + 10;
        maxSort = item.sort;
        config.servers.append(item);
    }

    if (config.currentIndexId.isEmpty() && !config.servers.isEmpty()) {
        config.currentIndexId = config.servers.constFirst().indexId;
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save imported servers."));
    }

    return OperationResult::ok(QStringLiteral("Imported %1 server(s) from text input.").arg(servers.size()));
}

OperationResult SubscriptionUpdateService::updateInternal(Config& config, const QList<SubItem>& items, bool useProxy)
{
    int updatedCount = 0;
    int importedCount = 0;
    QStringList successLines;
    QStringList failureLines;

    for (const SubItem& item : items) {
        // Honor cancellation requested by AppBootstrap::waitForBackgroundThreads()
        // at shutdown so we don't keep downloading subscriptions while the app
        // is trying to exit. Per-request timeouts already bound the in-flight
        // call below; this just stops scheduling new ones.
        QThread* const currentThread = QThread::currentThread();
        if (currentThread != nullptr && currentThread->isInterruptionRequested()) {
            failureLines.append(QStringLiteral("Subscription update cancelled by shutdown."));
            break;
        }

        int currentImportedCount = 0;
        const OperationResult result = updateSingle(config, item, &currentImportedCount, useProxy);
        if (result.success) {
            updatedCount++;
            importedCount += currentImportedCount;
            successLines.append(result.message);
            continue;
        }

        failureLines.append(result.message);
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
        return OperationResult::fail(QStringLiteral("%1 is disabled.").arg(displayName));
    }

    if (item.url.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("%1 has an empty URL.").arg(displayName));
    }

    QString content;
    const int proxyPort = useProxy ? config.localPort + 1 : 0;
    const OperationResult downloadResult = downloadText(item.url, item.userAgent, proxyPort, &content);
    if (!downloadResult.success) {
        return OperationResult::fail(QStringLiteral("%1 download failed: %2").arg(displayName, downloadResult.message));
    }

    const SubscriptionParseReport parseReport = SubscriptionContentParser::parseManyWithReport(content);
    const QList<VmessItem> servers = parseReport.items;
    if (servers.isEmpty()) {
        QStringList lines;
        lines.append(QStringLiteral("%1 returned no supported servers.").arg(displayName));
        lines.append(QStringLiteral("Download preview: %1").arg(summarizeContentPrefix(content)));
        for (const QString& note : parseReport.notes) {
            lines.append(note);
        }
        return OperationResult::fail(lines.join(QStringLiteral("\n")));
    }

    const int parsedCount = servers.size();
    const OperationResult replaceResult = subscriptionService_.replaceSubscriptionServers(config, item.id, servers);
    if (!replaceResult.success) {
        return OperationResult::fail(QStringLiteral("%1 save failed: %2").arg(displayName, replaceResult.message));
    }

    if (importedCount != nullptr) {
        *importedCount = 0;
        for (const VmessItem& server : config.servers) {
            if (server.subId == item.id) {
                (*importedCount)++;
            }
        }
    }

    const int finalImportedCount = importedCount == nullptr ? servers.size() : *importedCount;
    QStringList lines;
    lines.append(QStringLiteral("%1 imported %2 server(s).").arg(displayName).arg(finalImportedCount));
    lines.append(
        QStringLiteral("Subscription parse summary: parsed=%1 dedup-input=%2 saved=%3")
            .arg(parsedCount)
            .arg(subscriptionService_.lastReplaceInputCount())
            .arg(subscriptionService_.lastReplaceSavedCount()));
    if (subscriptionService_.lastReplaceSavedCount() < subscriptionService_.lastReplaceInputCount()) {
        lines.append(
            QStringLiteral("Subscription dedup removed %1 entrie(s).")
                .arg(subscriptionService_.lastReplaceInputCount() - subscriptionService_.lastReplaceSavedCount()));
    }
    for (const QString& note : parseReport.notes) {
        lines.append(note);
    }
    return OperationResult::ok(lines.join(QStringLiteral("\n")));
}

OperationResult SubscriptionUpdateService::downloadText(
    const QString& url,
    const QString& userAgent,
    int proxyPort,
    QString* content)
{
    if (content == nullptr) {
        return OperationResult::fail(QStringLiteral("Download target buffer is null."));
    }

    const QUrl parsedUrl = QUrl::fromUserInput(url.trimmed());
    if (!parsedUrl.isValid() || parsedUrl.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Subscription URL is invalid."));
    }

    QNetworkRequest request{parsedUrl};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    const QString defaultUA = QStringLiteral("nekobox/5.11.15 (Prefer ClashMeta Format)");
    request.setRawHeader("User-Agent", (userAgent.trimmed().isEmpty() ? defaultUA : userAgent).toUtf8());

    const QNetworkProxy previousProxy = networkAccessManager_.proxy();
    if (proxyPort > 0) {
        networkAccessManager_.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, kLoopbackAddress, proxyPort));
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    bool timedOut = false;
    QNetworkReply* reply = networkAccessManager_.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(kSubscriptionDownloadTimeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (timedOut) {
        networkAccessManager_.setProxy(previousProxy);
        reply->deleteLater();
        return OperationResult::fail(QStringLiteral("Subscription download timed out."));
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = reply->errorString();
        networkAccessManager_.setProxy(previousProxy);
        reply->deleteLater();
        return statusCode > 0
            ? OperationResult::fail(QStringLiteral("HTTP %1: %2").arg(statusCode).arg(error))
            : OperationResult::fail(error);
    }

    const QByteArray responseBody = reply->readAll();
    const QString contentType = QString::fromUtf8(reply->header(QNetworkRequest::ContentTypeHeader).toByteArray()).trimmed();
    *content = QString::fromUtf8(responseBody).trimmed();
    networkAccessManager_.setProxy(previousProxy);
    reply->deleteLater();

    if (statusCode >= 400) {
        return OperationResult::fail(QStringLiteral("HTTP %1").arg(statusCode));
    }

    if (content->isEmpty()) {
        return OperationResult::fail(QStringLiteral("Subscription response is empty."));
    }

    return OperationResult::ok(
        QStringLiteral("Subscription downloaded. HTTP %1, content-type=%2, bytes=%3, preview=%4")
            .arg(statusCode > 0 ? QString::number(statusCode) : QStringLiteral("200"))
            .arg(contentType.isEmpty() ? QStringLiteral("<empty>") : contentType)
            .arg(responseBody.size())
            .arg(summarizeContentPrefix(*content)));
}
