#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "subscription/ConfigFileImportParser.h"

namespace {

QJsonObject baseVmessUser()
{
    QJsonObject user;
    user.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    user.insert(QStringLiteral("alterId"), 0);
    user.insert(QStringLiteral("security"), QStringLiteral("auto"));
    return user;
}

QString buildClientConfigJson(const QJsonObject& streamSettings)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("vmess"));

    QJsonObject remote;
    remote.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    remote.insert(QStringLiteral("port"), 443);
    remote.insert(QStringLiteral("users"), QJsonArray{baseVmessUser()});

    QJsonObject settings;
    settings.insert(QStringLiteral("vnext"), QJsonArray{remote});
    outbound.insert(QStringLiteral("settings"), settings);
    outbound.insert(QStringLiteral("streamSettings"), streamSettings);

    QJsonObject root;
    root.insert(QStringLiteral("outbounds"), QJsonArray{outbound});
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace

class ConfigFileImportParserTests : public QObject {
    Q_OBJECT

private slots:
    void parseClientConfigReadsTcpHttpRequestPath();
    void parseClientConfigReadsWebSocketUserAgent();
    void parseClientConfigReadsGrpcMultiMode();
    void parseClientConfigReadsHttpupgradeTransportFields();
    void parseClientConfigReadsXhttpTransportFields();
    void parseClientConfigReadsXtlsSettings();
    void parseClientConfigReadsFinalmask();
    void parseClientConfigReadsTlsCertificates();
    void parseClientConfigReadsPinnedPeerCertSha256();
    void parseClientConfigReadsRealityMldsa65Verify();
    void parseClientConfigReadsTlsEchFields();
    void parseClientConfigReadsVlessOutbound();
    void parseClientConfigReadsTrojanOutbound();
    void parseClientConfigReadsShadowsocksOutbound();
    void parseServerConfigReadsVlessInbound();
};

void ConfigFileImportParserTests::parseClientConfigReadsTcpHttpRequestPath()
{
    QJsonObject requestHeaders;
    requestHeaders.insert(QStringLiteral("Host"), QJsonArray{QStringLiteral("cdn.example.com")});

    QJsonObject request;
    request.insert(QStringLiteral("path"), QJsonArray{QStringLiteral("/edge"), QStringLiteral("/fallback")});
    request.insert(QStringLiteral("headers"), requestHeaders);

    QJsonObject header;
    header.insert(QStringLiteral("type"), QStringLiteral("http"));
    header.insert(QStringLiteral("request"), request);

    QJsonObject tcpSettings;
    tcpSettings.insert(QStringLiteral("header"), header);

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("tcp"));
    streamSettings.insert(QStringLiteral("tcpSettings"), tcpSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.network, QStringLiteral("tcp"));
    QCOMPARE(item.headerType, QStringLiteral("http"));
    QCOMPARE(item.path, QStringLiteral("/edge,/fallback"));
    QCOMPARE(item.requestHost, QStringLiteral("cdn.example.com"));
}

void ConfigFileImportParserTests::parseClientConfigReadsWebSocketUserAgent()
{
    QJsonObject headers;
    headers.insert(QStringLiteral("Host"), QStringLiteral("cdn.example.com"));
    headers.insert(QStringLiteral("User-Agent"), QStringLiteral("v2rayN/6.0"));

    QJsonObject wsSettings;
    wsSettings.insert(QStringLiteral("path"), QStringLiteral("/ws"));
    wsSettings.insert(QStringLiteral("headers"), headers);

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("ws"));
    streamSettings.insert(QStringLiteral("wsSettings"), wsSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.network, QStringLiteral("ws"));
    QCOMPARE(item.path, QStringLiteral("/ws"));
    QCOMPARE(item.requestHost, QStringLiteral("cdn.example.com"));
    QCOMPARE(item.userAgent, QStringLiteral("v2rayN/6.0"));
}

void ConfigFileImportParserTests::parseClientConfigReadsGrpcMultiMode()
{
    QJsonObject grpcSettings;
    grpcSettings.insert(QStringLiteral("serviceName"), QStringLiteral("gun"));
    grpcSettings.insert(QStringLiteral("multiMode"), true);

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("grpc"));
    streamSettings.insert(QStringLiteral("grpcSettings"), grpcSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.network, QStringLiteral("grpc"));
    QCOMPARE(item.path, QStringLiteral("gun"));
    QCOMPARE(item.headerType, QStringLiteral("multi"));
}

