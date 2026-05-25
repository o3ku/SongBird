#include "app/AppRuntimeResolver.h"

#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "app/CoreDiscoveryService.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/ICoreBackend.h"

AppRuntimeResolver::AppRuntimeResolver(
    QString configPath,
    const Config& config,
    const QList<CoreType>& existingCoreTypes,
    const CoreDiscoveryService* coreDiscoveryService)
    : configPath_(std::move(configPath))
    , config_(config)
    , existingCoreTypes_(existingCoreTypes)
    , coreDiscoveryService_(coreDiscoveryService)
{
}

QString AppRuntimeResolver::resolveConfigPath() const
{
    return configPath_;
}

std::optional<VmessItem> AppRuntimeResolver::resolveActiveServer() const
{
    return resolveActiveServerSnapshot();
}

const VmessItem* AppRuntimeResolver::resolveActiveServerPointer() const
{
    const QString currentIndexId = config_.currentIndexId.trimmed();
    if (currentIndexId.isEmpty()) {
        return nullptr;
    }

    return findServerById(currentIndexId);
}

std::optional<VmessItem> AppRuntimeResolver::resolveActiveServerSnapshot() const
{
    const VmessItem* server = resolveActiveServerPointer();
    if (server == nullptr) {
        return std::nullopt;
    }

    return *server;
}

const VmessItem* AppRuntimeResolver::findServerById(const QString& indexId) const
{
    if (indexId.trimmed().isEmpty()) {
        return nullptr;
    }

    for (const VmessItem& item : config_.collection().servers) {
        if (item.indexId == indexId) {
            return &item;
        }
    }

    return nullptr;
}

std::optional<VmessItem> AppRuntimeResolver::findServerSnapshotById(const QString& indexId) const
{
    const VmessItem* server = findServerById(indexId);
    if (server == nullptr) {
        return std::nullopt;
    }

    return *server;
}

QString AppRuntimeResolver::resolveCustomConfigDirectory() const
{
    return QFileInfo(resolveConfigPath()).dir().filePath(QStringLiteral("guiConfigs"));
}

QString AppRuntimeResolver::resolveCustomConfigPath(const QString& address) const
{
    const QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    if (QFileInfo::exists(trimmed)) {
        return QFileInfo(trimmed).absoluteFilePath();
    }

    const QString managedPath = QDir(resolveCustomConfigDirectory()).filePath(trimmed);
    if (QFileInfo::exists(managedPath)) {
        return QFileInfo(managedPath).absoluteFilePath();
    }

    return trimmed;
}

QString AppRuntimeResolver::resolveRuntimeConfigPath(const VmessItem& server) const
{
    const QString runtimeDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime"));

    if (!QDir().mkpath(runtimeDirectory)) {
        return {};
    }

    QString extension = QStringLiteral("json");
    if (server.configType == ConfigType::Custom) {
        const QString sourcePath = resolveCustomConfigPath(server.address);
        const QString sourceExtension = QFileInfo(sourcePath).suffix().trimmed();
        if (!sourceExtension.isEmpty()) {
            extension = sourceExtension;
        }
    }
    return QDir(runtimeDirectory).filePath(QStringLiteral("config.generated.%1").arg(extension));
}

CoreType AppRuntimeResolver::resolveEffectiveCoreType(const VmessItem& server) const
{
    return resolveSelectedCoreType(config_, server, existingCoreTypes_);
}

QString AppRuntimeResolver::currentIndexId() const
{
    return config_.currentIndexId.trimmed();
}

CoreType AppRuntimeResolver::resolveRuntimeCoreType(CoreType coreType) const
{
    return ::resolveRuntimeCoreType(coreType);
}

CoreType AppRuntimeResolver::resolveLaunchCoreType(const VmessItem& server) const
{
    return resolveEffectiveCoreType(server);
}

CoreInfo AppRuntimeResolver::resolveCoreInfo(const VmessItem& server) const
{
    CoreInfo info;
    const CoreType launchCore = resolveLaunchCoreType(server);
    const ICoreBackend* backend = coreBackend(launchCore);
    if (backend == nullptr) {
        return {};
    }

    info.type = backend->type();
    info.arguments = backend->launchArguments(info.configPlaceholder);
    info.appendConfigArgument = backend->appendConfigArgument();
    const QString program = locateFirstExistingFile(resolveCoreCandidates(launchCore));

    if (program.isEmpty()) {
        return {};
    }

    info.program = program;
    info.workingDirectory = QFileInfo(program).absolutePath();

    return info;
}

QStringList AppRuntimeResolver::resolveCoreCandidates(CoreType coreType) const
{
    return coreDiscoveryService_ != nullptr
        ? coreDiscoveryService_->resolveCoreCandidates(coreType, resolveConfigPath())
        : QStringList{};
}

QString AppRuntimeResolver::locateFirstExistingFile(const QStringList& candidates) const
{
    return coreDiscoveryService_ != nullptr
        ? coreDiscoveryService_->locateFirstExistingFile(candidates)
        : QString{};
}

QList<CoreType> AppRuntimeResolver::detectExistingCoreTypes() const
{
    QList<CoreType> existingCoreTypes;
    for (const CoreType coreType : availableCoreTypes()) {
        if (!locateFirstExistingFile(resolveCoreCandidates(coreType)).isEmpty()) {
            existingCoreTypes.append(coreType);
        }
    }
    return existingCoreTypes;
}

QString AppRuntimeResolver::detectCoreVersion(CoreType coreType) const
{
    const QString program = locateFirstExistingFile(resolveCoreCandidates(coreType));
    return coreDiscoveryService_ != nullptr
        ? coreDiscoveryService_->detectCoreVersion(coreType, program)
        : QString{};
}

QString AppRuntimeResolver::resolveCoreInstallDirectory(CoreType coreType) const
{
    const QString existingCorePath = locateFirstExistingFile(resolveCoreCandidates(coreType));
    if (!existingCorePath.trimmed().isEmpty()) {
        return QFileInfo(existingCorePath).absolutePath();
    }

    const QString configDirectory = QFileInfo(resolveConfigPath()).dir().absolutePath();
    if (!configDirectory.trimmed().isEmpty()) {
        return configDirectory;
    }

    return QCoreApplication::applicationDirPath();
}
