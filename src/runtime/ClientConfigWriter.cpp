#include "runtime/ClientConfigWriter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <utility>

#include "common/JsonFile.h"
#include "common/ProcessRunner.h"
#include "runtime/ProtocolConfigMapper.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/XrayTlsCompatibility.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"

namespace {

bool isValidJsonObjectText(const QString& value)
{
    if (value.trimmed().isEmpty()) {
        return true;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject();
}

} // namespace

ClientConfigWriter::ClientConfigWriter(QString customConfigDirectory)
    : customConfigDirectory_(std::move(customConfigDirectory))
{
}

void ClientConfigWriter::setExistingCoreTypes(const QList<CoreType>& existingCoreTypes)
{
    existingCoreTypes_ = existingCoreTypes;
}

void ClientConfigWriter::setRuntimeCoreInfo(const CoreInfo& coreInfo, const QString& version)
{
    const ICoreBackend* backend = coreInfo.type == CoreType::Unknown
        ? coreBackendForExecutableName(QFileInfo(coreInfo.program).fileName()) : coreBackend(coreInfo.type);
    runtimeCoreType_ = backend != nullptr ? backend->type() : CoreType::Unknown;
    runtimeCoreVersion_ = version.trimmed();
    if (runtimeCoreType_ != CoreType::Xray || !runtimeCoreVersion_.isEmpty()) {
        return;
    }
    ProcessRunner::Request request;
    request.program = coreInfo.program;
    request.arguments = backend->versionCommandArguments();
    request.workingDirectory = coreInfo.workingDirectory;
    const ProcessRunner::Outcome outcome = ProcessRunner::runToCompletion(request);
    if (outcome.completedNormally()) {
        runtimeCoreVersion_ = backend->extractVersionFromOutput(outcome.output);
    }
}

QList<CoreType> ClientConfigWriter::effectiveExistingCoreTypes() const
{
    // 已解析的启动核心优先作为自动选择结果，测速清空协议策略后也不能换成其他核心。
    if (runtimeCoreType_ != CoreType::Unknown) {
        return {runtimeCoreType_};
    }
    // When AppBootstrap has not pushed the installed-core snapshot yet (or in
    // tests that construct the writer directly), fall back to treating both
    // Xray and SingBox as available so behavior matches the pre-fix default.
    return existingCoreTypes_.isEmpty() ? availableCoreTypes() : existingCoreTypes_;
}

OperationResult ClientConfigWriter::writeClientConfig(
    const Config& config,
    const VmessItem& server,
    const QString& filePath) const
{
    return writeClientConfigs(config, server, filePath, nullptr);
}

OperationResult ClientConfigWriter::writeClientConfigs(
    const Config& config,
    const VmessItem& server,
    const QString& filePath,
    QStringList* auxiliaryPaths) const
{
    if (server.address.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Cannot generate config for an empty server address."));
    }

    if (filePath.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Runtime config path is empty."));
    }

    const OperationResult validationResult = validateServer(config, server);
    if (!validationResult.success) {
        return validationResult;
    }

    if (server.configType == ConfigType::Custom) {
        return writeCustomConfig(server, filePath);
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to create runtime config directory."));
    }

    const GeneratedConfigSet generated = generateClientConfigs(config, server);
    if (!generated.result.success) {
        return generated.result;
    }
    if (generated.primary.root.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("ClientConfigWriter", "%1 cannot generate a runtime config for %2 servers.")
                .arg(coreTypeDisplayName(resolveSelectedCoreType(config, server, effectiveExistingCoreTypes())))
                .arg(configTypeDisplayName(server.configType)));
    }

    const OperationResult primaryWriteResult = writeGeneratedConfig(generated.primary, filePath);
    if (!primaryWriteResult.success) {
        return primaryWriteResult;
    }

    if (auxiliaryPaths != nullptr) {
        auxiliaryPaths->clear();
    }

