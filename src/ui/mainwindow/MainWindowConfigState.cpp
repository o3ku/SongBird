#include "ui/mainwindow/MainWindowConfigState.h"

#include "runtime/ProtocolCoreCompat.h"
#include "domain/models/RoutingProfiles.h"

MainWindowConfigSnapshot makeMainWindowConfigSnapshot(const Config& config)
{
    return {
        config.currentIndexId,
        config.collection().subscriptions,
        RoutingProfiles::routingItems(config.collection()),
        config.collection().routingModeId,
        config.policy().coreTypeItems};
}

QString currentCoreDisplayNameFromConfig(const Config& config, const QString& currentIndexId)
{
    for (const VmessItem& item : config.collection().servers) {
        if (item.indexId == currentIndexId) {
            return coreTypeDisplayName(resolveSelectedCoreType(config, item, availableCoreTypes()));
        }
    }
    return {};
}

QString subscriptionIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(QStringLiteral("sub:"))
        ? tabKey.mid(QStringLiteral("sub:").size())
        : QString();
}
