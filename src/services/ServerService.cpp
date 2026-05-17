#include "services/ServerService.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <algorithm>
#include <utility>

ServerService::ServerService(IConfigRepository& repository, QString customConfigDirectory)
    : repository_(repository)
    , customConfigDirectory_(std::move(customConfigDirectory))
{
}

QList<VmessItem> ServerService::list(const Config& config) const
{
    return config.servers;
}

OperationResult ServerService::addServer(Config& config, const VmessItem& item)
{
    const OperationResult validationResult = validateServer(item);
    if (!validationResult.success) {
        return validationResult;
    }

    VmessItem server = item;
    if (server.configType == ConfigType::Custom) {
        const OperationResult customResult = prepareCustomServer(server, nullptr);
        if (!customResult.success) {
            return customResult;
        }
    }
    server.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    server.remarks = server.remarks.trimmed().isEmpty()
        ? (server.configType == ConfigType::Custom
                ? QStringLiteral("import custom@%1").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")))
                : QStringLiteral("%1:%2").arg(server.address).arg(server.port))
        : server.remarks.trimmed();
    server.sort = nextSortValue(config);
    config.servers.append(server);

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
        config.servers.begin(),
        config.servers.end(),
        [&indexId](const VmessItem& existing) {
            return existing.indexId == indexId;
        });
    if (it == config.servers.end()) {
        return OperationResult::fail(QStringLiteral("The selected server does not exist."));
    }

    VmessItem updated = item;
    updated.indexId = it->indexId;
    updated.sort = it->sort;
    updated.subId = it->subId;
    updated.testResult = it->testResult;
    updated.preSocksPort = updated.configType == ConfigType::Custom
        ? item.preSocksPort
        : it->preSocksPort;
    if (updated.configType == ConfigType::Custom) {
        const OperationResult customResult = prepareCustomServer(updated, &(*it));
        if (!customResult.success) {
            return customResult;
        }
    }
    updated.remarks = updated.remarks.trimmed().isEmpty()
        ? (updated.configType == ConfigType::Custom
                ? QStringLiteral("import custom@%1").arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")))
                : QStringLiteral("%1:%2").arg(updated.address).arg(updated.port))
        : updated.remarks.trimmed();

    *it = updated;

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after editing the server."));
    }

    return OperationResult::ok(QStringLiteral("Server updated."));
}

OperationResult ServerService::duplicateServer(Config& config, const QString& indexId)
{
    if (indexId.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("No server selected for duplication."));
    }

    auto it = std::find_if(
        config.servers.begin(),
        config.servers.end(),
        [&indexId](const VmessItem& existing) {
            return existing.indexId == indexId;
        });
    if (it == config.servers.end()) {
        return OperationResult::fail(QStringLiteral("The selected server does not exist."));
    }

    VmessItem duplicated = *it;
    duplicated.indexId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    duplicated.subId.clear();
    duplicated.testResult.clear();

    const QString baseRemarks = duplicated.remarks.trimmed();
    duplicated.remarks = baseRemarks.isEmpty()
        ? QStringLiteral("Server copy")
        : QStringLiteral("%1 copy").arg(baseRemarks);

    if (duplicated.configType == ConfigType::Custom) {
        duplicated.address = resolveCustomConfigPath(it->address);
        const OperationResult customResult = prepareCustomServer(duplicated, nullptr);
        if (!customResult.success) {
            return customResult;
        }
    }

    const int insertIndex = static_cast<int>(std::distance(config.servers.begin(), it)) + 1;
    config.servers.insert(insertIndex, duplicated);
    normalizeSortValues(config.servers);

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after duplicating the server."));
    }

    return OperationResult::ok(QStringLiteral("Server duplicated."));
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

    for (const VmessItem& item : config.servers) {
        if (removedIds.contains(item.indexId) && item.configType == ConfigType::Custom && !item.address.trimmed().isEmpty()) {
            removedCustomAddresses.append(item.address);
        }
    }

    auto newEnd = std::remove_if(
        config.servers.begin(),
        config.servers.end(),
        [&removedIds](const VmessItem& item) {
            return removedIds.contains(item.indexId);
        });
    config.servers.erase(newEnd, config.servers.end());

    if (removedIds.contains(config.currentIndexId)) {
        config.currentIndexId = config.servers.isEmpty()
            ? QString()
            : config.servers.constFirst().indexId;
    }

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save configuration after removing server(s)."));
    }

    for (const QString& address : removedCustomAddresses) {
        removeManagedCustomConfig(address);
    }

    return OperationResult::ok(QStringLiteral("Server selection removed."));
}

