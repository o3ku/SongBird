#include <QtTest>

#include <QByteArray>

#include "subscription/SubscriptionContentParser.h"
#include "subscription/CustomConfigTextParser.h"

class SubscriptionParserTests : public QObject {
    Q_OBJECT

private slots:
    // --- SubscriptionContentParser ---
    void subscriptionSingleBase64Vmess();
    void subscriptionMultipleBase64Links();
    void subscriptionRawPlaintextShareUrls();
    void subscriptionSip008JsonObject();
    void subscriptionSip008JsonArray();
    void subscriptionEmptyInput();
    void subscriptionGarbageInput();
    void subscriptionBase64UrlSafeChars();
    void subscriptionBase64MissingPadding();
    void subscriptionSip008SkipsInvalidEntries();
    void subscriptionSip008RejectsOutOfRangePort();
    void subscriptionJsonStringArrayOfShareUrls();
    void subscriptionNestedBase64DecodesRepeatedly();
    void subscriptionSingBoxOutboundsJson();
    void subscriptionSingBoxRejectsOutOfRangePort();
    void subscriptionClashYamlProxies();
    void subscriptionClashRejectsOutOfRangePort();
    void subscriptionClashYamlFlowStyleVlessReality();

    // --- CustomConfigTextParser ---
    void customXrayJson();
    void customSingBoxJson();
    void customEmptyInput();
    void customGarbageInput();
    void customHtmlRejected();
    void customXrayRequiresInboundsOutbounds();
    void customSingBoxRequiresTypeKey();
    void customUnknownJsonReturnsEmpty();
};

// ---------------------------------------------------------------------------
// SubscriptionContentParser tests
// ---------------------------------------------------------------------------

void SubscriptionParserTests::subscriptionSingleBase64Vmess()
{
    // A single vmess:// link, base64-encoded.
    const QString link = QStringLiteral("vmess://eyJ2IjoiMiIsInBzIjoidGVzdC1zZXJ2ZXIiLCJhZGQiOiIxMC4wLjAuMSIsInBvcnQiOiIxMDAwMSIsImlkIjoiMTExMTExMTEtMTExMS0xMTExLTExMTEtMTExMTExMTExMTExIiwiYWlkIjoiMCIsIm5ldCI6InRjcCIsInR5cGUiOiJub25lIiwiaG9zdCI6IiIsInBhdGgiOiIiLCJ0bHMiOiIifQ==");
    const QByteArray raw = link.toUtf8();
    const QByteArray encoded = raw.toBase64();

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(QString::fromUtf8(encoded));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].remarks, QStringLiteral("test-server"));
    QCOMPARE(items[0].address, QStringLiteral("10.0.0.1"));
    QCOMPARE(items[0].port, 10001);
}

void SubscriptionParserTests::subscriptionMultipleBase64Links()
{
    // Two share URLs joined by newlines, then base64-encoded.
    const QString links = QStringLiteral("vmess://eyJ2IjoiMiIsInBzIjoiYSIsICJhZGQiOiIxMC4wLjAuMSIsInBvcnQiOiIxMDAwMSIsImlkIjoiMTExMTExMTEtMTExMS0xMTExLTExMTEtMTExMTExMTExMTExIiwiYWlkIjoiMCIsIm5ldCI6InRjcCIsInR5cGUiOiJub25lIiwiaG9zdCI6IiIsInBhdGgiOiIiLCJ0bHMiOiIifQ==\n")
        + QStringLiteral("trojan://password123@9.8.7.6:443?sni=example.com#trojan-server");
    const QByteArray encoded = links.toUtf8().toBase64();

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(QString::fromUtf8(encoded));
    QCOMPARE(items.size(), 2);

    QCOMPARE(items[0].configType, ConfigType::VMess);
    QCOMPARE(items[0].remarks, QStringLiteral("a"));

    QCOMPARE(items[1].configType, ConfigType::Trojan);
    QCOMPARE(items[1].address, QStringLiteral("9.8.7.6"));
    QCOMPARE(items[1].port, 443);
}

