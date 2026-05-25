#include "services/ServerService.h"

#include <QDate>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <algorithm>
#include <utility>

#include "common/PortValidator.h"

namespace {

QString normalizedServerRemarks(const VmessItem& server)
{
    const QString trimmedRemarks = server.remarks.trimmed();
    if (!trimmedRemarks.isEmpty()) {
        return trimmedRemarks;
    }

    if (server.configType == ConfigType::Custom) {
        return QStringLiteral("import custom@%1").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
    }

    return QStringLiteral("%1:%2").arg(server.address).arg(server.port);
}

} // namespace

ServerService::ServerService(IConfigRepository& repository, QString customConfigDirectory)
    : repository_(repository)
    , customConfigStore_(std::move(customConfigDirectory))
{
}

QList<VmessItem> ServerService::list(const Config& config) const
{
    return config.collection().servers;
}

OperationResult ServerService::addServer(Config& config, const VmessItem& item)
{
    const OperationResult validationResult = validateServer(item);
    if (!validationResult.success) {
        return validationResult;
    }

    VmessItem server = item;
    if (server.configType == ConfigType::Custom) {
        const OperationResult customResult = customConfigStore_.prepareServer(server, nullptr);
        if (!customResult.success) {
            return customResult;
        }
    }
    server.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    server.remarks = normalizedServerRemarks(server);
    config.collection().servers.append(server);

    if (config.currentIndexId.trimmed().isEmpty()) {
        config.currentIndexId = server.indexId;
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after adding the server."));
    }

    return OperationResult::ok(QStringLiteral("Server added."));
}

OperationResult ServerService::updateServer(Config& config, const QString& indexId, const VmessItem& item)
{
    if (indexId.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("No server selected for editing."));
    }

    const OperationResult validationResult = validateServer(item);
    if (!validationResult.success) {
        return validationResult;
    }

    auto it = std::find_if(
        config.collection().servers.begin(),
        config.collection().servers.end(),
        [&indexId](const VmessItem& existing) {
            return existing.indexId == indexId;
        });
    if (it == config.collection().servers.end()) {
        return OperationResult::fail(QStringLiteral("The selected server does not exist."));
    }

    VmessItem updated = item;
    updated.indexId = it->indexId;
    updated.subId = it->subId;
    updated.testResult = it->testResult;
    updated.preSocksPort = updated.configType == ConfigType::Custom
        ? item.preSocksPort
        : it->preSocksPort;
    if (updated.configType == ConfigType::Custom) {
        const OperationResult customResult = customConfigStore_.prepareServer(updated, &(*it));
        if (!customResult.success) {
            return customResult;
        }
    }
    updated.remarks = normalizedServerRemarks(updated);

    *it = updated;

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after editing the server."));
    }

    return OperationResult::ok(QStringLiteral("Server updated."));
}

OperationResult ServerService::removeServers(Config& config, const QList<QString>& indexIds)
{
    if (indexIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("No server was selected."));
    }

    QSet<QString> removedIds;
    QStringList removedCustomAddresses;
    for (const QString& indexId : indexIds) {
        removedIds.insert(indexId);
    }

    for (const VmessItem& item : config.collection().servers) {
        if (removedIds.contains(item.indexId) && item.configType == ConfigType::Custom && !item.address.trimmed().isEmpty()) {
            removedCustomAddresses.append(item.address);
        }
    }

    auto newEnd = std::remove_if(
        config.collection().servers.begin(),
        config.collection().servers.end(),
        [&removedIds](const VmessItem& item) {
            return removedIds.contains(item.indexId);
        });
    config.collection().servers.erase(newEnd, config.collection().servers.end());

    if (removedIds.contains(config.currentIndexId)) {
        config.currentIndexId = config.collection().servers.isEmpty()
            ? QString()
            : config.collection().servers.constFirst().indexId;
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after removing server(s)."));
    }

    for (const QString& address : removedCustomAddresses) {
        customConfigStore_.removeManagedConfig(address);
    }

    return OperationResult::ok(QStringLiteral("Server selection removed."));
}

OperationResult ServerService::moveServers(Config& config, const QList<QString>& indexIds, ServerMoveOperation operation)
{
    if (indexIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("No server was selected."));
    }

    if (config.collection().servers.size() <= 1) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    const QStringList selectedIds = ServerListOperations::normalizedIndexIds(indexIds);
    if (selectedIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("No server was selected."));
    }

    if (!ServerListOperations::moveServers(config.collection().servers, selectedIds, operation)) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after reordering server(s)."));
    }

    return OperationResult::ok(QStringLiteral("Server order updated."));
}

OperationResult ServerService::reorderServers(Config& config, const QList<QString>& orderedIndexIds)
{
    if (orderedIndexIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    const QStringList normalizedIds = ServerListOperations::normalizedIndexIds(orderedIndexIds);
    if (normalizedIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    const ServerListOperations::ReorderResult reorderResult =
        ServerListOperations::reorderServers(config.collection().servers, normalizedIds);
    if (reorderResult == ServerListOperations::ReorderResult::Invalid) {
        return OperationResult::fail(QStringLiteral("Failed to rebuild the requested server order."));
    }

    if (reorderResult == ServerListOperations::ReorderResult::Unchanged) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after drag reordering server(s)."));
    }

    return OperationResult::ok(QStringLiteral("Server order updated."));
}

OperationResult ServerService::setDefaultServer(Config& config, const QString& indexId)
{
    if (indexId.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("No server selected."));
    }

    const auto it = std::find_if(
        config.collection().servers.cbegin(),
        config.collection().servers.cend(),
        [&indexId](const VmessItem& item) {
            return item.indexId == indexId;
        });
    if (it == config.collection().servers.cend()) {
        return OperationResult::fail(QStringLiteral("The selected server does not exist."));
    }

    config.currentIndexId = indexId;
    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after switching the default server."));
    }

    return OperationResult::ok(QStringLiteral("Default server updated."));
}

OperationResult ServerService::setTestResult(Config& config, const QString& indexId, const QString& result)
{
    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty()) {
        return OperationResult::fail(QStringLiteral("No server selected for test result update."));
    }

    auto it = std::find_if(
        config.collection().servers.begin(),
        config.collection().servers.end(),
        [&trimmedId](const VmessItem& item) {
            return item.indexId == trimmedId;
        });
    if (it == config.collection().servers.end()) {
        return OperationResult::fail(QStringLiteral("The selected server does not exist."));
    }

    it->testResult = result.trimmed();
    return OperationResult::ok();
}

OperationResult ServerService::updateTestResult(Config& config, const QString& indexId, const QString& result)
{
    const OperationResult setResult = setTestResult(config, indexId, result);
    if (!setResult.success) {
        return setResult;
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after updating the test result."));
    }

    return OperationResult::ok();
}

bool ServerService::save(Config& config)
{
    return repository_.save(config);
}

QString ServerService::resolveCustomConfigPath(const QString& address) const
{
    return customConfigStore_.resolveConfigPath(address);
}

OperationResult ServerService::validateServer(const VmessItem& item)
{
    if (item.address.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Server address is required."));
    }

    if (item.configType == ConfigType::Custom) {
        if (!isValidOptionalTcpPort(item.preSocksPort)) {
            return OperationResult::fail(QStringLiteral("Pre-Socks port must be between 0 and 65535."));
        }
        return OperationResult::ok();
    }

    if (!isValidTcpPort(item.port)) {
        return OperationResult::fail(QStringLiteral("Server port must be between 1 and 65535."));
    }

    return OperationResult::ok();
}
