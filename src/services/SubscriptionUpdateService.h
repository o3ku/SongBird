#pragma once

#include <QList>
#include <QString>
#include <QStringList>

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

    OperationResult updateAll(Config& config, bool useProxy = false);
    OperationResult updateByIds(Config& config, const QStringList& subscriptionIds, bool useProxy = false);
    OperationResult importFromText(Config& config, const QString& text);

private:
    OperationResult updateInternal(Config& config, const QList<SubItem>& items, bool useProxy);
    OperationResult updateSingle(Config& config, const SubItem& item, int* importedCount, bool useProxy);
    OperationResult downloadText(const QString& url, const QString& userAgent, int proxyPort, QString* content);

    IConfigRepository& repository_;
    SubscriptionService& subscriptionService_;
    QNetworkAccessManager& networkAccessManager_;
};