void ConfigFileImportParserTests::parseClientConfigReadsHttpupgradeTransportFields()
{
    QJsonObject headers;
    headers.insert(QStringLiteral("User-Agent"), QStringLiteral("CustomUA/1.0"));

    QJsonObject httpupgradeSettings;
    httpupgradeSettings.insert(QStringLiteral("host"), QStringLiteral("cdn.example.com"));
    httpupgradeSettings.insert(QStringLiteral("path"), QStringLiteral("/upgrade"));
    httpupgradeSettings.insert(QStringLiteral("headers"), headers);

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("httpupgrade"));
    streamSettings.insert(QStringLiteral("httpupgradeSettings"), httpupgradeSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.network, QStringLiteral("httpupgrade"));
    QCOMPARE(item.requestHost, QStringLiteral("cdn.example.com"));
    QCOMPARE(item.path, QStringLiteral("/upgrade"));
    QCOMPARE(item.userAgent, QStringLiteral("CustomUA/1.0"));
}

void ConfigFileImportParserTests::parseClientConfigReadsXhttpTransportFields()
{
    QJsonObject xhttpSettings;
    xhttpSettings.insert(QStringLiteral("host"), QStringLiteral("cdn.example.com"));
    xhttpSettings.insert(QStringLiteral("path"), QStringLiteral("/xhttp"));
    xhttpSettings.insert(QStringLiteral("mode"), QStringLiteral("stream-up"));
    xhttpSettings.insert(QStringLiteral("extra"), QJsonObject{{QStringLiteral("scMaxEachPostBytes"), 12345}});

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("xhttp"));
    streamSettings.insert(QStringLiteral("xhttpSettings"), xhttpSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.network, QStringLiteral("xhttp"));
    QCOMPARE(item.requestHost, QStringLiteral("cdn.example.com"));
    QCOMPARE(item.path, QStringLiteral("/xhttp"));
    QCOMPARE(item.headerType, QStringLiteral("stream-up"));
    QCOMPARE(item.extra, QStringLiteral("{\"scMaxEachPostBytes\":12345}"));
}

void ConfigFileImportParserTests::parseClientConfigReadsXtlsSettings()
{
    QJsonObject xtlsSettings;
    xtlsSettings.insert(QStringLiteral("serverName"), QStringLiteral("tls.example.com"));
    xtlsSettings.insert(QStringLiteral("allowInsecure"), true);
    xtlsSettings.insert(QStringLiteral("alpn"), QJsonArray{QStringLiteral("h2"), QStringLiteral("http/1.1")});

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("xtls"));
    streamSettings.insert(QStringLiteral("xtlsSettings"), xtlsSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.streamSecurity, QStringLiteral("xtls"));
    QCOMPARE(item.sni, QStringLiteral("tls.example.com"));
    QCOMPARE(item.allowInsecure, QStringLiteral("true"));
    QCOMPARE(item.alpn, QStringList({QStringLiteral("h2"), QStringLiteral("http/1.1")}));
}

void ConfigFileImportParserTests::parseClientConfigReadsFinalmask()
{
    QJsonObject streamSettings;
    streamSettings.insert(
        QStringLiteral("finalmask"),
        QJsonObject{
            {QStringLiteral("udp"),
             QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("mkcp-original")}}}}});

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.finalmask, QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));
}

void ConfigFileImportParserTests::parseClientConfigReadsTlsCertificates()
{
    QJsonObject certificate;
    certificate.insert(
        QStringLiteral("certificate"),
        QJsonArray{
            QStringLiteral("-----BEGIN CERTIFICATE-----"),
            QStringLiteral("CERT-ONE"),
            QStringLiteral("-----END CERTIFICATE-----")});

    QJsonObject tlsSettings;
    tlsSettings.insert(QStringLiteral("certificates"), QJsonArray{certificate});

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("tls"));
    streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(
        item.cert,
        QStringLiteral(
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-ONE\n"
            "-----END CERTIFICATE-----"));
}

