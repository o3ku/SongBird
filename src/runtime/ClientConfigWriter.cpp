#include "runtime/ClientConfigWriter.h"

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

#include <utility>

#include "runtime/DnsConfigFragments.h"
#include "runtime/ProtocolConfigMapper.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/RoutingConfigFragments.h"
#include "runtime/TunCompatCoreRequirement.h"
#include "runtime/core/singbox/SingBoxConfigFragments.h"
#include "runtime/core/xray/XrayConfigFragments.h"

namespace {

const QString kLoopbackAddress = QStringLiteral("127.0.0.1");
const QString kDefaultAccessLogFileName = QStringLiteral("Vaccess.log");
const QString kDefaultErrorLogFileName = QStringLiteral("Verror.log");

bool isSupportedSingBoxNetwork(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("grpc"),
        QStringLiteral("h2"),
        QStringLiteral("httpupgrade"),
        QStringLiteral("quic")};
    return supportedNetworks.contains(network);
}

bool isSupportedSingBoxNonTcpTransport(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
    case ConfigType::VLESS:
    case ConfigType::Trojan:
    case ConfigType::Shadowsocks:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
        return true;
    case ConfigType::WireGuard:
    case ConfigType::Socks:
    case ConfigType::HTTP:
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return false;
    }
}

bool isSupportedSingBoxShadowsocksTransport(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("quic")};
    return supportedNetworks.contains(network);
}

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

    const OperationResult validationResult = validateServer(server);
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
        compat.root = buildTunCompatSingBoxRoot(config);
        generated.auxiliary.append(compat);
    }

    return generated;
}

OperationResult ClientConfigWriter::validateServer(const VmessItem& server) const
{
    const CoreType runtimeCore = resolvePreferredCoreType(Config{}, server);
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

    if (runtimeCore != CoreType::SingBox) {
        return OperationResult::ok();
    }

    if (ProtocolConfigMapper::resolveSingBoxOutboundType(server.configType).isEmpty()) {
        return OperationResult::fail(QStringLiteral("The selected server type is not supported by the current sing-box generator."));
    }

    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (!isSupportedSingBoxNetwork(network)) {
        return OperationResult::fail(
            QStringLiteral("sing-box config generation does not support network %1 yet.").arg(network));
    }

    if (network != QStringLiteral("tcp") && !isSupportedSingBoxNonTcpTransport(server.configType)) {
        return OperationResult::fail(
            QStringLiteral("sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    if (server.configType == ConfigType::Shadowsocks
        && !isSupportedSingBoxShadowsocksTransport(network)) {
        return OperationResult::fail(
            QStringLiteral("sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    return OperationResult::ok();
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
    const bool useSingBox = requiresSingBox || effectiveCore == CoreType::SingBox;
    return useSingBox
        ? buildSingBoxRoot(config, server)
        : buildLegacyRoot(config, server);
}

QJsonObject ClientConfigWriter::buildLegacyRoot(const Config& config, const VmessItem& server) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config));
    root.insert(QStringLiteral("inbounds"), buildInbounds(config));
    root.insert(QStringLiteral("outbounds"), buildOutbounds(config, server));

    const RoutingItem* selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    const QJsonObject routing = RoutingConfigFragments::buildLegacyRouting(config, selectedRouting);
    if (!routing.isEmpty()) {
        root.insert(QStringLiteral("routing"), routing);
    }

    const QJsonObject dns = DnsConfigFragments::buildLegacyDns(config, selectedRouting);
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    return root;
}

QJsonObject ClientConfigWriter::buildSingBoxRoot(const Config& config, const VmessItem& server) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildSingBoxLog(config));
    root.insert(QStringLiteral("inbounds"), buildSingBoxInbounds(config));
    root.insert(QStringLiteral("outbounds"), buildSingBoxOutbounds(config, server));
    const RoutingItem* selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    root.insert(QStringLiteral("route"), RoutingConfigFragments::buildSingBoxRoute(config, selectedRouting));

    const QJsonObject dns = DnsConfigFragments::buildSingBoxDns(config, selectedRouting);
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    const QJsonObject experimental = buildSingBoxExperimental(config);
    if (!experimental.isEmpty()) {
        root.insert(QStringLiteral("experimental"), experimental);
    }

    RoutingConfigFragments::migrateGeoToRuleSet(root);
    return root;
}

QJsonObject ClientConfigWriter::buildTunCompatSingBoxRoot(const Config& config) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildSingBoxLog(config));

    QJsonArray inbounds;
    inbounds.append(SingBoxConfigFragments::buildTunInbound(config));
    root.insert(QStringLiteral("inbounds"), inbounds);

    root.insert(QStringLiteral("outbounds"), SingBoxConfigFragments::buildTunCompatOutbounds(config));
    root.insert(QStringLiteral("route"), SingBoxConfigFragments::buildTunCompatRoute(config));

    const QJsonObject dns = SingBoxConfigFragments::buildTunCompatDns();
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    return root;
}

