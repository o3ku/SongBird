#include "services/SubscriptionService.h"

#include <algorithm>

#include <QHash>
#include <QSet>
#include <QUuid>

namespace {
bool hasServerWithIndexId(const QList<VmessItem>& servers, const QString& indexId)
{
    if (indexId.trimmed().isEmpty()) {
        return false;
    }

    return std::any_of(
        servers.cbegin(),
        servers.cend(),
        [&indexId](const VmessItem& item) {
            return item.indexId == indexId;
        });
}
}

SubscriptionService::SubscriptionService(IConfigRepository& repository)
    : repository_(repository)
{
}

QList<SubItem> SubscriptionService::list(const Config& config) const
{
    return config.subscriptions;
}

OperationResult SubscriptionService::saveSubscriptions(Config& config, QList<SubItem> items)
{
    normalizeSubscriptionIds(items);
    config.subscriptions = std::move(items);

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save subscriptions."));
    }

    return OperationResult::ok(QStringLiteral("Subscriptions saved."));
}

OperationResult SubscriptionService::setSubscriptionEnabled(Config& config, const QString& subscriptionId, bool enabled)
{
    const QString normalizedId = subscriptionId.trimmed();
    if (normalizedId.isEmpty()) {
        return OperationResult::fail(QStringLiteral("Subscription id is required."));
    }

    auto it = std::find_if(
        config.subscriptions.begin(),
        config.subscriptions.end(),
        [&normalizedId](const SubItem& item) {
            return item.id.trimmed() == normalizedId;
        });
    if (it == config.subscriptions.end()) {
        return OperationResult::fail(QStringLiteral("The selected subscription does not exist."));
    }

    it->enabled = enabled;
    if (!enabled && config.mainSelectedSubId.trimmed() == normalizedId) {
        config.mainSelectedSubId.clear();
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save subscriptions."));
    }

    return OperationResult::ok(enabled
            ? QStringLiteral("Subscription enabled.")
            : QStringLiteral("Subscription hidden."));
}

OperationResult SubscriptionService::removeSubscription(Config& config, const QString& subscriptionId)
{
    const QString normalizedId = subscriptionId.trimmed();
    if (normalizedId.isEmpty()) {
        return OperationResult::fail(QStringLiteral("Subscription id is required."));
    }

    const bool subscriptionExists = std::any_of(
        config.subscriptions.cbegin(),
        config.subscriptions.cend(),
        [&normalizedId](const SubItem& item) {
            return item.id.trimmed() == normalizedId;
        });
    if (!subscriptionExists) {
        return OperationResult::fail(QStringLiteral("The selected subscription does not exist."));
    }

    QSet<QString> removedServerIds;
    for (const VmessItem& item : config.servers) {
        if (item.subId.trimmed() == normalizedId) {
            removedServerIds.insert(item.indexId);
        }
    }

    auto newSubEnd = std::remove_if(
        config.subscriptions.begin(),
        config.subscriptions.end(),
        [&normalizedId](const SubItem& item) {
            return item.id.trimmed() == normalizedId;
        });
    config.subscriptions.erase(newSubEnd, config.subscriptions.end());

    auto newServerEnd = std::remove_if(
        config.servers.begin(),
        config.servers.end(),
        [&normalizedId](const VmessItem& item) {
            return item.subId.trimmed() == normalizedId;
        });
    config.servers.erase(newServerEnd, config.servers.end());

    if (removedServerIds.contains(config.currentIndexId)) {
        config.currentIndexId = config.servers.isEmpty()
            ? QString()
            : config.servers.constFirst().indexId;
    }
    if (config.mainSelectedSubId.trimmed() == normalizedId) {
        config.mainSelectedSubId.clear();
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save subscriptions."));
    }

    return OperationResult::ok(QStringLiteral("Subscription deleted."));
}