void SubscriptionParserTests::subscriptionRawPlaintextShareUrls()
{
    // Plain vmess:// and trojan:// URLs, no base64 wrapping.
    const QString content = QStringLiteral("vmess://eyJ2IjoiMiIsInBzIjoicGxhaW4iLCJhZGQiOiIxMC4xLjEuMSIsInBvcnQiOiIyMDAwMCIsImlkIjoiMTExMTExMTEtMTExMS0xMTExLTExMTEtMTExMTExMTExMTExIiwiYWlkIjoiMCIsIm5ldCI6InRjcCIsInR5cGUiOiJub25lIiwiaG9zdCI6IiIsInBhdGgiOiIiLCJ0bHMiOiIifQ==\n")
        + QStringLiteral("trojan://mypass@1.2.3.4:8888?sni=t.example.com#my-trojan\n");

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(content);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].remarks, QStringLiteral("plain"));
    QCOMPARE(items[1].configType, ConfigType::Trojan);
    QCOMPARE(items[1].port, 8888);
}

void SubscriptionParserTests::subscriptionSip008JsonObject()
{
    const QString json = QStringLiteral(
        "{"
        "  \"servers\": ["
        "    {"
        "      \"remarks\": \"ss-server-1\","
        "      \"server\": \"1.2.3.4\","
        "      \"server_port\": 8388,"
        "      \"method\": \"aes-256-gcm\","
        "      \"password\": \"s3cret\""
        "    }"
        "  ]"
        "}"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(json);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].configType, ConfigType::Shadowsocks);
    QCOMPARE(items[0].remarks, QStringLiteral("ss-server-1"));
    QCOMPARE(items[0].address, QStringLiteral("1.2.3.4"));
    QCOMPARE(items[0].port, 8388);
    QCOMPARE(items[0].security, QStringLiteral("aes-256-gcm"));
    QCOMPARE(items[0].id, QStringLiteral("s3cret"));
}

void SubscriptionParserTests::subscriptionSip008JsonArray()
{
    const QString json = QStringLiteral(
        "["
        "  {"
        "    \"remarks\": \"ss-a\","
        "    \"server\": \"10.0.0.1\","
        "    \"server_port\": 1000,"
        "    \"method\": \"chacha20-ietf-poly1305\","
        "    \"password\": \"aaa\""
        "  },"
        "  {"
        "    \"remarks\": \"ss-b\","
        "    \"server\": \"10.0.0.2\","
        "    \"server_port\": 2000,"
        "    \"method\": \"aes-128-gcm\","
        "    \"password\": \"bbb\""
        "  }"
        "]"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(json);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].remarks, QStringLiteral("ss-a"));
    QCOMPARE(items[0].port, 1000);
    QCOMPARE(items[1].remarks, QStringLiteral("ss-b"));
    QCOMPARE(items[1].port, 2000);
}

void SubscriptionParserTests::subscriptionEmptyInput()
{
    const QList<VmessItem> items = SubscriptionContentParser::parseMany(QString());
    QVERIFY(items.isEmpty());
}

void SubscriptionParserTests::subscriptionGarbageInput()
{
    const QList<VmessItem> items = SubscriptionContentParser::parseMany(
        QStringLiteral("this is not valid subscription content at all !!!"));
    QVERIFY(items.isEmpty());
}

void SubscriptionParserTests::subscriptionBase64UrlSafeChars()
{
    // Build a base64 payload with url-safe characters (- and _) in the encoded
    // form. We manually craft it by encoding the raw content and swapping chars.
    const QString link = QStringLiteral("vmess://eyJ2IjoiMiIsInBzIjoidXJsLXNhZmUiLCJhZGQiOiIxMC4wLjAuMSIsInBvcnQiOiIxMDAwMSIsImlkIjoiMTExMTExMTEtMTExMS0xMTExLTExMTEtMTExMTExMTExMTExIiwiYWlkIjoiMCIsIm5ldCI6InRjcCIsInR5cGUiOiJub25lIiwiaG9zdCI6IiIsInBhdGgiOiIiLCJ0bHMiOiIifQ==");
    QByteArray encoded = link.toUtf8().toBase64();
    // Introduce URL-safe characters: replace some + with - and / with _
    encoded.replace('+', '-');
    encoded.replace('/', '_');
    // Remove trailing = padding
    while (encoded.endsWith('=')) {
        encoded.chop(1);
    }

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(QString::fromUtf8(encoded));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].remarks, QStringLiteral("url-safe"));
}

