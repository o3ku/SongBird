#pragma once

#include <QList>
#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "persistence/IConfigRepository.h"
#include "services/ServerCustomConfigStore.h"
#include "services/ServerListOperations.h"

class ServerService {
public:
    ServerService(IConfigRepository& repository, QString customConfigDirectory = {});

    QList<VmessItem> list(const Config& config) const;
    OperationResult addServer(Config& config, const VmessItem& item);
    OperationResult updateServer(Config& config, const QString& indexId, const VmessItem& item);
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

    IConfigRepository& repository_;
    ServerCustomConfigStore customConfigStore_;
};
