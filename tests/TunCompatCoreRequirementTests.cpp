#include <QtTest>

#include "domain/models/Config.h"
#include "runtime/TunCompatCoreRequirement.h"

class TunCompatCoreRequirementTests : public QObject {
    Q_OBJECT

private slots:
    void xrayTunRequiresAuxiliarySingBoxCore();
    void xrayTunWithoutLegacyProtectStillRequiresAuxiliarySingBoxCore();
    void singBoxCoreDoesNotRequireAuxiliarySingBoxCore();
    void protocolConfiguredSingBoxDoesNotRequireAuxiliarySingBoxCore();
    void httpAutoCoreDoesNotRequireAuxiliarySingBoxCore();
    void customConfigDoesNotRequireAuxiliarySingBoxCore();
    void disabledTunDoesNotRequireAuxiliarySingBoxCore();
    void xrayOnlyInstallWithUnconfiguredProtocolRequiresAuxiliarySingBoxCore();
    void singBoxOnlyInstallWithUnconfiguredProtocolSkipsAuxiliarySingBoxCore();
};

namespace {

Config baseConfig()
{
    Config config;
    config.tunModeItem.enableTun = true;
    config.tunModeItem.enableLegacyProtect = true;
    return config;
}

// Map VMess -> Xray so resolveSelectedCoreType picks Xray for the baseServer.
// VmessItem::coreType is informational and not consulted by the TUN compat path
// (per commit 4b80801), so tests asserting "Xray runs with TUN" must drive the
// decision through coreTypeItems instead.
Config baseConfigWithXrayForVMess()
{
    Config config = baseConfig();
    config.coreTypeItems.append(CoreTypeItem{
        static_cast<int>(ConfigType::VMess),
        static_cast<int>(CoreType::Xray)});
    return config;
}

VmessItem baseServer()
{
    VmessItem server;
    server.coreType = CoreType::Xray;
    server.configType = ConfigType::VMess;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    return server;
}

} // namespace

void TunCompatCoreRequirementTests::xrayTunRequiresAuxiliarySingBoxCore()
{
    const QList<CoreType> required = resolveAuxiliaryTunCompatCoreTypes(
        baseConfigWithXrayForVMess(), baseServer());

    QCOMPARE(required, QList<CoreType>{CoreType::SingBox});
}

void TunCompatCoreRequirementTests::xrayTunWithoutLegacyProtectStillRequiresAuxiliarySingBoxCore()
{
    Config config = baseConfigWithXrayForVMess();
    config.tunModeItem.enableLegacyProtect = false;

    const QList<CoreType> required = resolveAuxiliaryTunCompatCoreTypes(config, baseServer());

    QCOMPARE(required, QList<CoreType>{CoreType::SingBox});
}

void TunCompatCoreRequirementTests::singBoxCoreDoesNotRequireAuxiliarySingBoxCore()
{
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    QVERIFY(resolveAuxiliaryTunCompatCoreTypes(baseConfig(), server).isEmpty());
}

void TunCompatCoreRequirementTests::protocolConfiguredSingBoxDoesNotRequireAuxiliarySingBoxCore()
{
    Config config = baseConfig();
    config.coreTypeItems.append(CoreTypeItem{
        static_cast<int>(ConfigType::VMess),
        static_cast<int>(CoreType::SingBox)});

    VmessItem server = baseServer();
    server.coreType = CoreType::Auto;

    QVERIFY(resolveAuxiliaryTunCompatCoreTypes(config, server).isEmpty());
}

void TunCompatCoreRequirementTests::httpAutoCoreDoesNotRequireAuxiliarySingBoxCore()
{
    VmessItem server = baseServer();
    server.configType = ConfigType::HTTP;
    server.coreType = CoreType::SingBox;

    QVERIFY(resolveAuxiliaryTunCompatCoreTypes(baseConfig(), server).isEmpty());
}

void TunCompatCoreRequirementTests::customConfigDoesNotRequireAuxiliarySingBoxCore()
{
    VmessItem server = baseServer();
    server.configType = ConfigType::Custom;

    QVERIFY(resolveAuxiliaryTunCompatCoreTypes(baseConfig(), server).isEmpty());
}

void TunCompatCoreRequirementTests::disabledTunDoesNotRequireAuxiliarySingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;

    QVERIFY(resolveAuxiliaryTunCompatCoreTypes(config, baseServer()).isEmpty());
}

void TunCompatCoreRequirementTests::xrayOnlyInstallWithUnconfiguredProtocolRequiresAuxiliarySingBoxCore()
{
    // Config has no coreTypeItem entry for this protocol (e.g. older config that
    // predates the protocol or a manually trimmed file), so configuredCoreTypeForProtocol
    // returns Unknown and the launch path falls back to whatever is installed.
    // With only Xray on disk, AppBootstrap will spawn Xray -- TUN compat must
    // therefore demand a sing-box sidecar, even though resolvePreferredCoreType
    // (the pre-fix path that ignored installed cores) would have picked SingBox.
    Config config = baseConfig();
    config.coreTypeItems.clear();

    VmessItem server = baseServer();
    server.coreType = CoreType::Auto;

    const QList<CoreType> required = resolveAuxiliaryTunCompatCoreTypes(
        config, server, QList<CoreType>{CoreType::Xray});

    QCOMPARE(required, QList<CoreType>{CoreType::SingBox});
}

void TunCompatCoreRequirementTests::singBoxOnlyInstallWithUnconfiguredProtocolSkipsAuxiliarySingBoxCore()
{
    // Mirror of the above: only sing-box installed and no protocol config ->
    // launch picks sing-box, no sidecar is needed.
    Config config = baseConfig();
    config.coreTypeItems.clear();

    VmessItem server = baseServer();
    server.coreType = CoreType::Auto;

    QVERIFY(resolveAuxiliaryTunCompatCoreTypes(
        config, server, QList<CoreType>{CoreType::SingBox}).isEmpty());
}

QTEST_MAIN(TunCompatCoreRequirementTests)

#include "TunCompatCoreRequirementTests.moc"