void SubscriptionParserTests::subscriptionBase64MissingPadding()
{
    // Same as above, but just strip the padding without swapping chars.
    const QString link = QStringLiteral("vmess://eyJ2IjoiMiIsInBzIjoibm8tcGFkIiwiYWRkIjoiMTAuMC4wLjEiLCJwb3J0IjoiMTAwMDEiLCJpZCI6IjExMTExMTExLTExMTEtMTExMS0xMTExLTExMTExMTExMTExMSIsImFpZCI6IjAiLCJuZXQiOiJ0Y3AiLCJ0eXBlIjoibm9uZSIsImhvc3QiOiIiLCJwYXRoIjoiIiwidGxzIjoiIn0=");
    QByteArray encoded = link.toUtf8().toBase64();
    // Strip padding
    while (encoded.endsWith('=')) {
        encoded.chop(1);
    }

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(QString::fromUtf8(encoded));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].remarks, QStringLiteral("no-pad"));
}

void SubscriptionParserTests::subscriptionSip008SkipsInvalidEntries()
{
    // SIP008 array where one entry is missing required fields and one is not an object.
    const QString json = QStringLiteral(
        "["
        "  {"
        "    \"remarks\": \"valid\","
        "    \"server\": \"10.0.0.1\","
        "    \"server_port\": 1234,"
        "    \"method\": \"aes-256-gcm\","
        "    \"password\": \"pw\""
        "  },"
        "  \"not-an-object\","
        "  {"
        "    \"remarks\": \"no-server\""
        "  }"
        "]"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(json);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].remarks, QStringLiteral("valid"));
}

void SubscriptionParserTests::subscriptionSip008RejectsOutOfRangePort()
{
    const QString json = QStringLiteral(
        "["
        "  {"
        "    \"remarks\": \"bad-port\","
        "    \"server\": \"10.0.0.1\","
        "    \"server_port\": 65536,"
        "    \"method\": \"aes-256-gcm\","
        "    \"password\": \"pw\""
        "  }"
        "]"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(json);
    QVERIFY(items.isEmpty());
}

void SubscriptionParserTests::subscriptionJsonStringArrayOfShareUrls()
{
    const QString content = QStringLiteral(
        "["
        "\"vmess://eyJ2IjoiMiIsInBzIjoiYXJyYXktdm1lc3MiLCJhZGQiOiIxMC4xMC4wLjEiLCJwb3J0IjoiMTAwMDEiLCJpZCI6IjExMTExMTExLTExMTEtMTExMS0xMTExLTExMTExMTExMTExMSIsImFpZCI6IjAiLCJuZXQiOiJ3cyIsInR5cGUiOiJub25lIiwiaG9zdCI6ImNkbi5leGFtcGxlLmNvbSIsInBhdGgiOiIvd3MiLCJ0bHMiOiJ0bHMifQ==\","
        "\"trojan://pass123@2.3.4.5:443?sni=t.example.com#array-trojan\""
        "]"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(content);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].configType, ConfigType::VMess);
    QCOMPARE(items[0].remarks, QStringLiteral("array-vmess"));
    QCOMPARE(items[1].configType, ConfigType::Trojan);
    QCOMPARE(items[1].remarks, QStringLiteral("array-trojan"));
}

void SubscriptionParserTests::subscriptionNestedBase64DecodesRepeatedly()
{
    const QString link = QStringLiteral(
        "vmess://eyJ2IjoiMiIsInBzIjoibXVsdGktYjY0IiwiYWRkIjoiMTAuMC4wLjUiLCJwb3J0IjoiMTAwMDEiLCJpZCI6IjExMTExMTExLTExMTEtMTExMS0xMTExLTExMTExMTExMTExMSIsImFpZCI6IjAiLCJuZXQiOiJ0Y3AiLCJ0eXBlIjoibm9uZSIsImhvc3QiOiIiLCJwYXRoIjoiIiwidGxzIjoiIn0=");
    const QByteArray once = link.toUtf8().toBase64();
    const QByteArray twice = once.toBase64();

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(QString::fromUtf8(twice));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].remarks, QStringLiteral("multi-b64"));
}

void SubscriptionParserTests::subscriptionSingBoxOutboundsJson()
{
    const QString json = QStringLiteral(
        "{"
        "\"outbounds\":["
        "  {"
        "    \"type\":\"vmess\","
        "    \"tag\":\"sb-vmess\","
        "    \"server\":\"vmess.example.com\","
        "    \"server_port\":443,"
        "    \"uuid\":\"11111111-1111-1111-1111-111111111111\","
        "    \"security\":\"auto\","
        "    \"transport\":{\"type\":\"ws\",\"path\":\"/ws\",\"headers\":{\"Host\":\"cdn.example.com\"}},"
        "    \"tls\":{\"enabled\":true,\"server_name\":\"tls.example.com\"}"
        "  },"
        "  {"
        "    \"type\":\"trojan\","
        "    \"tag\":\"sb-trojan\","
        "    \"server\":\"trojan.example.com\","
        "    \"server_port\":443,"
        "    \"password\":\"trojan-pass\""
        "  }"
        "]"
        "}"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(json);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].configType, ConfigType::VMess);
    QCOMPARE(items[0].remarks, QStringLiteral("sb-vmess"));
    QCOMPARE(items[0].address, QStringLiteral("vmess.example.com"));
    QCOMPARE(items[1].configType, ConfigType::Trojan);
    QCOMPARE(items[1].remarks, QStringLiteral("sb-trojan"));
}