void ConfigFileImportParserTests::parseClientConfigReadsPinnedPeerCertSha256()
{
    QJsonObject tlsSettings;
    tlsSettings.insert(QStringLiteral("pinnedPeerCertSha256"), QStringLiteral("sha256/base64-one,sha256/base64-two"));

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("tls"));
    streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.certSha, QStringLiteral("sha256/base64-one,sha256/base64-two"));
}

void ConfigFileImportParserTests::parseClientConfigReadsRealityMldsa65Verify()
{
    QJsonObject realitySettings;
    realitySettings.insert(QStringLiteral("publicKey"), QStringLiteral("reality-public-key"));
    realitySettings.insert(QStringLiteral("mldsa65Verify"), QStringLiteral("mldsa65-public-key"));

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("reality"));
    streamSettings.insert(QStringLiteral("realitySettings"), realitySettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.mldsa65Verify, QStringLiteral("mldsa65-public-key"));
}

void ConfigFileImportParserTests::parseClientConfigReadsTlsEchFields()
{
    QJsonObject tlsSettings;
    tlsSettings.insert(QStringLiteral("echConfigList"), QStringLiteral("ECHCONFIGBASE64"));
    tlsSettings.insert(QStringLiteral("echForceQuery"), QStringLiteral("half"));

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("tls"));
    streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseClientConfig(buildClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.echConfigList, QStringLiteral("ECHCONFIGBASE64"));
    QCOMPARE(item.echForceQuery, QStringLiteral("half"));
}

namespace {

QString buildVlessClientConfigJson(const QJsonObject& streamSettings)
{
    QJsonObject user;
    user.insert(QStringLiteral("id"), QStringLiteral("22222222-2222-2222-2222-222222222222"));
    user.insert(QStringLiteral("flow"), QStringLiteral("xtls-rprx-vision"));
    user.insert(QStringLiteral("encryption"), QStringLiteral("none"));

    QJsonObject remote;
    remote.insert(QStringLiteral("address"), QStringLiteral("vless.example.com"));
    remote.insert(QStringLiteral("port"), 443);
    remote.insert(QStringLiteral("users"), QJsonArray{user});

    QJsonObject settings;
    settings.insert(QStringLiteral("vnext"), QJsonArray{remote});

    QJsonObject outbound;
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("vless"));
    outbound.insert(QStringLiteral("settings"), settings);
    outbound.insert(QStringLiteral("streamSettings"), streamSettings);

    QJsonObject root;
    root.insert(QStringLiteral("outbounds"), QJsonArray{outbound});
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString buildTrojanClientConfigJson(const QJsonObject& streamSettings)
{
    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("trojan.example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("password"), QStringLiteral("trojan-password-123"));

    QJsonObject settings;
    settings.insert(QStringLiteral("servers"), QJsonArray{server});

    QJsonObject outbound;
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("trojan"));
    outbound.insert(QStringLiteral("settings"), settings);
    outbound.insert(QStringLiteral("streamSettings"), streamSettings);

    QJsonObject root;
    root.insert(QStringLiteral("outbounds"), QJsonArray{outbound});
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString buildShadowsocksClientConfigJson()
{
    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("ss.example.com"));
    server.insert(QStringLiteral("port"), 8388);
    server.insert(QStringLiteral("method"), QStringLiteral("aes-256-gcm"));
    server.insert(QStringLiteral("password"), QStringLiteral("ss-password-456"));

    QJsonObject settings;
    settings.insert(QStringLiteral("servers"), QJsonArray{server});

    QJsonObject outbound;
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("shadowsocks"));
    outbound.insert(QStringLiteral("settings"), settings);

    QJsonObject root;
    root.insert(QStringLiteral("outbounds"), QJsonArray{outbound});
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace

void ConfigFileImportParserTests::parseClientConfigReadsVlessOutbound()
{
    QJsonObject tlsSettings;
    tlsSettings.insert(QStringLiteral("serverName"), QStringLiteral("vless.example.com"));

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("tcp"));
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("reality"));
    streamSettings.insert(QStringLiteral("realitySettings"), tlsSettings);

    VmessItem item;
    const OperationResult result =
        ConfigFileImportParser::parseClientConfig(buildVlessClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.configType, ConfigType::VLESS);
    QCOMPARE(item.address, QStringLiteral("vless.example.com"));
    QCOMPARE(item.port, 443);
    QCOMPARE(item.id, QStringLiteral("22222222-2222-2222-2222-222222222222"));
    QCOMPARE(item.flow, QStringLiteral("xtls-rprx-vision"));
    QCOMPARE(item.streamSecurity, QStringLiteral("reality"));
}