    const QDir outputDirectory = fileInfo.dir();
    for (const GeneratedConfig& auxiliaryConfig : generated.auxiliary) {
        const QString auxiliaryPath = outputDirectory.filePath(auxiliaryConfig.fileName);
        const OperationResult auxiliaryWriteResult = writeGeneratedConfig(auxiliaryConfig, auxiliaryPath);
        if (!auxiliaryWriteResult.success) {
            return auxiliaryWriteResult;
        }
        if (auxiliaryPaths != nullptr) {
            auxiliaryPaths->append(auxiliaryPath);
        }
    }

    return primaryWriteResult;
}

OperationResult ClientConfigWriter::writeCustomConfig(const VmessItem& server, const QString& filePath) const
{
    const QString sourcePath = resolveCustomConfigPath(server.address);
    if (sourcePath.trimmed().isEmpty() || !QFileInfo::exists(sourcePath)) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Custom config file does not exist."));
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to create runtime config directory."));
    }

    if (QFileInfo(sourcePath).absoluteFilePath().compare(fileInfo.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        return OperationResult::ok(QCoreApplication::translate("ClientConfigWriter", "Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
    }

    if (QFileInfo::exists(filePath) && !QFile::remove(filePath)) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to replace existing runtime custom config file."));
    }

    if (!QFile::copy(sourcePath, filePath)) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to copy custom config file to runtime path."));
    }

    return OperationResult::ok(QCoreApplication::translate("ClientConfigWriter", "Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

ClientConfigWriter::GeneratedConfigSet ClientConfigWriter::generateClientConfigs(
    const Config& config,
    const VmessItem& server) const
{
    GeneratedConfigSet generated;
    generated.primary.fileName = QStringLiteral("config.generated.json");
    generated.primary.root = buildRoot(config, server);
    if (!generated.primary.root.isEmpty() && runtimeCoreType_ == CoreType::Xray
        && resolveSelectedCoreType(config, server, effectiveExistingCoreTypes()) == CoreType::Xray) {
        generated.result = XrayTlsCompatibility::adaptConfig(
            generated.primary.root, runtimeCoreVersion_, QDateTime::currentDateTimeUtc());
        if (!generated.result.success) {
            generated.primary.root = {};
            return generated;
        }
    }

    const QList<CoreType> auxiliaryCoreTypes =
        resolveAuxiliaryTunCompatCoreTypes(config, server, effectiveExistingCoreTypes());
    for (const CoreType auxiliaryCoreType : auxiliaryCoreTypes) {
        // The local rewrite has not split upstream's non-legacy Xray TUN relay path yet.
        // Keep emitting the sing-box sidecar whenever Xray runs with TUN so the TUN adapter
        // can still be created instead of silently starting Xray alone with no TUN inbound.
        const ICoreBackend* backend = coreBackend(auxiliaryCoreType);
        if (backend == nullptr) {
            continue;
        }
        GeneratedConfig compat;
        compat.fileName = QStringLiteral("tun-singbox-compat.json");
        compat.root = backend->buildAuxiliaryTunClientRoot(config, AuxiliaryTunRouting::RelayToLocalProxy);
        if (compat.root.isEmpty()) {
            continue;
        }
        generated.auxiliary.append(compat);
    }

    return generated;
}

OperationResult ClientConfigWriter::validateServer(const Config& config, const VmessItem& server) const
{
    const CoreType runtimeCore = resolveSelectedCoreType(config, server, effectiveExistingCoreTypes());
    const QString transportSecurity = server.streamSecurity.trimmed();
    const bool realityTransport = ProtocolConfigMapper::isRealityTransport(transportSecurity);

    if (realityTransport) {
        if (server.publicKey.trimmed().isEmpty()) {
            return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Reality transport requires a public key."));
        }

        if (!ProtocolConfigMapper::isValidRealityShortId(server.shortId)) {
            return OperationResult::fail(
                QCoreApplication::translate("ClientConfigWriter", "Reality short ID must be a hexadecimal string with an even length up to 16 characters."));
        }
    }

    if (server.network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0
        && !server.extra.trimmed().isEmpty()) {
        if (!isValidJsonObjectText(server.extra)) {
            return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "XHTTP extra must be a valid JSON object."));
        }
    }

    if (!isValidJsonObjectText(server.finalmask)) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Finalmask must be a valid JSON object."));
    }

    if (server.configType == ConfigType::Custom) {
        return OperationResult::ok();
    }

    const ICoreBackend* backend = coreBackend(runtimeCore);
    return backend != nullptr
        ? backend->validateServer(server)
        : OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Unsupported core type: %1.").arg(coreTypeDisplayName(runtimeCore)));
}

