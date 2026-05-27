#include "services/SubscriptionService.h"

#include <algorithm>
#include <utility>

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
    return config.collection().subscriptions;
}

OperationResult SubscriptionService::saveSubscriptions(Config& config, QList<SubItem> items)
{
    normalizeSubscriptionIds(items);
    config.collection().subscriptions = std::move(items);

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
        config.collection().subscriptions.begin(),
        config.collection().subscriptions.end(),
        [&normalizedId](const SubItem& item) {
            return item.id.trimmed() == normalizedId;
        });
    if (it == config.collection().subscriptions.end()) {
        return OperationResult::fail(QStringLiteral("The selected subscription does not exist."));
    }

    it->enabled = enabled;
    if (!enabled && config.ui().mainSelectedSubId.trimmed() == normalizedId) {
        config.ui().mainSelectedSubId.clear();
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
        config.collection().subscriptions.cbegin(),
        config.collection().subscriptions.cend(),
        [&normalizedId](const SubItem& item) {
            return item.id.trimmed() == normalizedId;
        });
    if (!subscriptionExists) {
        return OperationResult::fail(QStringLiteral("The selected subscription does not exist."));
    }

    QSet<QString> removedServerIds;
    for (const VmessItem& item : config.collection().servers) {
        if (item.subId.trimmed() == normalizedId) {
            removedServerIds.insert(item.indexId);
        }
    }

    auto newSubEnd = std::remove_if(
        config.collection().subscriptions.begin(),
        config.collection().subscriptions.end(),
        [&normalizedId](const SubItem& item) {
            return item.id.trimmed() == normalizedId;
        });
    config.collection().subscriptions.erase(newSubEnd, config.collection().subscriptions.end());

    auto newServerEnd = std::remove_if(
        config.collection().servers.begin(),
        config.collection().servers.end(),
        [&normalizedId](const VmessItem& item) {
            return item.subId.trimmed() == normalizedId;
        });
    config.collection().servers.erase(newServerEnd, config.collection().servers.end());

    if (removedServerIds.contains(config.currentIndexId)) {
        config.currentIndexId = config.collection().servers.isEmpty()
            ? QString()
            : config.collection().servers.constFirst().indexId;
    }
    if (config.ui().mainSelectedSubId.trimmed() == normalizedId) {
        config.ui().mainSelectedSubId.clear();
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
    bool currentBelongedToUpdatedSubscription = false;
    QHash<QString, QList<ReusableServerState>> reusableServersByKey = takeReusableServerStates(
        config.collection().servers,
        normalizedId,
        previousCurrentIndexId,
        &currentBelongedToUpdatedSubscription);

    QStringList newSubscriptionIndexIds;
    newSubscriptionIndexIds.reserve(items.size());
    config.collection().servers.reserve(config.collection().servers.size() + items.size());
    for (VmessItem& item : items) {
        const QString reuseKey = serverReuseKey(item);
        QList<ReusableServerState>& reusableServers = reusableServersByKey[reuseKey];
        if (!reusableServers.isEmpty()) {
            const ReusableServerState reusableServer = reusableServers.takeFirst();
            item.indexId = reusableServer.indexId;
            item.testResult = reusableServer.testResult;
        } else {
            item.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            item.testResult.clear();
        }

        item.subId = normalizedId;
        newSubscriptionIndexIds.append(item.indexId);
        config.collection().servers.append(std::move(item));
    }

    if (!hasServerWithIndexId(config.collection().servers, previousCurrentIndexId)) {
        if (currentBelongedToUpdatedSubscription) {
            if (!newSubscriptionIndexIds.isEmpty()) {
                config.currentIndexId = newSubscriptionIndexIds.constFirst();
            } else if (!config.collection().servers.isEmpty()) {
                config.currentIndexId = config.collection().servers.constFirst().indexId;
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

QHash<QString, QList<SubscriptionService::ReusableServerState>> SubscriptionService::takeReusableServerStates(
    QList<VmessItem>& servers,
    const QString& subscriptionId,
    const QString& currentIndexId,
    bool* currentBelongedToUpdatedSubscription)
{
    if (currentBelongedToUpdatedSubscription != nullptr) {
        *currentBelongedToUpdatedSubscription = false;
    }

    QHash<QString, QList<ReusableServerState>> reusableServersByKey;
    auto newEnd = std::remove_if(
        servers.begin(),
        servers.end(),
        [&subscriptionId, &currentIndexId, currentBelongedToUpdatedSubscription, &reusableServersByKey](const VmessItem& item) {
            if (item.subId.trimmed() != subscriptionId) {
                return false;
            }

            if (currentBelongedToUpdatedSubscription != nullptr && item.indexId == currentIndexId) {
                *currentBelongedToUpdatedSubscription = true;
            }

            reusableServersByKey[serverReuseKey(item)].append(ReusableServerState{item.indexId, item.testResult});
            return true;
        });
    servers.erase(newEnd, servers.end());

    return reusableServersByKey;
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
        + normalizedValue(item.packetEncoding)
        + QLatin1Char('|')
        + normalizedValue(item.security)
        + QLatin1Char('|')
        + normalizedValue(item.idleSessionCheckInterval)
        + QLatin1Char('|')
        + normalizedValue(item.idleSessionTimeout)
        + QLatin1Char('|')
        + normalizedValue(item.minIdleSession);

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
