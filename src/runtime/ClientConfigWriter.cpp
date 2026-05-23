#include "runtime/ClientConfigWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <utility>

#include "runtime/ProtocolConfigMapper.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"
#include "runtime/core/singbox/SingBoxCoreBackend.h"

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

QList<CoreType> ClientConfigWriter::effectiveExistingCoreTypes() const
{
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
        return OperationResult::fail(QStringLiteral("Cannot generate config for an empty server address."));
    }

    if (filePath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Runtime config path is empty."));
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
        return OperationResult::fail(QStringLiteral("Failed to create runtime config directory."));
    }

    const GeneratedConfigSet generated = generateClientConfigs(config, server);
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
        return OperationResult::fail(QStringLiteral("Custom config file does not exist."));
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create runtime config directory."));
    }

    if (QFileInfo(sourcePath).absoluteFilePath().compare(fileInfo.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        return OperationResult::ok(QStringLiteral("Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
    }

    if (QFileInfo::exists(filePath) && !QFile::remove(filePath)) {
        return OperationResult::fail(QStringLiteral("Failed to replace existing runtime custom config file."));
    }

    if (!QFile::copy(sourcePath, filePath)) {
        return OperationResult::fail(QStringLiteral("Failed to copy custom config file to runtime path."));
    }

    return OperationResult::ok(QStringLiteral("Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

ClientConfigWriter::GeneratedConfigSet ClientConfigWriter::generateClientConfigs(
    const Config& config,
    const VmessItem& server) const
{
    GeneratedConfigSet generated;
    generated.primary.fileName = QStringLiteral("config.generated.json");
    generated.primary.root = buildRoot(config, server);

    if (requiresTunCompatSingBoxCore(config, server, effectiveExistingCoreTypes())) {
        // The local rewrite has not split upstream's non-legacy Xray TUN relay path yet.
        // Keep emitting the sing-box sidecar whenever Xray runs with TUN so `singbox_tun`
        // can still be created instead of silently starting Xray alone with no TUN inbound.
        GeneratedConfig compat;
        compat.fileName = QStringLiteral("tun-singbox-compat.json");
        compat.root = SingBoxCoreBackend::buildTunCompatClientRoot(config);
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
            return OperationResult::fail(QStringLiteral("Reality transport requires a public key."));
        }

        if (!ProtocolConfigMapper::isValidRealityShortId(server.shortId)) {
            return OperationResult::fail(
                QStringLiteral("Reality short ID must be a hexadecimal string with an even length up to 16 characters."));
        }
    }

    if (server.network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0
        && !server.extra.trimmed().isEmpty()) {
        if (!isValidJsonObjectText(server.extra)) {
            return OperationResult::fail(QStringLiteral("XHTTP extra must be a valid JSON object."));
        }
    }

    if (!isValidJsonObjectText(server.finalmask)) {
        return OperationResult::fail(QStringLiteral("Finalmask must be a valid JSON object."));
    }

    const ICoreBackend* backend = coreBackend(runtimeCore);
    return backend != nullptr
        ? backend->validateServer(server)
        : OperationResult::fail(QStringLiteral("Unsupported core type: %1.").arg(coreTypeDisplayName(runtimeCore)));
}

OperationResult ClientConfigWriter::writeGeneratedConfig(const GeneratedConfig& generatedConfig, const QString& filePath) const
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to open runtime config file for writing."));
    }

    const QJsonDocument document(generatedConfig.root);
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        return OperationResult::fail(QStringLiteral("Failed to write runtime config file."));
    }

    if (!file.commit()) {
        return OperationResult::fail(QStringLiteral("Failed to commit runtime config file."));
    }

    return OperationResult::ok(QStringLiteral("Runtime config generated: %1").arg(QDir::toNativeSeparators(filePath)));
}

QJsonObject ClientConfigWriter::buildRoot(const Config& config, const VmessItem& server) const
{
    const bool requiresSingBox = server.configType == ConfigType::AnyTLS
        || server.configType == ConfigType::Naive;
    const CoreType effectiveCore = resolveSelectedCoreType(config, server, effectiveExistingCoreTypes());
    const CoreType rootCore = requiresSingBox ? CoreType::SingBox : effectiveCore;
    const ICoreBackend* backend = coreBackend(rootCore);
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