OperationResult ClientConfigWriter::writeGeneratedConfig(const GeneratedConfig& generatedConfig, const QString& filePath) const
{
    const QJsonDocument document(generatedConfig.root);
    const JsonFile::WriteResult written = JsonFile::writeFileAtomically(
        filePath,
        document.toJson(QJsonDocument::Indented),
        QIODevice::Text);

    if (!written.ok) {
        switch (written.stage) {
        case JsonFile::WriteFailureStage::Open:
            return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to open runtime config file for writing."));
        case JsonFile::WriteFailureStage::Write:
            return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to write runtime config file."));
        case JsonFile::WriteFailureStage::Commit:
            return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to commit runtime config file."));
        case JsonFile::WriteFailureStage::None:
            break;
        }
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to write runtime config file."));
    }

    return OperationResult::ok(QCoreApplication::translate("ClientConfigWriter", "Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

QJsonObject ClientConfigWriter::buildRoot(const Config& config, const VmessItem& server) const
{
    const CoreType effectiveCore = resolveSelectedCoreType(config, server, effectiveExistingCoreTypes());
    const ICoreBackend* backend = coreBackend(effectiveCore);
    return backend != nullptr ? backend->buildClientRoot(config, server) : QJsonObject{};
}

QString ClientConfigWriter::resolveCustomConfigPath(const QString& address) const
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


ClientConfigWriter::GroupGeneration ClientConfigWriter::generateClientGroupConfigs(
    const Config& config,
    const QList<VmessItem>& members) const
{
    GroupGeneration generation;
    generation.generated.primary.fileName = QStringLiteral("config.generated.json");

    if (members.isEmpty()) {
        generation.generated.result = OperationResult::fail(
            QCoreApplication::translate("ClientConfigWriter", "Cannot generate a group config without members."));
        return generation;
    }

    // The anchor (first member) drives the launch core exactly as the single-node path does;
    // members that cannot run on it are dropped below rather than failing the group.
    const VmessItem& anchor = members.first();
    if (anchor.address.trimmed().isEmpty()) {
        generation.generated.result = OperationResult::fail(
            QCoreApplication::translate("ClientConfigWriter", "Cannot generate config for an empty server address."));
        return generation;
    }

    const CoreType launchCore = resolveSelectedCoreType(config, anchor, effectiveExistingCoreTypes());
    const ICoreBackend* backend = coreBackend(launchCore);
    if (backend == nullptr) {
        generation.generated.result = OperationResult::fail(
            QCoreApplication::translate("ClientConfigWriter", "Unsupported core type: %1.").arg(coreTypeDisplayName(launchCore)));
        return generation;
    }

    for (const VmessItem& member : members) {
        const bool embeddable = member.configType != ConfigType::Custom
            && !member.address.trimmed().isEmpty()
            && resolveSelectedCoreType(config, member, effectiveExistingCoreTypes()) == launchCore
            && validateServer(config, member).success;
        if (embeddable) {
            generation.embeddedMembers.append(member);
            continue;
        }
        generation.droppedMemberNames.append(member.remarks.trimmed().isEmpty()
            ? member.address : member.remarks);
    }

    if (generation.embeddedMembers.isEmpty()) {
        generation.generated.result = OperationResult::fail(
            QCoreApplication::translate("ClientConfigWriter", "No member of the group can run on %1.")
                .arg(coreTypeDisplayName(launchCore)));
        return generation;
    }

    generation.generated.primary.root = backend->buildClientGroupRoot(config, generation.embeddedMembers);
    if (generation.generated.primary.root.isEmpty()) {
        generation.generated.result = OperationResult::fail(
            QCoreApplication::translate("ClientConfigWriter", "%1 cannot generate an auto-select group for these servers.")
                .arg(coreTypeDisplayName(launchCore)));
        return generation;
    }

    // Group roots never run on Xray (Xray has no native auto-select group), so the
    // XrayTlsCompatibility adaptation of the single-node path does not apply here.
    const QList<CoreType> auxiliaryCoreTypes =
        resolveAuxiliaryTunCompatCoreTypes(config, generation.embeddedMembers.first(), effectiveExistingCoreTypes());
    for (const CoreType auxiliaryCoreType : auxiliaryCoreTypes) {
        // The local rewrite has not split upstream's non-legacy Xray TUN relay path yet.
        // Keep emitting the sing-box sidecar whenever Xray runs with TUN so the TUN adapter
        // can still be created instead of silently starting Xray alone with no TUN inbound.
        const ICoreBackend* auxiliaryBackend = coreBackend(auxiliaryCoreType);
        if (auxiliaryBackend == nullptr) {
            continue;
        }
        GeneratedConfig compat;
        compat.fileName = QStringLiteral("tun-singbox-compat.json");
        compat.root = auxiliaryBackend->buildAuxiliaryTunClientRoot(config, AuxiliaryTunRouting::RelayToLocalProxy);
        if (compat.root.isEmpty()) {
            continue;
        }
        generation.generated.auxiliary.append(compat);
    }

    return generation;
}

OperationResult ClientConfigWriter::writeClientGroupConfigs(
    const Config& config,
    const QList<VmessItem>& members,
    const QString& filePath,
    QStringList* auxiliaryPaths,
    QList<VmessItem>* embeddedMembers) const
{
    if (filePath.trimmed().isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Runtime config path is empty."));
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QCoreApplication::translate("ClientConfigWriter", "Failed to create runtime config directory."));
    }

    const GroupGeneration generation = generateClientGroupConfigs(config, members);
    if (!generation.generated.result.success) {
        return generation.generated.result;
    }

    const OperationResult primaryWriteResult = writeGeneratedConfig(generation.generated.primary, filePath);
    if (!primaryWriteResult.success) {
        return primaryWriteResult;
    }

    if (auxiliaryPaths != nullptr) {
        auxiliaryPaths->clear();
    }
    if (embeddedMembers != nullptr) {
        *embeddedMembers = generation.embeddedMembers;
    }

    const QDir outputDirectory = fileInfo.dir();
    for (const GeneratedConfig& auxiliaryConfig : generation.generated.auxiliary) {
        const QString auxiliaryPath = outputDirectory.filePath(auxiliaryConfig.fileName);
        const OperationResult auxiliaryWriteResult = writeGeneratedConfig(auxiliaryConfig, auxiliaryPath);
        if (!auxiliaryWriteResult.success) {
            return auxiliaryWriteResult;
        }
        if (auxiliaryPaths != nullptr) {
            auxiliaryPaths->append(auxiliaryPath);
        }
    }

    if (generation.droppedMemberNames.isEmpty()) {
        return primaryWriteResult;
    }
    return OperationResult::ok(QStringLiteral("%1 %2").arg(
        primaryWriteResult.message,
        QCoreApplication::translate("ClientConfigWriter", "Skipped %1 member(s) not supported by the core: %2.")
            .arg(QString::number(generation.droppedMemberNames.size()),
                 generation.droppedMemberNames.join(QStringLiteral(", ")))));
}
