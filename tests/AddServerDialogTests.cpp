#include <QtTest>

#include "ui/dialogs/AddServerDialog.h"

class AddServerDialogTests : public QObject {
    Q_OBJECT

private slots:
    void alpnRoundTripsExistingServer();
    void httpupgradeUserAgentRoundTripsExistingServer();
    void xhttpExtraRoundTripsExistingServer();
    void finalmaskRoundTripsExistingServer();
    void httpTypeRoundTripsExistingServer();
    void certPemRoundTripsExistingServer();
    void certShaRoundTripsExistingServer();
    void mldsa65VerifyRoundTripsExistingServer();
    void echFieldsRoundTripExistingServer();
    void muxOverrideRoundTripsExplicitDisable();
    void newDialogKeepsMuxOverrideInheritedByDefault();
};

void AddServerDialogTests::alpnRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("keep alpn");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.network = QStringLiteral("ws");
    server.headerType = QStringLiteral("none");
    server.streamSecurity = QStringLiteral("tls");
    server.requestHost = QStringLiteral("cdn.example.com");
    server.path = QStringLiteral("/ws");
    server.alpn = QStringList{
        QStringLiteral("h2"),
        QStringLiteral("http/1.1")};

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.alpn, server.alpn);
}

void AddServerDialogTests::httpupgradeUserAgentRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("keep user-agent");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.network = QStringLiteral("httpupgrade");
    server.headerType = QStringLiteral("none");
    server.streamSecurity = QStringLiteral("tls");
    server.requestHost = QStringLiteral("cdn.example.com");
    server.path = QStringLiteral("/upgrade");
    server.userAgent = QStringLiteral("v2rayN/6.0");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.userAgent, server.userAgent);
}

void AddServerDialogTests::xhttpExtraRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("keep xhttp extra");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.network = QStringLiteral("xhttp");
    server.headerType = QStringLiteral("stream-up");
    server.streamSecurity = QStringLiteral("tls");
    server.requestHost = QStringLiteral("cdn.example.com");
    server.path = QStringLiteral("/xhttp");
    server.extra = QStringLiteral("{\"scMaxEachPostBytes\":12345}");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.extra, server.extra);
}

void AddServerDialogTests::finalmaskRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("keep finalmask");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.network = QStringLiteral("kcp");
    server.headerType = QStringLiteral("wireguard");
    server.path = QStringLiteral("seed-value");
    server.finalmask = QStringLiteral("{\"udp\":[{\"type\":\"mkcp-aes128gcm\",\"settings\":{\"password\":\"seed-value\"}}]}");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.finalmask, server.finalmask);
}

void AddServerDialogTests::httpTypeRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::HTTP;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("http proxy");
    server.address = QStringLiteral("example.com");
    server.port = 8080;
    server.id = QStringLiteral("alice");
    server.security = QStringLiteral("secret");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.configType, ConfigType::HTTP);
    QCOMPARE(updated.id, server.id);
    QCOMPARE(updated.security, server.security);
}

void AddServerDialogTests::certPemRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("tls cert");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.streamSecurity = QStringLiteral("tls");
    server.cert = QStringLiteral(
        "-----BEGIN CERTIFICATE-----\n"
        "MIIBszCCAVmgAwIBAgIUQ2VydGlmaWNhdGUtT25lMB4XDTI2MDQxMzAwMDAwMFoX\n"
        "DTM2MDQxMzAwMDAwMFowFDESMBAGA1UEAwwJRXhhbXBsZSBDQTBaMBMGByqGSM49\n"
        "AgEGCCqGSM49AwEHA0IABK1bTnJ0b2Jlc2VydmVkY2VydGlmaWNhdGVkYXRhMDEw\n"
        "MDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwCgYIKoZIzj0EAwIDSAAwRQIgA1J3\n"
        "Q2VydGlmaWNhdGVPbmVTaWduYXR1cmUwIhAKQ2VydGlmaWNhdGVPbmVTdWJqZWN0\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIBrTCCAVOgAwIBAgIUQ2VydGlmaWNhdGUtVHdvMB4XDTI2MDQxMzAwMDAwMFoX\n"
        "DTM2MDQxMzAwMDAwMFowFDESMBAGA1UEAwwJRXhhbXBsZSBSb290MFkwEwYHKoZI\n"
        "zj0CAQYIKoZIzj0DAQcDQgAEQ2VydGlmaWNhdGVUd29EYXRhMDEwMDAwMDAwMDAw\n"
        "MDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAKBggqhkjOPQQDAgNJADBGAiEA\n"
        "Q2VydGlmaWNhdGVUd29TaWduYXR1cmUwIhAKQ2VydGlmaWNhdGVUd29TdWJqZWN0\n"
        "-----END CERTIFICATE-----");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.cert, server.cert);
}

void AddServerDialogTests::certShaRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("tls pinning");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.streamSecurity = QStringLiteral("tls");
    server.certSha = QStringLiteral("sha256/base64-one,sha256/base64-two");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.certSha, server.certSha);
}

void AddServerDialogTests::mldsa65VerifyRoundTripsExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VLESS;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("reality verify");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("none");
    server.streamSecurity = QStringLiteral("reality");
    server.publicKey = QStringLiteral("reality-public-key");
    server.mldsa65Verify = QStringLiteral("mldsa65-public-key");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.mldsa65Verify, server.mldsa65Verify);
}

void AddServerDialogTests::echFieldsRoundTripExistingServer()
{
    VmessItem server;
    server.configType = ConfigType::VLESS;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("ech tls");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("none");
    server.streamSecurity = QStringLiteral("tls");
    server.echConfigList = QStringLiteral("ECHCONFIGBASE64");
    server.echForceQuery = QStringLiteral("half");

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QCOMPARE(updated.echConfigList, server.echConfigList);
    QCOMPARE(updated.echForceQuery, server.echForceQuery);
}

void AddServerDialogTests::muxOverrideRoundTripsExplicitDisable()
{
    VmessItem server;
    server.configType = ConfigType::VMess;
    server.coreType = CoreType::Auto;
    server.remarks = QStringLiteral("keep mux override");
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("auto");
    server.network = QStringLiteral("tcp");
    server.headerType = QStringLiteral("none");
    server.muxEnabled = false;

    AddServerDialog dialog;
    dialog.setServer(server);

    const VmessItem updated = dialog.server();
    QVERIFY(updated.muxEnabled.has_value());
    QCOMPARE(updated.muxEnabled.value(), false);
}

void AddServerDialogTests::newDialogKeepsMuxOverrideInheritedByDefault()
{
    AddServerDialog dialog;

    const VmessItem updated = dialog.server();
    QVERIFY(!updated.muxEnabled.has_value());
}

QTEST_MAIN(AddServerDialogTests)

#include "AddServerDialogTests.moc"
