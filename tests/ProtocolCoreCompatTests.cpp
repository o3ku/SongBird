#include <QtTest>

#include "runtime/CoreLaunchCompatDecision.h"
#include "runtime/ProtocolCoreCompat.h"
#include "runtime/core/CoreCatalog.h"

namespace {

Config configWithStoredCore(ConfigType configType, CoreType coreType)
{
    Config config;
    config.policy().coreTypeItems.append(
        CoreTypeItem{static_cast<int>(configType), static_cast<int>(coreType)});
    return config;
}

VmessItem serverOfType(ConfigType configType)
{
    VmessItem server;
    server.configType = configType;
    return server;
}

} // namespace

class ProtocolCoreCompatTests : public QObject {
    Q_OBJECT

private slots:
    void dualProtocolSupportsBothCores();
    void httpSupportsOnlyXray();
    void hysteria2SupportsBothCores();
    void singBoxFirstProtocolsSupportBothCoresForSelection();
    void singBoxOnlyProtocolsExcludeXray();
    void unknownSupportsNeitherCore();
    void protocolSupportsCoreQuery();
    void availableCoreTypesList();
    void catalogExposesRegisteredCores();
    void catalogKeepsSingBoxClientExecutableFirst();
    void resolveExistingCoreTypeForProtocolPrefersSingBoxWhenPresent();
    void resolveExistingCoreTypeForProtocolFallsBackToXrayWhenOnlyXrayExists();
    void resolveExistingCoreTypeForProtocolUsesSingBoxAsDownloadTargetWhenNothingExists();
    void resolveExistingCoreTypeForCustomProtocolPrefersSingBoxWhenPresent();
    void resolveSelectedCoreTypeForCustomProtocolHonorsServerCore();
    void resolveExistingCoreTypeForSingBoxFirstProtocolsPrefersSingBox();
    void defaultCoreTypeForAllSupportedProtocolsUsesSingBox();
    void coreLaunchCompatAcceptsSupportedStoredCore();
    void coreLaunchCompatRequiresSwitchWhenStoredCoreCannotRunProtocol();
    void coreLaunchCompatReportsNoCompatibleCoreForUnknownProtocol();
    void coreLaunchCompatSkipsCustomProtocol();
};

void ProtocolCoreCompatTests::dualProtocolSupportsBothCores()
{
    const QList<ConfigType> dual = {
        ConfigType::VMess, ConfigType::Custom, ConfigType::Shadowsocks,
        ConfigType::Socks, ConfigType::VLESS, ConfigType::Trojan
    };
    for (const ConfigType ct : dual) {
        const auto cores = supportedCoreTypes(ct);
        QCOMPARE(cores.size(), 3);
        QVERIFY(cores.contains(CoreType::Xray));
        QVERIFY(cores.contains(CoreType::SingBox));
        QVERIFY(cores.contains(CoreType::Mihomo));
    }
}

