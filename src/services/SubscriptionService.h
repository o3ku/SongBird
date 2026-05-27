#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/SubItem.h"
#include "domain/models/VmessItem.h"
#include "persistence/IConfigRepository.h"

class SubscriptionService {
public:
    explicit SubscriptionService(IConfigRepository& repository);

    QList<SubItem> list(const Config& config) const;
    OperationResult saveSubscriptions(Config& config, QList<SubItem> items);
    OperationResult setSubscriptionEnabled(Config& config, const QString& subscriptionId, bool enabled);
    OperationResult removeSubscription(Config& config, const QString& subscriptionId);
    OperationResult replaceSubscriptionServers(Config& config, const QString& subscriptionId, QList<VmessItem> items);

    static void normalizeSubscriptionIds(QList<SubItem>& items);

private:
    struct ReusableServerState {
        QString indexId;
        QString testResult;
    };

    static QHash<QString, QList<ReusableServerState>> takeReusableServerStates(
        QList<VmessItem>& servers,
        const QString& subscriptionId,
        const QString& currentIndexId,
        bool* currentBelongedToUpdatedSubscription);
    static QString serverReuseKey(const VmessItem& item);
    static QString normalizedValue(const QString& value);

    IConfigRepository& repository_;
};
