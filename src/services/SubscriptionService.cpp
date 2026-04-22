#include "services/SubscriptionService.h"

#include <algorithm>

#include <QSet>
#include <QUuid>

namespace {
QList<VmessItem> deduplicateSubscriptionServersByUuid(const QList<VmessItem>& items)
{
    QList<VmessItem> deduplicated;
    QSet<QString> seenKeys;

    for (const VmessItem& item : items) {
        const QString uuid = item.id.trimmed().toLower();
        const QString dedupKey = uuid.isEmpty()
            ? (item.address.trimmed().toLower() + QLatin1Char(':') + QString::number(item.port))
            : (uuid + QLatin1Char('@') + item.address.trimmed().toLower() + QLatin1Char(':') + QString::number(item.port));

        if (seenKeys.contains(dedupKey)) {
            continue;
        }

        seenKeys.insert(dedupKey);
        deduplicated.append(item);
    }

    return deduplicated;
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
    if (subscriptionId.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Subscription id is required."));
    }

    auto newEnd = std::remove_if(
        config.servers.begin(),
        config.servers.end(),
        [&subscriptionId](const VmessItem& item) {
            return item.subId == subscriptionId;
        });
    config.servers.erase(newEnd, config.servers.end());

    items = deduplicateSubscriptionServersByUuid(items);
    for (VmessItem& item : items) {
        item.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.subId = subscriptionId;
        config.servers.append(item);
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
