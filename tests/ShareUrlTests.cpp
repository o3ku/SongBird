#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include "subscription/ShareUrlBuilder.h"
#include "subscription/ShareUrlParser.h"

class ShareUrlTests : public QObject {
    Q_OBJECT

private slots:
    void buildAndParseHttpupgradeTransportRoundTrips();
    void buildAndParseXhttpTransportRoundTrips();
    void buildAndParseFinalmaskRoundTrips();
    void buildAndParseCertShaRoundTrips();
    void buildAndParseRealityMldsa65VerifyRoundTrips();
    void buildAndParseEchRoundTrips();
    void buildAndParseAnytlsRoundTrips();
    void buildAndParseNaiveHttpsRoundTrips();
    void buildAndParseNaiveQuicRoundTrips();
    void buildAndParseTrojanEscapesUserInfo();
    void buildAndParseTuicEscapesUserInfo();
    void parseRejectsOutOfRangePort();
    void parseHysteria2PinSha256IntoCertSha();
    void parseVlessTlsVisionShareUrl();
    void parseVlessRealityVisionShareUrl();
    void parseVlessRealityVisionShareUrlKeepsPacketEncoding();
    void parseAnytlsShareUrlKeepsIdleSessionSettings();
    void parseTrojanShareUrlKeepsAllowInsecure();
    void parseSocksUrlWithEmptyBase64Credentials();
    void parseLegacyVmessShareUrlKeepsVmessType();
};

void ShareUrlTests::buildAndParseHttpupgradeTransportRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::VLESS;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("none");
    source.streamSecurity = QStringLiteral("tls");
    source.network = QStringLiteral("httpupgrade");
    source.requestHost = QStringLiteral("cdn.example.com");
    source.path = QStringLiteral("/upgrade");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.contains(QStringLiteral("type=httpupgrade")));
    QVERIFY(url.contains(QStringLiteral("host=cdn.example.com")));
    QVERIFY(url.contains(QStringLiteral("path=%2Fupgrade")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.network, QStringLiteral("httpupgrade"));
    QCOMPARE(parsed.requestHost, source.requestHost);
    QCOMPARE(parsed.path, source.path);
}

void ShareUrlTests::buildAndParseXhttpTransportRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::VLESS;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("none");
    source.streamSecurity = QStringLiteral("tls");
    source.network = QStringLiteral("xhttp");
    source.requestHost = QStringLiteral("cdn.example.com");
    source.path = QStringLiteral("/xhttp");
    source.headerType = QStringLiteral("stream-up");
    source.extra = QStringLiteral("{\"scMaxEachPostBytes\":12345}");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.contains(QStringLiteral("type=xhttp")));
    QVERIFY(url.contains(QStringLiteral("mode=stream-up")));
    QVERIFY(url.contains(QStringLiteral("extra=")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.network, QStringLiteral("xhttp"));
    QCOMPARE(parsed.requestHost, source.requestHost);
    QCOMPARE(parsed.path, source.path);
    QCOMPARE(parsed.headerType, source.headerType);
    QCOMPARE(
        QJsonDocument::fromJson(parsed.extra.toUtf8()).object(),
        QJsonDocument::fromJson(source.extra.toUtf8()).object());
}

void ShareUrlTests::buildAndParseFinalmaskRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::VLESS;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("none");
    source.streamSecurity = QStringLiteral("tls");
    source.network = QStringLiteral("kcp");
    source.headerType = QStringLiteral("wireguard");
    source.path = QStringLiteral("seed-value");
    source.finalmask =
        QStringLiteral("{\"udp\":[{\"type\":\"mkcp-aes128gcm\",\"settings\":{\"password\":\"seed-value\"}}]}");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.contains(QStringLiteral("fm=")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(
        QJsonDocument::fromJson(parsed.finalmask.toUtf8()).object(),
        QJsonDocument::fromJson(source.finalmask.toUtf8()).object());
}

void ShareUrlTests::buildAndParseCertShaRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::VLESS;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("none");
    source.streamSecurity = QStringLiteral("tls");
    source.certSha = QStringLiteral("sha256/base64-one,sha256/base64-two");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.contains(QStringLiteral("pcs=")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.certSha, source.certSha);
}