void ConfigFileImportParserTests::parseClientConfigReadsTrojanOutbound()
{
    QJsonObject wsSettings;
    wsSettings.insert(QStringLiteral("path"), QStringLiteral("/trojan-ws"));

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("ws"));
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("tls"));
    streamSettings.insert(QStringLiteral("wsSettings"), wsSettings);

    VmessItem item;
    const OperationResult result =
        ConfigFileImportParser::parseClientConfig(buildTrojanClientConfigJson(streamSettings), &item);

    QVERIFY(result.success);
    QCOMPARE(item.configType, ConfigType::Trojan);
    QCOMPARE(item.address, QStringLiteral("trojan.example.com"));
    QCOMPARE(item.port, 443);
    QCOMPARE(item.id, QStringLiteral("trojan-password-123"));
    QCOMPARE(item.network, QStringLiteral("ws"));
    QCOMPARE(item.path, QStringLiteral("/trojan-ws"));
}

void ConfigFileImportParserTests::parseClientConfigReadsShadowsocksOutbound()
{
    VmessItem item;
    const OperationResult result =
        ConfigFileImportParser::parseClientConfig(buildShadowsocksClientConfigJson(), &item);

    QVERIFY(result.success);
    QCOMPARE(item.configType, ConfigType::Shadowsocks);
    QCOMPARE(item.address, QStringLiteral("ss.example.com"));
    QCOMPARE(item.port, 8388);
    QCOMPARE(item.id, QStringLiteral("ss-password-456"));
    QCOMPARE(item.security, QStringLiteral("aes-256-gcm"));
}

void ConfigFileImportParserTests::parseServerConfigReadsVlessInbound()
{
    QJsonObject client;
    client.insert(QStringLiteral("id"), QStringLiteral("33333333-3333-3333-3333-333333333333"));
    client.insert(QStringLiteral("flow"), QStringLiteral("xtls-rprx-vision"));

    QJsonObject settings;
    settings.insert(QStringLiteral("clients"), QJsonArray{client});
    settings.insert(QStringLiteral("decryption"), QStringLiteral("none"));

    QJsonObject realitySettings;
    realitySettings.insert(QStringLiteral("publicKey"), QStringLiteral("pubkey123"));
    realitySettings.insert(QStringLiteral("shortId"), QStringLiteral("abcd1234"));

    QJsonObject streamSettings;
    streamSettings.insert(QStringLiteral("network"), QStringLiteral("tcp"));
    streamSettings.insert(QStringLiteral("security"), QStringLiteral("reality"));
    streamSettings.insert(QStringLiteral("realitySettings"), realitySettings);

    QJsonObject inbound;
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("vless"));
    inbound.insert(QStringLiteral("listen"), QStringLiteral("0.0.0.0"));
    inbound.insert(QStringLiteral("port"), 443);
    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("streamSettings"), streamSettings);

    QJsonObject root;
    root.insert(QStringLiteral("inbounds"), QJsonArray{inbound});
    const QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    VmessItem item;
    const OperationResult result = ConfigFileImportParser::parseServerConfig(json, &item);

    QVERIFY(result.success);
    QCOMPARE(item.configType, ConfigType::VLESS);
    QCOMPARE(item.port, 443);
    QCOMPARE(item.id, QStringLiteral("33333333-3333-3333-3333-333333333333"));
    QCOMPARE(item.flow, QStringLiteral("xtls-rprx-vision"));
    QCOMPARE(item.streamSecurity, QStringLiteral("reality"));
    QCOMPARE(item.publicKey, QStringLiteral("pubkey123"));
    QCOMPARE(item.shortId, QStringLiteral("abcd1234"));
}

QTEST_MAIN(ConfigFileImportParserTests)

#include "ConfigFileImportParserTests.moc"
