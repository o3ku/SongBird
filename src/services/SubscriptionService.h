#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "common/OperationResult.h"
#include "common/SubscriptionOrder.h"
#include "domain/models/Config.h"
#include "domain/models/SubItem.h"
#include "domain/models/VmessItem.h"
#include "persistence/IConfigRepository.h"

class SubscriptionService {
public:
    explicit SubscriptionService(IConfigRepository& repository);

    QList<SubItem> list(const Config& config) const;
    OperationResult saveSubscriptions(Config& config, QList<SubItem> items);
    // `enabled` is read back by the settings dialog as a normal edit, so this usually writes the
    // file itself. `persistSave` is false for the one caller that hides a subscription as half of a
    // larger edit the caller is about to save anyway -- writing here too turned one click into two
    // writes of the same file, the second of them over a stale snapshot of the other fields.
    OperationResult setSubscriptionEnabled(
        Config& config,
        const QString& subscriptionId,
        bool enabled,
        bool persistSave = true);
    // Reorders the subscription list, which is also the order of the tabs in the main window: the
    // list is persisted as-is, so no separate order field has to be kept in step with it. An
    // operation that would not move anything reports success without saving.
    OperationResult moveSubscriptions(
        Config& config,
        const QList<QString>& subscriptionIds,
        SubscriptionOrder::MoveOperation operation);
    // Applies a complete order, which is what a tab drag produces: the bar already knows where the
    // tab was dropped, so there is no step to take. Ids the order does not mention keep their
    // relative position after the ones it does, because a hidden subscription has no tab and so
    // cannot be dragged -- dropping it would lose the subscription on the next save.
    OperationResult reorderSubscriptions(Config& config, const QList<QString>& orderedSubscriptionIds);
    OperationResult removeSubscription(Config& config, const QString& subscriptionId);
    OperationResult replaceSubscriptionServers(Config& config, const QString& subscriptionId, QList<VmessItem> items);

    static void normalizeSubscriptionIds(QList<SubItem>& items);
    static QString serverReuseKey(const VmessItem& item);

private:
    struct ReusableServerState {
        QString indexId;
        QString testResult;
        // Carried across a refresh beside testResult -- same node (reuse key matched), same
        // hand-made location/favorite assignment.
        QString locationOverride;
        bool favorite = false;
    };

    static QHash<QString, QList<ReusableServerState>> takeReusableServerStates(
        QList<VmessItem>& servers,
        const QString& subscriptionId,
        const QString& currentIndexId,
        bool* currentBelongedToUpdatedSubscription);
    static QString normalizedValue(const QString& value);

    IConfigRepository& repository_;
};
