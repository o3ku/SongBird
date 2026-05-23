#include <QtTest>

#include "runtime/ProtocolCoreCompat.h"
#include "runtime/core/CoreCatalog.h"

class ProtocolCoreCompatTests : public QObject {
    Q_OBJECT

private slots:
    void dualProtocolSupportsBothCores();
    void httpSupportsOnlyXray();
    void hysteria2SupportsOnlySingBox();
    void singBoxFirstProtocolsSupportBothCoresForSelection();
    void unknownSupportsNeitherCore();
    void protocolSupportsCoreQuery();
    void availableCoreTypesList();
    void catalogExposesRegisteredCores();
    void catalogKeepsSingBoxClientExecutableFirst();
    void resolveExistingCoreTypeForProtocolPrefersSingBoxWhenPresent();
    void resolveExistingCoreTypeForProtocolFallsBackToXrayWhenOnlyXrayExists();
    void resolveExistingCoreTypeForProtocolUsesSingBoxAsDownloadTargetWhenNothingExists();
    void resolveExistingCoreTypeForCustomProtocolPrefersSingBoxWhenPresent();
    void resolveExistingCoreTypeForSingBoxFirstProtocolsPrefersSingBox();
    void defaultCoreTypeForAllSupportedProtocolsUsesSingBox();
};

void ProtocolCoreCompatTests::dualProtocolSupportsBothCores()
{
    const QList<ConfigType> dual = {
        ConfigType::VMess, ConfigType::Custom, ConfigType::Shadowsocks,
        ConfigType::Socks, ConfigType::VLESS, ConfigType::Trojan
    };
    for (const ConfigType ct : dual) {
        const auto cores = supportedCoreTypes(ct);
        QCOMPARE(cores.size(), 2);
        QVERIFY(cores.contains(CoreType::Xray));
        QVERIFY(cores.contains(CoreType::SingBox));
    }
}

void ProtocolCoreCompatTests::httpSupportsOnlyXray()
{
    const auto cores = supportedCoreTypes(ConfigType::HTTP);
    QCOMPARE(cores.size(), 2);
    QVERIFY(cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
}

void ProtocolCoreCompatTests::hysteria2SupportsOnlySingBox()
{
    const auto cores = supportedCoreTypes(ConfigType::Hysteria2);
    QCOMPARE(cores.size(), 1);
    QVERIFY(!cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
}

void ProtocolCoreCompatTests::singBoxFirstProtocolsSupportBothCoresForSelection()
{
    const QList<ConfigType> protocols = {
        ConfigType::TUIC,
        ConfigType::WireGuard,
        ConfigType::AnyTLS,
        ConfigType::Naive
    };

    for (const ConfigType configType : protocols) {
        const auto cores = supportedCoreTypes(configType);
        QCOMPARE(cores.size(), 2);
        QVERIFY(cores.contains(CoreType::Xray));
        QVERIFY(cores.contains(CoreType::SingBox));
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
    QVERIFY(protocolSupportsCore(ConfigType::HTTP, CoreType::Xray));
    QVERIFY(protocolSupportsCore(ConfigType::HTTP, CoreType::SingBox));
    QVERIFY(!protocolSupportsCore(ConfigType::Hysteria2, CoreType::Xray));
    QVERIFY(protocolSupportsCore(ConfigType::Hysteria2, CoreType::SingBox));
    QVERIFY(!protocolSupportsCore(ConfigType::Unknown, CoreType::Xray));
}

void ProtocolCoreCompatTests::availableCoreTypesList()
{
    const auto cores = availableCoreTypes();
    QVERIFY(cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
    QVERIFY(cores.size() >= 2);
}

void ProtocolCoreCompatTests::catalogExposesRegisteredCores()
{
    const QList<CoreType> cores = catalogCoreTypes();
    QCOMPARE(cores, QList<CoreType>({CoreType::Xray, CoreType::SingBox}));
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

QTEST_MAIN(ProtocolCoreCompatTests)
#include "ProtocolCoreCompatTests.moc"