void ShareUrlTests::buildAndParseAnytlsRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::AnyTLS;
    source.coreType = CoreType::SingBox;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("hunter2");
    source.sni = QStringLiteral("cdn.example.com");
    source.allowInsecure = QStringLiteral("1");
    source.alpn = QStringList{QStringLiteral("h2"), QStringLiteral("http/1.1")};
    source.fingerprint = QStringLiteral("chrome");
    source.remarks = QStringLiteral("anytls-node");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.startsWith(QStringLiteral("anytls://")));
    QVERIFY(url.contains(QStringLiteral("sni=cdn.example.com")));
    QVERIFY(url.contains(QStringLiteral("insecure=1")));
    QVERIFY(url.contains(QStringLiteral("alpn=h2%2Chttp%2F1.1")));
    QVERIFY(url.contains(QStringLiteral("fp=chrome")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::AnyTLS);
    QCOMPARE(parsed.address, source.address);
    QCOMPARE(parsed.port, source.port);
    QCOMPARE(parsed.id, source.id);
    QCOMPARE(parsed.sni, source.sni);
    QCOMPARE(parsed.allowInsecure, QStringLiteral("1"));
    QCOMPARE(parsed.alpn, source.alpn);
    QCOMPARE(parsed.fingerprint, source.fingerprint);
    QCOMPARE(parsed.remarks, source.remarks);
}

void ShareUrlTests::buildAndParseNaiveHttpsRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::Naive;
    source.coreType = CoreType::SingBox;
    source.address = QStringLiteral("naive.example.com");
    source.port = 443;
    source.username = QStringLiteral("alice");
    source.id = QStringLiteral("s3cret:/pass");
    source.sni = QStringLiteral("naive.example.com");
    source.alpn = QStringList{QStringLiteral("h2")};
    source.insecureConcurrency = 2;
    source.remarks = QStringLiteral("naive-https");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.startsWith(QStringLiteral("naive+https://")));
    QVERIFY(url.contains(QStringLiteral("insecure-concurrency=2")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::Naive);
    QCOMPARE(parsed.address, source.address);
    QCOMPARE(parsed.port, source.port);
    QCOMPARE(parsed.username, source.username);
    QCOMPARE(parsed.id, source.id);
    QCOMPARE(parsed.sni, source.sni);
    QCOMPARE(parsed.alpn, source.alpn);
    QCOMPARE(parsed.insecureConcurrency, source.insecureConcurrency);
    QCOMPARE(parsed.naiveQuic, false);
    QCOMPARE(parsed.remarks, source.remarks);
}

void ShareUrlTests::buildAndParseNaiveQuicRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::Naive;
    source.coreType = CoreType::SingBox;
    source.address = QStringLiteral("naive.example.com");
    source.port = 443;
    source.username = QStringLiteral("bob");
    source.id = QStringLiteral("password");
    source.naiveQuic = true;
    source.congestionControl = QStringLiteral("bbr");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.startsWith(QStringLiteral("naive+quic://")));
    QVERIFY(url.contains(QStringLiteral("congestion_control=bbr")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::Naive);
    QCOMPARE(parsed.naiveQuic, true);
    QCOMPARE(parsed.username, source.username);
    QCOMPARE(parsed.id, source.id);
    QCOMPARE(parsed.congestionControl, source.congestionControl);
}

void ShareUrlTests::buildAndParseTrojanEscapesUserInfo()
{
    VmessItem source;
    source.configType = ConfigType::Trojan;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("p@ss:/?#");
    source.remarks = QStringLiteral("trojan-special");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.startsWith(QStringLiteral("trojan://p%40ss%3A%2F%3F%23@example.com:443")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::Trojan);
    QCOMPARE(parsed.id, source.id);
    QCOMPARE(parsed.address, source.address);
    QCOMPARE(parsed.port, source.port);
    QCOMPARE(parsed.remarks, source.remarks);
}

void ShareUrlTests::buildAndParseTuicEscapesUserInfo()
{
    VmessItem source;
    source.configType = ConfigType::TUIC;
    source.address = QStringLiteral("tuic.example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("p@ss:/?#");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.startsWith(QStringLiteral("tuic://11111111-1111-1111-1111-111111111111:p%40ss%3A%2F%3F%23@tuic.example.com:443")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::TUIC);
    QCOMPARE(parsed.id, source.id);
    QCOMPARE(parsed.security, source.security);
    QCOMPARE(parsed.address, source.address);
    QCOMPARE(parsed.port, source.port);
}

void ShareUrlTests::parseRejectsOutOfRangePort()
{
    bool ok = true;
    const VmessItem parsed = ShareUrlParser::parse(
        QStringLiteral("trojan://password@example.com:65536#bad-port"),
        &ok);

    QVERIFY(!ok);
    Q_UNUSED(parsed);
}

void ShareUrlTests::parseHysteria2PinSha256IntoCertSha()
{
    const QString url =
        QStringLiteral("hysteria2://password@example.com:443?sni=cdn.example.com&pinSHA256=sha256%2Fbase64-pin#hy2");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::Hysteria2);
    QCOMPARE(parsed.certSha, QStringLiteral("sha256/base64-pin"));
    QVERIFY(parsed.fingerprint.isEmpty());
}

