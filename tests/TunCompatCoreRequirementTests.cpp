#include <QtTest>

#include "domain/models/Config.h"
#include "runtime/TunCompatCoreRequirement.h"

class TunCompatCoreRequirementTests : public QObject {
    Q_OBJECT

private slots:
    void xrayTunRequiresAuxiliarySingBoxCore();
    void xrayTunWithoutLegacyProtectStillRequiresAuxiliarySingBoxCore();
    void singBoxCoreDoesNotRequireAuxiliarySingBoxCore();
    void httpAutoCoreDoesNotRequireAuxiliarySingBoxCore();
    void customConfigDoesNotRequireAuxiliarySingBoxCore();
    void disabledTunDoesNotRequireAuxiliarySingBoxCore();
};

namespace {

Config baseConfig()
{
    Config config;
    config.tunModeItem.enableTun = true;
    config.tunModeItem.enableLegacyProtect = true;
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
    const QList<CoreType> required = resolveAuxiliaryTunCompatCoreTypes(baseConfig(), baseServer());

    QCOMPARE(required, QList<CoreType>{CoreType::SingBox});
}

void TunCompatCoreRequirementTests::xrayTunWithoutLegacyProtectStillRequiresAuxiliarySingBoxCore()
{
    Config config = baseConfig();
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

void TunCompatCoreRequirementTests::httpAutoCoreDoesNotRequireAuxiliarySingBoxCore()
{
    VmessItem server = baseServer();
    server.configType = ConfigType::HTTP;
    server.coreType = CoreType::Auto;

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

QTEST_MAIN(TunCompatCoreRequirementTests)

#include "TunCompatCoreRequirementTests.moc"
