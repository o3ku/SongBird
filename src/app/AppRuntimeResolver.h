#pragma once

#include <optional>

#include <QList>
#include <QString>
#include <QStringList>

#include "app/ProxyRuntimeInterfaces.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"

class CoreDiscoveryService;

class AppRuntimeResolver final : public IRuntimeProfileResolver
{
public:
    AppRuntimeResolver(
        QString configPath,
        const Config& config,
        const QList<CoreType>& existingCoreTypes,
        const CoreDiscoveryService* coreDiscoveryService);

    QString resolveConfigPath() const;
    std::optional<VmessItem> resolveActiveServer() const override;
    const VmessItem* resolveActiveServerPointer() const;
    std::optional<VmessItem> resolveActiveServerSnapshot() const;
    const VmessItem* findServerById(const QString& indexId) const;
    std::optional<VmessItem> findServerSnapshotById(const QString& indexId) const;
    QString resolveCustomConfigDirectory() const;
    QString resolveCustomConfigPath(const QString& address) const;
    QString resolveRuntimeConfigPath(const VmessItem& server) const override;
    CoreType resolveEffectiveCoreType(const VmessItem& server) const;
    CoreType resolveLaunchCoreType(const VmessItem& server) const override;
    CoreInfo resolveCoreInfo(const VmessItem& server) const override;
    QStringList resolveCoreCandidates(CoreType coreType) const override;
    QString locateFirstExistingFile(const QStringList& candidates) const override;
    QString currentIndexId() const override;
    CoreType resolveRuntimeCoreType(CoreType coreType) const override;
    QList<CoreType> detectExistingCoreTypes() const;
    QString detectCoreVersion(CoreType coreType) const;
    QString resolveCoreInstallDirectory(CoreType coreType) const override;

private:
    QString configPath_;
    const Config& config_;
    const QList<CoreType>& existingCoreTypes_;
    const CoreDiscoveryService* coreDiscoveryService_ = nullptr;
};
