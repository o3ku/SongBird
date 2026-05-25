#include "ui/mainwindow/MainWindowConfigState.h"

#include "runtime/ProtocolCoreCompat.h"
#include "subscription/ShareUrlBuilder.h"

MainWindowConfigSnapshot makeMainWindowConfigSnapshot(const Config& config)
{
    return {
        config.currentIndexId,
        config.collection().subscriptions,
        config.collection().routingItems,
        config.collection().routingIndex,
        config.policy().coreTypeItems};
}

QHash<QString, QString> buildMainWindowShareUrlCache(const QList<VmessItem>& servers)
{
    QHash<QString, QString> shareUrlByIndexId;
    shareUrlByIndexId.reserve(servers.size());
    for (const VmessItem& item : servers) {
        const QString shareUrl = ShareUrlBuilder::build(item).trimmed();
        if (!item.indexId.isEmpty() && !shareUrl.isEmpty()) {
            shareUrlByIndexId.insert(item.indexId, shareUrl);
        }
    }
    return shareUrlByIndexId;
}

QString currentCoreDisplayNameFromConfig(const Config& config, const QString& currentIndexId)
{
    for (const VmessItem& item : config.collection().servers) {
        if (item.indexId == currentIndexId) {
            return coreTypeDisplayName(resolveSelectedCoreType(config, item, availableCoreTypes()));
        }
    }
    return QStringLiteral("Unknown");
}

QString subscriptionIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(QStringLiteral("sub:"))
        ? tabKey.mid(QStringLiteral("sub:").size())
        : QString();
}