void SubscriptionParserTests::subscriptionSingBoxRejectsOutOfRangePort()
{
    const QString json = QStringLiteral(
        "{"
        "\"outbounds\":["
        "  {"
        "    \"type\":\"trojan\","
        "    \"tag\":\"bad-port\","
        "    \"server\":\"trojan.example.com\","
        "    \"server_port\":65536,"
        "    \"password\":\"trojan-pass\""
        "  }"
        "]"
        "}"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(json);
    QVERIFY(items.isEmpty());
}

void SubscriptionParserTests::subscriptionClashYamlProxies()
{
    const QString yaml = QStringLiteral(
        "mixed-port: 7890\n"
        "proxies:\n"
        "  - name: clash-vmess\n"
        "    type: vmess\n"
        "    server: 1.2.3.4\n"
        "    port: 443\n"
        "    uuid: 11111111-1111-1111-1111-111111111111\n"
        "    alterId: 0\n"
        "    cipher: auto\n"
        "    tls: true\n"
        "    network: ws\n"
        "    ws-path: /ws\n"
        "  - name: clash-ss\n"
        "    type: ss\n"
        "    server: 5.6.7.8\n"
        "    port: 8388\n"
        "    cipher: aes-256-gcm\n"
        "    password: secret\n"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(yaml);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].configType, ConfigType::VMess);
    QCOMPARE(items[0].remarks, QStringLiteral("clash-vmess"));
    QCOMPARE(items[0].path, QStringLiteral("/ws"));
    QCOMPARE(items[1].configType, ConfigType::Shadowsocks);
    QCOMPARE(items[1].remarks, QStringLiteral("clash-ss"));
}

