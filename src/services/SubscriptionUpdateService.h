#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "persistence/IConfigRepository.h"
#include "services/SubscriptionService.h"

class QNetworkAccessManager;

class SubscriptionUpdateService {
public:
    SubscriptionUpdateService(
        IConfigRepository& repository,
        SubscriptionService& subscriptionService,
        QNetworkAccessManager& networkAccessManager);

    using ProgressCallback = std::function<void(int current, int total, const QString& message)>;

    OperationResult updateAll(Config& config, bool useProxy = false);
    OperationResult updateByIds(Config& config, const QStringList& subscriptionIds, bool useProxy = false);
    OperationResult updateAllWithProgress(Config& config, bool useProxy, const ProgressCallback& progressCallback);
    OperationResult importFromText(Config& config, const QString& text);


    // When set to a non-empty directory, every successfully downloaded subscription
    // payload is archived there verbatim (latest copy per subscription, overwritten
    // in place) before parsing. The parsed server list in the config is a lossy
    // projection; this keeps the original bytes recoverable. Empty (the default)
    // disables archiving.
    void setRawBackupDirectory(const QString& directoryPath);
private:
    OperationResult updateInternal(Config& config, const QList<SubItem>& items, bool useProxy, const ProgressCallback& progressCallback = {});
    OperationResult updateSingle(Config& config, const SubItem& item, int* importedCount, bool useProxy);
    OperationResult downloadText(
        const QString& url,
        const QString& userAgent,
        int proxyPort,
        const QString& ifNoneMatch,
        const QString& ifModifiedSince,
        QString* content,
        QString* outEtag,
        QString* outLastModified,
        bool* outNotModified);

    IConfigRepository& repository_;
    SubscriptionService& subscriptionService_;
    QNetworkAccessManager& networkAccessManager_;
    QString rawBackupDirectory_;
};