void ShareUrlTests::parseVlessTlsVisionShareUrl()
{
    const QString url =
        QStringLiteral("vless://79b78774-0ccd-4792-aa2d-189887dd987f@hk1.ap221.com:443?"
                       "encryption=none&flow=xtls-rprx-vision&security=tls&sni=hk.ap221.com&"
                       "type=tcp&headerType=none#HK");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::VLESS);
    QCOMPARE(parsed.address, QStringLiteral("hk1.ap221.com"));
    QCOMPARE(parsed.port, 443);
    QCOMPARE(parsed.id, QStringLiteral("79b78774-0ccd-4792-aa2d-189887dd987f"));
    QCOMPARE(parsed.security, QStringLiteral("none"));
    QCOMPARE(parsed.flow, QStringLiteral("xtls-rprx-vision"));
    QCOMPARE(parsed.streamSecurity, QStringLiteral("tls"));
    QCOMPARE(parsed.sni, QStringLiteral("hk.ap221.com"));
    QCOMPARE(parsed.network, QStringLiteral("tcp"));
    QCOMPARE(parsed.headerType, QStringLiteral("none"));
}

void ShareUrlTests::parseVlessRealityVisionShareUrl()
{
    const QString url =
        QStringLiteral("vless://a4caef32-090f-4f17-b7fa-2808f205620d@64.49.46.225:57502?"
                       "encryption=none&flow=xtls-rprx-vision&security=reality&sni=www.tesla.com&"
                       "fp=firefox&pbk=IOAu39FSiRk_VX4sfzCVZorWhUY0ikftPdHqcmNoY1c&"
                       "sid=ebfeeffb94d5bd5d&type=tcp&headerType=none#HK");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::VLESS);
    QCOMPARE(parsed.address, QStringLiteral("64.49.46.225"));
    QCOMPARE(parsed.port, 57502);
    QCOMPARE(parsed.id, QStringLiteral("a4caef32-090f-4f17-b7fa-2808f205620d"));
    QCOMPARE(parsed.security, QStringLiteral("none"));
    QCOMPARE(parsed.flow, QStringLiteral("xtls-rprx-vision"));
    QCOMPARE(parsed.streamSecurity, QStringLiteral("reality"));
    QCOMPARE(parsed.sni, QStringLiteral("www.tesla.com"));
    QCOMPARE(parsed.fingerprint, QStringLiteral("firefox"));
    QCOMPARE(parsed.publicKey, QStringLiteral("IOAu39FSiRk_VX4sfzCVZorWhUY0ikftPdHqcmNoY1c"));
    QCOMPARE(parsed.shortId, QStringLiteral("ebfeeffb94d5bd5d"));
    QCOMPARE(parsed.network, QStringLiteral("tcp"));
    QCOMPARE(parsed.headerType, QStringLiteral("none"));
}

void ShareUrlTests::parseVlessRealityVisionShareUrlKeepsPacketEncoding()
{
    const QString url =
        QStringLiteral("vless://83e96db9-34ec-4d85-ac13-e37e44006aa8@82.152.166.143:18961?"
                       "encryption=none&flow=xtls-rprx-vision&security=reality&sni=www.icloud.com&"
                       "fp=safari&pbk=JAid5nKTI2w_Xpqszf80hSsYFn4sUkTAorIkd_PDlmw&sid=9f&"
                       "packetEncoding=xudp#HK-01%F0%9F%87%AD%F0%9F%87%B0");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::VLESS);
    QCOMPARE(parsed.flow, QStringLiteral("xtls-rprx-vision"));
    QCOMPARE(parsed.streamSecurity, QStringLiteral("reality"));
    QCOMPARE(parsed.sni, QStringLiteral("www.icloud.com"));
    QCOMPARE(parsed.fingerprint, QStringLiteral("safari"));
    QCOMPARE(parsed.publicKey, QStringLiteral("JAid5nKTI2w_Xpqszf80hSsYFn4sUkTAorIkd_PDlmw"));
    QCOMPARE(parsed.shortId, QStringLiteral("9f"));
    QCOMPARE(parsed.packetEncoding, QStringLiteral("xudp"));
    QCOMPARE(parsed.remarks, QUrl::fromPercentEncoding(QByteArrayLiteral("HK-01%F0%9F%87%AD%F0%9F%87%B0")));
}