OperationResult ServerService::moveServers(Config& config, const QList<QString>& indexIds, ServerMoveOperation operation)
{
    if (indexIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("No server was selected."));
    }

    if (config.servers.size() <= 1) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    QSet<QString> selectedIds;
    for (const QString& indexId : indexIds) {
        const QString trimmed = indexId.trimmed();
        if (!trimmed.isEmpty()) {
            selectedIds.insert(trimmed);
        }
    }
    if (selectedIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("No server was selected."));
    }

    bool moved = false;
    switch (operation) {
    case ServerMoveOperation::Up:
        for (int index = 1; index < config.servers.size(); ++index) {
            if (selectedIds.contains(config.servers.at(index).indexId)
                && !selectedIds.contains(config.servers.at(index - 1).indexId)) {
                config.servers.swapItemsAt(index - 1, index);
                moved = true;
            }
        }
        break;
    case ServerMoveOperation::Down:
        for (int index = config.servers.size() - 2; index >= 0; --index) {
            if (selectedIds.contains(config.servers.at(index).indexId)
                && !selectedIds.contains(config.servers.at(index + 1).indexId)) {
                config.servers.swapItemsAt(index, index + 1);
                moved = true;
            }
        }
        break;
    case ServerMoveOperation::Top:
    case ServerMoveOperation::Bottom: {
        QList<VmessItem> selectedItems;
        QList<VmessItem> remainingItems;
        selectedItems.reserve(config.servers.size());
        remainingItems.reserve(config.servers.size());
        for (const VmessItem& item : config.servers) {
            if (selectedIds.contains(item.indexId)) {
                selectedItems.append(item);
            } else {
                remainingItems.append(item);
            }
        }

        if (!selectedItems.isEmpty() && selectedItems.size() != config.servers.size()) {
            config.servers = operation == ServerMoveOperation::Top
                ? selectedItems + remainingItems
                : remainingItems + selectedItems;
            moved = true;
        }
        break;
    }
    }

    if (!moved) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    normalizeSortValues(config.servers);
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

    QStringList normalizedIds;
    for (const QString& indexId : orderedIndexIds) {
        const QString trimmed = indexId.trimmed();
        if (!trimmed.isEmpty() && !normalizedIds.contains(trimmed)) {
            normalizedIds.append(trimmed);
        }
    }
    if (normalizedIds.isEmpty()) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    QList<VmessItem> reordered;
    reordered.reserve(config.servers.size());

    for (const QString& indexId : normalizedIds) {
        const auto it = std::find_if(
            config.servers.cbegin(),
            config.servers.cend(),
            [&indexId](const VmessItem& item) {
                return item.indexId == indexId;
            });
        if (it != config.servers.cend()) {
            reordered.append(*it);
        }
    }

    for (const VmessItem& item : config.servers) {
        if (!normalizedIds.contains(item.indexId)) {
            reordered.append(item);
        }
    }

    if (reordered.size() != config.servers.size()) {
        return OperationResult::fail(QStringLiteral("Failed to rebuild the requested server order."));
    }

    if (std::equal(
            reordered.cbegin(),
            reordered.cend(),
            config.servers.cbegin(),
            config.servers.cend(),
            [](const VmessItem& left, const VmessItem& right) {
                return left.indexId == right.indexId;
            })) {
        return OperationResult::ok(QStringLiteral("Server order unchanged."));
    }

    config.servers = reordered;
    normalizeSortValues(config.servers);

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
        config.servers.cbegin(),
        config.servers.cend(),
        [&indexId](const VmessItem& item) {
            return item.indexId == indexId;
        });
    if (it == config.servers.cend()) {
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
        config.servers.begin(),
        config.servers.end(),
        [&trimmedId](const VmessItem& item) {
            return item.indexId == trimmedId;
        });
    if (it == config.servers.end()) {
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
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return QFileInfo(trimmed).absoluteFilePath();
    }

    if (!customConfigDirectory_.trimmed().isEmpty()) {
        const QString managedPath = QDir(customConfigDirectory_).filePath(trimmed);
        if (QFileInfo::exists(managedPath)) {
            return QFileInfo(managedPath).absoluteFilePath();
        }
    }

    return trimmed;
}