OperationResult SubscriptionService::replaceSubscriptionServers(
    Config& config,
    const QString& subscriptionId,
    QList<VmessItem> items)
{
    const QString normalizedId = subscriptionId.trimmed();
    if (normalizedId.isEmpty()) {
        return OperationResult::fail(QStringLiteral("Subscription id is required."));
    }

    const QString previousCurrentIndexId = config.currentIndexId;
    QList<VmessItem> oldSubscriptionServers;
    auto newEnd = std::remove_if(
        config.servers.begin(),
        config.servers.end(),
        [&normalizedId, &oldSubscriptionServers](const VmessItem& item) {
            if (item.subId.trimmed() == normalizedId) {
                oldSubscriptionServers.append(item);
                return true;
            }

            return false;
        });
    config.servers.erase(newEnd, config.servers.end());

    QHash<QString, QList<VmessItem>> reusableServersByKey;
    for (const VmessItem& oldItem : oldSubscriptionServers) {
        reusableServersByKey[serverReuseKey(oldItem)].append(oldItem);
    }

    QStringList newSubscriptionIndexIds;
    for (VmessItem& item : items) {
        const QString reuseKey = serverReuseKey(item);
        QList<VmessItem>& reusableServers = reusableServersByKey[reuseKey];
        if (!reusableServers.isEmpty()) {
            item.indexId = reusableServers.takeFirst().indexId;
        } else {
            item.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        item.subId = normalizedId;
        config.servers.append(item);
        newSubscriptionIndexIds.append(item.indexId);
    }

    if (!hasServerWithIndexId(config.servers, previousCurrentIndexId)) {
        const bool currentBelongedToUpdatedSubscription = std::any_of(
            oldSubscriptionServers.cbegin(),
            oldSubscriptionServers.cend(),
            [&previousCurrentIndexId](const VmessItem& item) {
                return item.indexId == previousCurrentIndexId;
            });
        if (currentBelongedToUpdatedSubscription) {
            if (!newSubscriptionIndexIds.isEmpty()) {
                config.currentIndexId = newSubscriptionIndexIds.constFirst();
            } else if (!config.servers.isEmpty()) {
                config.currentIndexId = config.servers.constFirst().indexId;
            } else {
                config.currentIndexId.clear();
            }
        } else if (previousCurrentIndexId.trimmed().isEmpty()) {
            config.currentIndexId.clear();
        } else {
            config.currentIndexId = previousCurrentIndexId;
        }
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to replace subscription servers."));
    }

    return OperationResult::ok(QStringLiteral("Subscription servers replaced."));
}

void SubscriptionService::normalizeSubscriptionIds(QList<SubItem>& items)
{
    for (SubItem& item : items) {
        if (item.id.trimmed().isEmpty()) {
            item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
    }
}

QString SubscriptionService::serverReuseKey(const VmessItem& item)
{
    const QString baseKey = QString::number(static_cast<int>(item.configType))
        + QLatin1Char('|')
        + normalizedValue(item.address)
        + QLatin1Char('|')
        + QString::number(item.port)
        + QLatin1Char('|')
        + normalizedValue(item.network)
        + QLatin1Char('|')
        + normalizedValue(item.headerType)
        + QLatin1Char('|')
        + normalizedValue(item.requestHost)
        + QLatin1Char('|')
        + normalizedValue(item.path)
        + QLatin1Char('|')
        + normalizedValue(item.streamSecurity)
        + QLatin1Char('|')
        + normalizedValue(item.sni)
        + QLatin1Char('|')
        + normalizedValue(item.flow)
        + QLatin1Char('|')
        + normalizedValue(item.security);

    switch (item.configType) {
    case ConfigType::Shadowsocks:
        return baseKey + QLatin1Char('|') + normalizedValue(item.id);
    case ConfigType::Naive:
        return baseKey + QLatin1Char('|') + normalizedValue(item.username)
            + QLatin1Char('|') + normalizedValue(item.id);
    case ConfigType::Socks:
    case ConfigType::HTTP:
        return baseKey + QLatin1Char('|') + normalizedValue(item.id)
            + QLatin1Char('|') + normalizedValue(item.security);
    case ConfigType::VMess:
    case ConfigType::VLESS:
    case ConfigType::Trojan:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::AnyTLS:
        return baseKey + QLatin1Char('|') + normalizedValue(item.id);
    case ConfigType::Custom:
    case ConfigType::WireGuard:
    case ConfigType::Unknown:
    default:
        return baseKey + QLatin1Char('|') + normalizedValue(item.id);
    }
}

QString SubscriptionService::normalizedValue(const QString& value)
{
    return value.trimmed().toLower();
}