void ProtocolCoreCompatTests::httpSupportsOnlyXray()
{
    const auto cores = supportedCoreTypes(ConfigType::HTTP);
    QCOMPARE(cores.size(), 3);
    QVERIFY(cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
    QVERIFY(cores.contains(CoreType::Mihomo));
}

void ProtocolCoreCompatTests::hysteria2SupportsBothCores()
{
    const auto cores = supportedCoreTypes(ConfigType::Hysteria2);
    QCOMPARE(cores.size(), 3);
    QVERIFY(cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
    QVERIFY(cores.contains(CoreType::Mihomo));
}

void ProtocolCoreCompatTests::singBoxFirstProtocolsSupportBothCoresForSelection()
{
    const QList<ConfigType> protocols = {
        ConfigType::TUIC,
        ConfigType::WireGuard
    };

    for (const ConfigType configType : protocols) {
        const auto cores = supportedCoreTypes(configType);
        QCOMPARE(cores.size(), 2);
        QVERIFY(cores.contains(CoreType::Xray));
        QVERIFY(cores.contains(CoreType::SingBox));
    }
}

void ProtocolCoreCompatTests::singBoxOnlyProtocolsExcludeXray()
{
    const QList<ConfigType> protocols = {
        ConfigType::AnyTLS,
        ConfigType::Naive
    };

    for (const ConfigType configType : protocols) {
        const auto cores = supportedCoreTypes(configType);
        QCOMPARE(cores.size(), 1);
        QVERIFY(cores.contains(CoreType::SingBox));
        QVERIFY(!cores.contains(CoreType::Xray));
        QVERIFY(!cores.contains(CoreType::Mihomo));
    }
}

void ProtocolCoreCompatTests::unknownSupportsNeitherCore()
{
    const auto cores = supportedCoreTypes(ConfigType::Unknown);
    QVERIFY(cores.isEmpty());
}

void ProtocolCoreCompatTests::protocolSupportsCoreQuery()
{
    QVERIFY(protocolSupportsCore(ConfigType::VMess, CoreType::Xray));
    QVERIFY(protocolSupportsCore(ConfigType::VMess, CoreType::SingBox));
    QVERIFY(protocolSupportsCore(ConfigType::VMess, CoreType::Mihomo));
    QVERIFY(protocolSupportsCore(ConfigType::HTTP, CoreType::Xray));
    QVERIFY(protocolSupportsCore(ConfigType::HTTP, CoreType::SingBox));
    QVERIFY(protocolSupportsCore(ConfigType::HTTP, CoreType::Mihomo));
    QVERIFY(protocolSupportsCore(ConfigType::Hysteria2, CoreType::Xray));
    QVERIFY(protocolSupportsCore(ConfigType::Hysteria2, CoreType::SingBox));
    QVERIFY(protocolSupportsCore(ConfigType::Hysteria2, CoreType::Mihomo));
    QVERIFY(!protocolSupportsCore(ConfigType::Unknown, CoreType::Xray));
    QVERIFY(!protocolSupportsCore(ConfigType::TUIC, CoreType::Mihomo));
    QVERIFY(!protocolSupportsCore(ConfigType::AnyTLS, CoreType::Xray));
    QVERIFY(!protocolSupportsCore(ConfigType::Naive, CoreType::Xray));
    QVERIFY(protocolSupportsCore(ConfigType::AnyTLS, CoreType::SingBox));
}

void ProtocolCoreCompatTests::availableCoreTypesList()
{
    const auto cores = availableCoreTypes();
    QVERIFY(cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
    QVERIFY(cores.contains(CoreType::Mihomo));
    QVERIFY(cores.size() >= 3);
}

void ProtocolCoreCompatTests::catalogExposesRegisteredCores()
{
    const QList<CoreType> cores = catalogCoreTypes();
    QCOMPARE(cores, QList<CoreType>({CoreType::Xray, CoreType::SingBox, CoreType::Mihomo}));
    QCOMPARE(availableCoreTypes(), cores);
}

void ProtocolCoreCompatTests::catalogKeepsSingBoxClientExecutableFirst()
{
    const QStringList executableNames = catalogCoreExecutableNames(CoreType::SingBox);
    QCOMPARE(executableNames.size(), 2);
    QCOMPARE(executableNames.constFirst(), QStringLiteral("sing-box-client.exe"));
    QCOMPARE(executableNames.at(1), QStringLiteral("sing-box.exe"));
}

void ProtocolCoreCompatTests::resolveExistingCoreTypeForProtocolPrefersSingBoxWhenPresent()
{
    const CoreType core = resolveExistingCoreTypeForProtocol(
        ConfigType::VMess,
        QList<CoreType>{CoreType::Xray, CoreType::SingBox});

    QCOMPARE(core, CoreType::SingBox);
}

void ProtocolCoreCompatTests::resolveExistingCoreTypeForProtocolFallsBackToXrayWhenOnlyXrayExists()
{
    const CoreType core = resolveExistingCoreTypeForProtocol(
        ConfigType::VMess,
        QList<CoreType>{CoreType::Xray});

    QCOMPARE(core, CoreType::Xray);
}

void ProtocolCoreCompatTests::resolveExistingCoreTypeForProtocolUsesSingBoxAsDownloadTargetWhenNothingExists()
{
    const CoreType core = resolveExistingCoreTypeForProtocol(ConfigType::VMess, {});

    QCOMPARE(core, CoreType::SingBox);
}

void ProtocolCoreCompatTests::resolveExistingCoreTypeForCustomProtocolPrefersSingBoxWhenPresent()
{
    const CoreType core = resolveExistingCoreTypeForProtocol(
        ConfigType::Custom,
        QList<CoreType>{CoreType::SingBox});

    QCOMPARE(core, CoreType::SingBox);
}

void ProtocolCoreCompatTests::resolveSelectedCoreTypeForCustomProtocolHonorsServerCore()
{
    Config config;
    VmessItem server;
    server.configType = ConfigType::Custom;
    server.coreType = CoreType::Mihomo;

    QCOMPARE(resolveSelectedCoreType(config, server, availableCoreTypes()), CoreType::Mihomo);
}

void ProtocolCoreCompatTests::resolveExistingCoreTypeForSingBoxFirstProtocolsPrefersSingBox()
{
    const QList<ConfigType> protocols = {
        ConfigType::Hysteria2,
        ConfigType::TUIC,
        ConfigType::WireGuard,
        ConfigType::AnyTLS,
        ConfigType::Naive
    };

    for (const ConfigType configType : protocols) {
        QCOMPARE(resolveExistingCoreTypeForProtocol(configType, {}), CoreType::SingBox);
        QCOMPARE(resolveExistingCoreTypeForProtocol(configType, QList<CoreType>{CoreType::SingBox}), CoreType::SingBox);
        QCOMPARE(resolveExistingCoreTypeForProtocol(configType, QList<CoreType>{CoreType::Xray, CoreType::SingBox}), CoreType::SingBox);
    }
}

void ProtocolCoreCompatTests::defaultCoreTypeForAllSupportedProtocolsUsesSingBox()
{
    const QList<ConfigType> protocols = {
        ConfigType::VMess,
        ConfigType::Custom,
        ConfigType::Shadowsocks,
        ConfigType::Socks,
        ConfigType::VLESS,
        ConfigType::Trojan,
        ConfigType::HTTP,
        ConfigType::Hysteria2,
        ConfigType::TUIC,
        ConfigType::WireGuard,
        ConfigType::AnyTLS,
        ConfigType::Naive
    };

    for (const ConfigType configType : protocols) {
        QCOMPARE(defaultCoreTypeForProtocol(configType), CoreType::SingBox);
    }
}

void ProtocolCoreCompatTests::coreLaunchCompatAcceptsSupportedStoredCore()
{
    const Config config = configWithStoredCore(ConfigType::AnyTLS, CoreType::SingBox);
    const CoreLaunchCompatDecision decision = evaluateCoreLaunchCompat(
        config,
        serverOfType(ConfigType::AnyTLS),
        QList<CoreType>{CoreType::SingBox});

    QCOMPARE(decision.outcome, CoreLaunchCompatOutcome::Compatible);
    QVERIFY(!decision.requiresUserDecision());
    QCOMPARE(decision.storedCore, CoreType::SingBox);
    QCOMPARE(decision.resolvedCore, CoreType::SingBox);
}

void ProtocolCoreCompatTests::coreLaunchCompatRequiresSwitchWhenStoredCoreCannotRunProtocol()
{
    // Legacy configs may still pin AnyTLS to Xray, which cannot run it.
    const Config config = configWithStoredCore(ConfigType::AnyTLS, CoreType::Xray);
    const CoreLaunchCompatDecision decision = evaluateCoreLaunchCompat(
        config,
        serverOfType(ConfigType::AnyTLS),
        QList<CoreType>{CoreType::Xray});

    QCOMPARE(decision.outcome, CoreLaunchCompatOutcome::RequiresCoreSwitch);
    QVERIFY(decision.requiresUserDecision());
    QCOMPARE(decision.storedCore, CoreType::Xray);
    QCOMPARE(decision.resolvedCore, CoreType::SingBox);
}

void ProtocolCoreCompatTests::coreLaunchCompatReportsNoCompatibleCoreForUnknownProtocol()
{
    const Config config;
    const CoreLaunchCompatDecision decision = evaluateCoreLaunchCompat(
        config,
        serverOfType(ConfigType::Unknown),
        availableCoreTypes());

    QCOMPARE(decision.outcome, CoreLaunchCompatOutcome::NoCompatibleCore);
    QVERIFY(!decision.requiresUserDecision());
}

void ProtocolCoreCompatTests::coreLaunchCompatSkipsCustomProtocol()
{
    const Config config = configWithStoredCore(ConfigType::Custom, CoreType::Xray);
    VmessItem server = serverOfType(ConfigType::Custom);
    server.coreType = CoreType::Mihomo;

    const CoreLaunchCompatDecision decision = evaluateCoreLaunchCompat(
        config,
        server,
        availableCoreTypes());

    QCOMPARE(decision.outcome, CoreLaunchCompatOutcome::Compatible);
    QCOMPARE(decision.resolvedCore, CoreType::Mihomo);
}

QTEST_MAIN(ProtocolCoreCompatTests)
#include "ProtocolCoreCompatTests.moc"
