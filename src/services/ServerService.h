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

    // What removeServers() reports. Removing servers and deleting the managed custom config files
    // they used are two steps with two failure modes, so a single bool could not describe both:
    //
    //   applied -- the removal itself took effect and was persisted. False only when there was
    //              nothing to remove or the config could not be saved.
    //   result  -- what the user should be told. It is a failure when a managed config file
    //              survived the cleanup, even though `applied` is true in that case.
    //
    // Callers that act on "the server list changed" (stopping or reloading the core, refreshing the
    // window) must key off `applied`: keying off `result.success` skipped that work whenever only
    // the cleanup failed, which left a running core proxying through a server the user had just
    // deleted.
    struct RemovalOutcome {
        OperationResult result;
        bool applied = false;
    };

    QList<VmessItem> list(const Config& config) const;
    OperationResult addServer(Config& config, const VmessItem& item);
    OperationResult updateServer(Config& config, const QString& indexId, const VmessItem& item);
    RemovalOutcome removeServers(Config& config, const QList<QString>& indexIds);
    OperationResult moveServers(Config& config, const QList<QString>& indexIds, ServerMoveOperation operation);
    OperationResult reorderServers(Config& config, const QList<QString>& orderedIndexIds);
    OperationResult setDefaultServer(Config& config, const QString& indexId);
    OperationResult setTestResult(Config& config, const QString& indexId, const QString& result);
    OperationResult updateTestResult(Config& config, const QString& indexId, const QString& result);
    // Returns OperationResult like every other mutating method here: a bare bool
    // forced callers to invent their own message and drop the repository's
    // specific save failure reason.
    OperationResult save(Config& config);
    QString resolveCustomConfigPath(const QString& address) const;

private:
    static OperationResult validateServer(const VmessItem& item);

    IConfigRepository& repository_;
    ServerCustomConfigStore customConfigStore_;
};
