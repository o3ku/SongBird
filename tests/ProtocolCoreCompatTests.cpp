#include <QtTest>

#include "runtime/ProtocolCoreCompat.h"

class ProtocolCoreCompatTests : public QObject {
    Q_OBJECT

private slots:
    void dualProtocolSupportsBothCores();
    void httpSupportsOnlyXray();
    void unknownSupportsNeitherCore();
    void protocolSupportsCoreQuery();
    void availableCoreTypesList();
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
    QCOMPARE(cores.size(), 1);
    QCOMPARE(cores.first(), CoreType::Xray);
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
    QVERIFY(!protocolSupportsCore(ConfigType::HTTP, CoreType::SingBox));
    QVERIFY(!protocolSupportsCore(ConfigType::Unknown, CoreType::Xray));
}

void ProtocolCoreCompatTests::availableCoreTypesList()
{
    const auto cores = availableCoreTypes();
    QVERIFY(cores.contains(CoreType::Xray));
    QVERIFY(cores.contains(CoreType::SingBox));
    QVERIFY(cores.size() >= 2);
}

QTEST_MAIN(ProtocolCoreCompatTests)
#include "ProtocolCoreCompatTests.moc"