QJsonObject ClientConfigWriter::buildSingBoxExperimental(const Config& config) const
{
    QJsonObject experimental;

    if (config.dns().enableCacheFile4Sbox) {
        QJsonObject cacheFile;
        cacheFile.insert(QStringLiteral("enabled"), true);
        cacheFile.insert(
            QStringLiteral("path"),
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cache.db")));
        cacheFile.insert(QStringLiteral("store_fakeip"), config.dns().fakeIp);
        experimental.insert(QStringLiteral("cache_file"), cacheFile);
    }

    return experimental;
}

QJsonObject ClientConfigWriter::buildLog(const Config& config) const
{
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), config.logLevel.trimmed().isEmpty() ? QStringLiteral("warning") : config.logLevel);
    // Match WinForms behavior: empty paths keep access/error on the captured console output,
    // which allows the Information panel to display proxy/direct hit lines in real time.
    log.insert(QStringLiteral("access"), config.logEnabled ? kDefaultAccessLogFileName : QString());
    log.insert(QStringLiteral("error"), config.logEnabled ? kDefaultErrorLogFileName : QString());
    return log;
}

QJsonObject ClientConfigWriter::buildSingBoxLog(const Config& config) const
{
    QJsonObject log;
    log.insert(QStringLiteral("disabled"), false);
    log.insert(QStringLiteral("level"), ProtocolConfigMapper::normalizeSingBoxLogLevel(config.logLevel));
    return log;
}

QJsonArray ClientConfigWriter::buildInbounds(const Config& config) const
{
    QJsonArray inbounds;
    inbounds.append(XrayConfigFragments::buildSocksInbound(config, false, 0));
    inbounds.append(XrayConfigFragments::buildHttpInbound(config, false, 1));
    inbounds.append(XrayConfigFragments::buildHttpInboundWithTag(
        config,
        RoutingConfigFragments::locationProbeTag(),
        RoutingConfigFragments::locationProbePortOffset()));

    if (config.allowLanConnection) {
        inbounds.append(XrayConfigFragments::buildSocksInbound(config, true, 2));
        inbounds.append(XrayConfigFragments::buildHttpInbound(config, true, 3));
    }

    return inbounds;
}

QJsonArray ClientConfigWriter::buildSingBoxInbounds(const Config& config) const
{
    QJsonArray inbounds;
    if (config.tun().tunModeItem.enableTun) {
        inbounds.append(SingBoxConfigFragments::buildTunInbound(config));
    }
    inbounds.append(SingBoxConfigFragments::buildSocksInbound(config, false, 0));
    inbounds.append(SingBoxConfigFragments::buildHttpInbound(config, false, 1));
    inbounds.append(SingBoxConfigFragments::buildHttpInboundWithTag(
        config,
        RoutingConfigFragments::locationProbeTag(),
        RoutingConfigFragments::locationProbePortOffset()));

    if (config.allowLanConnection) {
        inbounds.append(SingBoxConfigFragments::buildSocksInbound(config, true, 2));
        inbounds.append(SingBoxConfigFragments::buildHttpInbound(config, true, 3));
    }

    return inbounds;
}

QJsonArray ClientConfigWriter::buildOutbounds(const Config& config, const VmessItem& server) const
{
    QJsonArray outbounds;
    QJsonObject primaryOutbound = XrayConfigFragments::buildPrimaryOutbound(config, server);
    if (config.dns().enableFragment) {
        const QJsonObject streamSettings = primaryOutbound.value(QStringLiteral("streamSettings")).toObject();
        if (!streamSettings.value(QStringLiteral("security")).toString().trimmed().isEmpty()) {
            QJsonObject updatedStreamSettings = streamSettings;
            QJsonObject sockopt = updatedStreamSettings.value(QStringLiteral("sockopt")).toObject();
            if (sockopt.value(QStringLiteral("dialerProxy")).toString().trimmed().isEmpty()) {
                sockopt.insert(QStringLiteral("dialerProxy"), QStringLiteral("frag-proxy"));
                updatedStreamSettings.insert(QStringLiteral("sockopt"), sockopt);
                primaryOutbound.insert(QStringLiteral("streamSettings"), updatedStreamSettings);
                outbounds.append(XrayConfigFragments::buildFragmentOutbound());
            }
        }
    }
    outbounds.append(primaryOutbound);
    outbounds.append(XrayConfigFragments::buildDirectOutbound(config));
    outbounds.append(XrayConfigFragments::buildBlackholeOutbound());
    return outbounds;
}

QJsonArray ClientConfigWriter::buildSingBoxOutbounds(const Config& config, const VmessItem& server) const
{
    QJsonArray outbounds;
    outbounds.append(SingBoxConfigFragments::buildPrimaryOutbound(config, server));
    outbounds.append(SingBoxConfigFragments::buildDirectOutbound());
    outbounds.append(SingBoxConfigFragments::buildBlockOutbound());
    return outbounds;
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

