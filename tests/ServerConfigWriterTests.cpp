#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "runtime/ServerConfigWriter.h"

class ServerConfigWriterTests : public QObject {
    Q_OBJECT

private slots:
    void rejectsRealityWithoutFlow();
    void generatesRealityServerConfig();
    void generatesVmessTcpServerConfig();
};

namespace {

VmessItem baseVlessServer()
{
    VmessItem server;
    server.configType = ConfigType::VLESS;
    server.coreType = CoreType::Xray;
    server.address = QStringLiteral("1.2.3.4");
    server.port = 443;
    server.id = QStringLiteral("a3482e88-686a-4a58-8126-99c9df64b7bf");
    server.flow = QStringLiteral("xtls-rprx-vision");
    return server;
}

} // namespace

void ServerConfigWriterTests::rejectsRealityWithoutFlow()
{
    QTemporaryDir dir;
    const QString filePath = dir.path() + QStringLiteral("/reality_server.json");

    VmessItem server = baseVlessServer();
    server.streamSecurity = QStringLiteral("reality");
    server.flow.clear();

    ServerConfigWriter writer;
    const OperationResult result = writer.writeServerConfig(server, filePath);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("flow")));
}

void ServerConfigWriterTests::generatesRealityServerConfig()
{
    QTemporaryDir dir;
    const QString filePath = dir.path() + QStringLiteral("/reality_server.json");

    VmessItem server = baseVlessServer();
    server.streamSecurity = QStringLiteral("reality");
    server.sni = QStringLiteral("example.com");
    server.shortId = QStringLiteral("abcd1234");
    server.flow = QStringLiteral("xtls-rprx-vision");

    ServerConfigWriter writer;
    const OperationResult result = writer.writeServerConfig(server, filePath);

    QVERIFY(result.success);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();

    const QJsonArray inbounds = root.value(QStringLiteral("inbounds")).toArray();
    QCOMPARE(inbounds.size(), 1);

    const QJsonObject inbound = inbounds.at(0).toObject();
    QCOMPARE(inbound.value(QStringLiteral("protocol")).toString(), QStringLiteral("vless"));

    const QJsonObject streamSettings = inbound.value(QStringLiteral("streamSettings")).toObject();
    QCOMPARE(streamSettings.value(QStringLiteral("security")).toString(), QStringLiteral("reality"));

    const QJsonObject realitySettings =
        streamSettings.value(QStringLiteral("realitySettings")).toObject();

    const QJsonArray serverNames = realitySettings.value(QStringLiteral("serverNames")).toArray();
    QCOMPARE(serverNames.size(), 1);
    QCOMPARE(serverNames.at(0).toString(), QStringLiteral("example.com"));

    QVERIFY(realitySettings.contains(QStringLiteral("privateKey")));
    QVERIFY(realitySettings.contains(QStringLiteral("dest")));

    const QJsonArray shortIds = realitySettings.value(QStringLiteral("shortIds")).toArray();
    QCOMPARE(shortIds.size(), 1);
    QCOMPARE(shortIds.at(0).toString(), QStringLiteral("abcd1234"));
}

void ServerConfigWriterTests::generatesVmessTcpServerConfig()
{
    QTemporaryDir dir;
    const QString filePath = dir.path() + QStringLiteral("/vmess_server.json");

    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Xray;
    server.address = QStringLiteral("5.6.7.8");
    server.port = 10086;
    server.id = QStringLiteral("a3482e88-686a-4a58-8126-99c9df64b7bf");
    server.alterId = 0;

    ServerConfigWriter writer;
    const OperationResult result = writer.writeServerConfig(server, filePath);

    QVERIFY(result.success);

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();

    const QJsonArray inbounds = root.value(QStringLiteral("inbounds")).toArray();
    QCOMPARE(inbounds.size(), 1);

    const QJsonObject inbound = inbounds.at(0).toObject();
    QCOMPARE(inbound.value(QStringLiteral("protocol")).toString(), QStringLiteral("vmess"));
    QCOMPARE(inbound.value(QStringLiteral("port")).toInt(), 10086);

    const QJsonObject settings = inbound.value(QStringLiteral("settings")).toObject();
    const QJsonArray clients = settings.value(QStringLiteral("clients")).toArray();
    QCOMPARE(clients.size(), 1);
    QCOMPARE(clients.at(0).toObject().value(QStringLiteral("id")).toString(),
        QStringLiteral("a3482e88-686a-4a58-8126-99c9df64b7bf"));
    QCOMPARE(clients.at(0).toObject().value(QStringLiteral("alterId")).toInt(), 0);
}

QTEST_MAIN(ServerConfigWriterTests)
#include "ServerConfigWriterTests.moc"
