#pragma once

#include <QDir>
#include <QList>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "persistence/IConfigRepository.h"

enum class ServerMoveOperation {
    Top = 0,
    Up,
    Down,
    Bottom
};

class ServerService {
public:
    ServerService(IConfigRepository& repository, QString customConfigDirectory = {});

    QList<VmessItem> list(const Config& config) const;
    OperationResult addServer(Config& config, const VmessItem& item);
    OperationResult updateServer(Config& config, const QString& indexId, const VmessItem& item);
    OperationResult duplicateServer(Config& config, const QString& indexId);
    OperationResult removeServers(Config& config, const QList<QString>& indexIds);
    OperationResult moveServers(Config& config, const QList<QString>& indexIds, ServerMoveOperation operation);
    OperationResult reorderServers(Config& config, const QList<QString>& orderedIndexIds);
    OperationResult setDefaultServer(Config& config, const QString& indexId);
    OperationResult setTestResult(Config& config, const QString& indexId, const QString& result);
    OperationResult updateTestResult(Config& config, const QString& indexId, const QString& result);
    bool save(Config& config);
    QString resolveCustomConfigPath(const QString& address) const;

private:
    static OperationResult validateServer(const VmessItem& item);
    static void normalizeSortValues(QList<VmessItem>& items);
    static int nextSortValue(const Config& config);
    OperationResult prepareCustomServer(VmessItem& server, const VmessItem* existing) const;
    bool removeManagedCustomConfig(const QString& address) const;
    bool isManagedCustomConfigPath(const QString& filePath) const;

    IConfigRepository& repository_;
    QString customConfigDirectory_;
};