void SubscriptionParserTests::subscriptionClashRejectsOutOfRangePort()
{
    const QString yaml = QStringLiteral(
        "proxies:\n"
        "  - name: bad-port\n"
        "    type: trojan\n"
        "    server: 1.2.3.4\n"
        "    port: 65536\n"
        "    password: secret\n"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(yaml);
    QVERIFY(items.isEmpty());
}

void SubscriptionParserTests::subscriptionClashYamlFlowStyleVlessReality()
{
    const QString yaml = QStringLiteral(
        "proxies:\n"
        "  - { name: HK丨香港1, type: vless, server: f3a9k5d8.gabuit.com, port: 48601, uuid: BAB25126-A72C-4156-818F-8E32738EDE72, udp: true, cipher: auto, tls: true, flow: xtls-rprx-vision, client-fingerprint: chrome, servername: itunes.apple.com, reality-opts: { public-key: Nwa_ddI7yYai9YMz67iFL5FirgFwO9lvvxYM0qozdEQ, short-id: 7b2658c87841 } }\n"
    );

    const QList<VmessItem> items = SubscriptionContentParser::parseMany(yaml);
    QCOMPARE(items.size(), 1);
    QCOMPARE(items[0].configType, ConfigType::VLESS);
    QCOMPARE(items[0].remarks, QStringLiteral("HK丨香港1"));
    QCOMPARE(items[0].address, QStringLiteral("f3a9k5d8.gabuit.com"));
    QCOMPARE(items[0].port, 48601);
    QCOMPARE(items[0].id, QStringLiteral("BAB25126-A72C-4156-818F-8E32738EDE72"));
    QCOMPARE(items[0].flow, QStringLiteral("xtls-rprx-vision"));
    QCOMPARE(items[0].streamSecurity, QStringLiteral("reality"));
    QCOMPARE(items[0].fingerprint, QStringLiteral("chrome"));
    QCOMPARE(items[0].sni, QStringLiteral("itunes.apple.com"));
    QCOMPARE(items[0].publicKey, QStringLiteral("Nwa_ddI7yYai9YMz67iFL5FirgFwO9lvvxYM0qozdEQ"));
    QCOMPARE(items[0].shortId, QStringLiteral("7b2658c87841"));
}

// ---------------------------------------------------------------------------
// CustomConfigTextParser tests
// ---------------------------------------------------------------------------

void SubscriptionParserTests::customXrayJson()
{
    const QString json = QStringLiteral(
        "{"
        "  \"inbounds\": [{\"protocol\": \"socks\",\"listen\": \"127.0.0.1\",\"port\": 10808}],"
        "  \"outbounds\": [{\"protocol\": \"vmess\",\"settings\": {}}]"
        "}"
    );

    bool ok = false;
    QString ext;
    const VmessItem item = CustomConfigTextParser::parse(json, &ext, &ok);

    QVERIFY(ok);
    QCOMPARE(item.configType, ConfigType::Custom);
    QCOMPARE(item.coreType, CoreType::Xray);
    QCOMPARE(item.remarks, QStringLiteral("xray_custom"));
    QCOMPARE(ext, QStringLiteral("json"));
}

void SubscriptionParserTests::customSingBoxJson()
{
    const QString json = QStringLiteral(
        "{"
        "  \"inbounds\": [{\"type\": \"tun\",\"tag\": \"tun-in\"}],"
        "  \"outbounds\": [{\"type\": \"vmess\",\"tag\": \"proxy\"}]"
        "}"
    );

    bool ok = false;
    const VmessItem item = CustomConfigTextParser::parse(json, nullptr, &ok);

    QVERIFY(ok);
    QCOMPARE(item.configType, ConfigType::Custom);
    QCOMPARE(item.coreType, CoreType::SingBox);
    QCOMPARE(item.remarks, QStringLiteral("singbox_custom"));
}

void SubscriptionParserTests::customEmptyInput()
{
    bool ok = true;
    const VmessItem item = CustomConfigTextParser::parse(QString(), nullptr, &ok);
    QVERIFY(!ok);
}

void SubscriptionParserTests::customGarbageInput()
{
    bool ok = true;
    const VmessItem item = CustomConfigTextParser::parse(
        QStringLiteral("not valid config text at all"), nullptr, &ok);
    QVERIFY(!ok);
}

void SubscriptionParserTests::customHtmlRejected()
{
    // HTML that vaguely resembles a config should still be rejected.
    const QString html = QStringLiteral(
        "<html><body>"
        "<p>server: 1.2.3.4</p>"
        "<p>up: 100</p>"
        "<p>down: 200</p>"
        "<p>listen: 127.0.0.1</p>"
        "</body></html>"
    );

    bool ok = true;
    const VmessItem item = CustomConfigTextParser::parse(html, nullptr, &ok);
    QVERIFY(!ok);

}

void SubscriptionParserTests::customXrayRequiresInboundsOutbounds()
{
    // JSON object with "protocol" in outbound but missing inbounds -> not Xray.
    const QString json = QStringLiteral(
        "{"
        "  \"outbounds\": [{\"protocol\": \"vmess\",\"settings\": {}}]"
        "}"
    );

    bool ok = true;
    const VmessItem item = CustomConfigTextParser::parse(json, nullptr, &ok);
    QVERIFY(!ok);
}

void SubscriptionParserTests::customSingBoxRequiresTypeKey()
{
    // JSON with inbounds+outbounds but outbound has "protocol" instead of "type" -> Xray, not sing-box.
    const QString json = QStringLiteral(
        "{"
        "  \"inbounds\": [{\"protocol\": \"socks\",\"listen\": \"127.0.0.1\",\"port\": 10808}],"
        "  \"outbounds\": [{\"protocol\": \"vmess\",\"settings\": {}}]"
        "}"
    );

    bool ok = false;
    const VmessItem item = CustomConfigTextParser::parse(json, nullptr, &ok);
    QVERIFY(ok);
    // Detected as Xray because outbound has "protocol", not "type".
    QCOMPARE(item.coreType, CoreType::Xray);
}

void SubscriptionParserTests::customUnknownJsonReturnsEmpty()
{
    // Valid JSON object but does not match any known config format.
    const QString json = QStringLiteral(
        "{"
        "  \"foo\": \"bar\","
        "  \"baz\": 42"
        "}"
    );

    bool ok = true;
    const VmessItem item = CustomConfigTextParser::parse(json, nullptr, &ok);
    QVERIFY(!ok);
}

QTEST_MAIN(SubscriptionParserTests)
#include "SubscriptionParserTests.moc"