void ShareUrlTests::parseAnytlsShareUrlKeepsIdleSessionSettings()
{
    const QString url =
        QStringLiteral("anytls://83e96db9-34ec-4d85-ac13-e37e44006aa8@82.152.166.143:18963?"
                       "idle_session_check_interval=30s&idle_session_timeout=30s&min_idle_session=5&"
                       "security=tls&sni=hycs3.ipl.cc.cd&allowInsecure=true#HK-02");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::AnyTLS);
    QCOMPARE(parsed.address, QStringLiteral("82.152.166.143"));
    QCOMPARE(parsed.port, 18963);
    QCOMPARE(parsed.id, QStringLiteral("83e96db9-34ec-4d85-ac13-e37e44006aa8"));
    QCOMPARE(parsed.streamSecurity, QStringLiteral("tls"));
    QCOMPARE(parsed.sni, QStringLiteral("hycs3.ipl.cc.cd"));
    QCOMPARE(parsed.allowInsecure, QStringLiteral("true"));
    QCOMPARE(parsed.idleSessionCheckInterval, QStringLiteral("30s"));
    QCOMPARE(parsed.idleSessionTimeout, QStringLiteral("30s"));
    QCOMPARE(parsed.minIdleSession, QStringLiteral("5"));
}

void ShareUrlTests::parseTrojanShareUrlKeepsAllowInsecure()
{
    const QString url =
        QStringLiteral("trojan://%E5%8D%87%E7%BA%A7%E8%AE%A2%E9%98%85@z-eu1.mfa.cat:10030?"
                       "security=tls&sni=www.cloudflare.com&allowInsecure=true#trojan");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::Trojan);
    QCOMPARE(parsed.id, QUrl::fromPercentEncoding(QByteArrayLiteral("%E5%8D%87%E7%BA%A7%E8%AE%A2%E9%98%85")));
    QCOMPARE(parsed.streamSecurity, QStringLiteral("tls"));
    QCOMPARE(parsed.sni, QStringLiteral("www.cloudflare.com"));
    QCOMPARE(parsed.allowInsecure, QStringLiteral("true"));
}

void ShareUrlTests::buildAndParseRealityMldsa65VerifyRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::VLESS;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("none");
    source.streamSecurity = QStringLiteral("reality");
    source.publicKey = QStringLiteral("reality-public-key");
    source.shortId = QStringLiteral("0123456789abcdef");
    source.mldsa65Verify = QStringLiteral("mldsa65-public-key");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.contains(QStringLiteral("pqv=")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.mldsa65Verify, source.mldsa65Verify);
}

void ShareUrlTests::buildAndParseEchRoundTrips()
{
    VmessItem source;
    source.configType = ConfigType::VLESS;
    source.address = QStringLiteral("example.com");
    source.port = 443;
    source.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    source.security = QStringLiteral("none");
    source.streamSecurity = QStringLiteral("tls");
    source.echConfigList = QStringLiteral("ECHCONFIGBASE64");

    const QString url = ShareUrlBuilder::build(source);

    QVERIFY(url.contains(QStringLiteral("ech=")));

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.echConfigList, source.echConfigList);
}

void ShareUrlTests::parseSocksUrlWithEmptyBase64Credentials()
{
    const QString url = QStringLiteral("socks://Og==@192.168.100.117:1080#local");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::Socks);
    QCOMPARE(parsed.address, QStringLiteral("192.168.100.117"));
    QCOMPARE(parsed.port, 1080);
    QCOMPARE(parsed.remarks, QStringLiteral("local"));
    QVERIFY(parsed.id.isEmpty());
    QVERIFY(parsed.security.isEmpty());
}

void ShareUrlTests::parseLegacyVmessShareUrlKeepsVmessType()
{
    const QString url = QStringLiteral(
        "vmess://eyJhZGQiOiIxNTkuMTMuNDguMzAiLCJhaWQiOiIwIiwiYWxwbiI6IiIsImhvc3QiOiIiLCJpZCI6IjcwMTEzOTliLTU3NDUtNDM5NS04MmMzLWNmYWZlNDg2ZTg2MyIsIm5ldCI6InRjcCIsInBhdGgiOiIiLCJwb3J0IjoiMTM2NjgiLCJwcyI6ImF1MTMiLCJzY3kiOiJhdXRvIiwic25pIjoiIiwidGxzIjoiIiwidHlwZSI6Im5vbmUiLCJ2IjoiMiJ9");

    bool ok = false;
    const VmessItem parsed = ShareUrlParser::parse(url, &ok);

    QVERIFY(ok);
    QCOMPARE(parsed.configType, ConfigType::VMess);
    QCOMPARE(parsed.address, QStringLiteral("159.13.48.30"));
    QCOMPARE(parsed.port, 13668);
    QCOMPARE(parsed.id, QStringLiteral("7011399b-5745-4395-82c3-cfafe486e863"));
    QCOMPARE(parsed.remarks, QStringLiteral("au13"));
}

QTEST_MAIN(ShareUrlTests)

#include "ShareUrlTests.moc"