OperationResult ServerService::validateServer(const VmessItem& item)
{
    if (item.address.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Server address is required."));
    }

    if (item.configType == ConfigType::Custom) {
        if (item.preSocksPort < 0 || item.preSocksPort > 65535) {
            return OperationResult::fail(QStringLiteral("Pre-Socks port must be between 0 and 65535."));
        }
        return OperationResult::ok();
    }

    if (item.port <= 0 || item.port > 65535) {
        return OperationResult::fail(QStringLiteral("Server port must be between 1 and 65535."));
    }

    return OperationResult::ok();
}

void ServerService::normalizeSortValues(QList<VmessItem>& items)
{
    for (int index = 0; index < items.size(); ++index) {
        items[index].sort = (index + 1) * 10;
    }
}

int ServerService::nextSortValue(const Config& config)
{
    int maxSort = 0;
    for (const VmessItem& item : config.servers) {
        maxSort = std::max(maxSort, item.sort);
    }

    return maxSort + 10;
}

OperationResult ServerService::prepareCustomServer(VmessItem& server, const VmessItem* existing) const
{
    const QString sourcePath = resolveCustomConfigPath(server.address);
    if (sourcePath.trimmed().isEmpty() || !QFileInfo::exists(sourcePath)) {
        return OperationResult::fail(QStringLiteral("Custom config file does not exist."));
    }

    if (customConfigDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Custom config directory is unavailable."));
    }

    QDir customDirectory(customConfigDirectory_);
    if (!customDirectory.exists() && !QDir().mkpath(customDirectory.absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create custom config directory."));
    }

    const QString existingStoredPath = existing == nullptr ? QString() : resolveCustomConfigPath(existing->address);
    if (!existingStoredPath.isEmpty()
        && QFileInfo(existingStoredPath).absoluteFilePath().compare(QFileInfo(sourcePath).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        server.address = existing->address;
        return OperationResult::ok();
    }

    const QString extension = QFileInfo(sourcePath).suffix();
    const QString targetFileName = extension.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : QStringLiteral("%1.%2").arg(QUuid::createUuid().toString(QUuid::WithoutBraces), extension);
    const QString targetPath = customDirectory.filePath(targetFileName);

    if (QFileInfo::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    if (!QFile::copy(sourcePath, targetPath)) {
        return OperationResult::fail(QStringLiteral("Failed to copy custom config file into managed storage."));
    }

    if (!existingStoredPath.isEmpty() && isManagedCustomConfigPath(existingStoredPath) && QFileInfo::exists(existingStoredPath)) {
        QFile::remove(existingStoredPath);
    }

    server.address = targetFileName;
    server.port = 0;
    return OperationResult::ok();
}

bool ServerService::removeManagedCustomConfig(const QString& address) const
{
    const QString resolvedPath = resolveCustomConfigPath(address);
    if (!isManagedCustomConfigPath(resolvedPath) || !QFileInfo::exists(resolvedPath)) {
        return false;
    }

    return QFile::remove(resolvedPath);
}

bool ServerService::isManagedCustomConfigPath(const QString& filePath) const
{
    if (customConfigDirectory_.trimmed().isEmpty() || filePath.trimmed().isEmpty()) {
        return false;
    }

    const QString rootPath = QDir::cleanPath(QDir(customConfigDirectory_).absolutePath()).replace('\\', '/').toLower();
    const QString absoluteFilePath = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()).replace('\\', '/').toLower();
    return absoluteFilePath == rootPath
        || absoluteFilePath.startsWith(rootPath + QStringLiteral("/"));
}
