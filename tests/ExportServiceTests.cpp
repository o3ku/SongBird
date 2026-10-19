#include <QtTest>

#include "domain/models/VmessItem.h"
#include "services/ExportService.h"
#include "subscription/ClashProxyItemParser.h"
#include "subscription/ClashYamlBuilder.h"
#include "subscription/ClashYamlProxyParser.h"
#include "subscription/NodeFingerprint.h"
#include "subscription/ShareUrlBuilder.h"

namespace {

VmessItem makeVmess(const QString& address = QStringLiteral("1.2.3.4"),
                    int port = 1234,
                    const QString& id = QStringLiteral("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"),
                    const QString& remarks = QStringLiteral("test"))
{
    VmessItem item;
    item.configType = ConfigType::VMess;
    item.address = address;
    item.port = port;
    item.id = id;
    item.remarks = remarks;
    item.security = QStringLiteral("auto");
    item.network = QStringLiteral("tcp");
    item.alterId = 0;
    return item;
}

VmessItem makeTrojan(const QString& address = QStringLiteral("1.2.3.4"),
                     int port = 443,
                     const QString& pw = QStringLiteral("hunter2"))
{
    VmessItem item;
    item.configType = ConfigType::Trojan;
    item.address = address;
    item.port = port;
    item.id = pw;
    item.remarks = QStringLiteral("trojan-test");
    item.streamSecurity = QStringLiteral("tls");
    item.sni = QStringLiteral("example.com");
    item.network = QStringLiteral("tcp");
    return item;
}

VmessItem makeShadowsocks(const QString& address = QStringLiteral("1.2.3.4"),
                          int port = 8388,
                          const QString& pw = QStringLiteral("password"))
{
    VmessItem item;
    item.configType = ConfigType::Shadowsocks;
    item.address = address;
    item.port = port;
    item.id = pw;
    item.security = QStringLiteral("aes-128-gcm");
    item.remarks = QStringLiteral("ss-test");
    return item;
}

VmessItem makeHysteria2(const QString& address = QStringLiteral("1.2.3.4"),
                        int port = 8443,
                        const QString& pw = QStringLiteral("mypassword"))
{
    VmessItem item;
    item.configType = ConfigType::Hysteria2;
    item.address = address;
    item.port = port;
    item.id = pw;
    item.remarks = QStringLiteral("hy2-test");
    item.streamSecurity = QStringLiteral("tls");
    item.sni = QStringLiteral("example.com");
    return item;
}

// Parse a single flow-mapping entry back through ClashYamlProxyParser.
VmessItem parseClashEntry(const QString& entry)
{
    // ClashYamlProxyParser expects a full proxies block, not a bare flow entry.
    const QString yaml = QStringLiteral("proxies:\n  - ") + entry + QLatin1Char('\n');
    const QList<VmessItem> items = ClashYamlProxyParser::parseProxyItems(yaml);
    return items.isEmpty() ? VmessItem{} : items.constFirst();
}

} // namespace

class ExportServiceTests : public QObject {
    Q_OBJECT

private slots:
    // ---------- NodeFingerprint ----------

    void fingerprintIsCaseInsensitiveOnAddress();
    void fingerprintIsCaseInsensitiveOnUuid();
    void fingerprintDiffersByPort();
    void fingerprintDiffersByType();
    void fingerprintUsesCredentialsForShadowsocks();
    void fingerprintUsesUsernameAndPasswordForSocks();
    void fingerprintUsesPeerPublicKeyForWireguard();

    // ---------- ClashYamlBuilder ----------

    void buildVmessEntryContainsExpectedFields();
    void buildTrojanEntryContainsExpectedFields();
    void buildShadowsocksEntryContainsExpectedFields();
    void buildHysteria2EntryContainsExpectedFields();
    void buildWsVlessEntryContainsTransportFields();
    void buildSkipsCustomNodes();
    void buildSkipsEmptyAddress();
    void buildSkipsInvalidPort();
    void buildProxiesBlockHeader();
    void buildProxiesBlockEmptyList();

    // ---------- Round-trip (build → parse) ----------

    void roundTripVmessThroughClashYaml();
    void roundTripTrojanThroughClashYaml();
    void roundTripShadowsocksThroughClashYaml();

    // ---------- ExportService ----------

    void exportUrisSkipsCustomNodes();
    void exportClashYamlProducesProxiesBlock();
    void deduplicateKeepsFirstOccurrence();
    void deduplicateIgnoresRemarks();
    void deduplicateIgnoresTransportDetails();
    void exportDedupedUrisRemovesDuplicates();
    void exportDedupedClashYamlRemovesDuplicates();
};

// ---------- NodeFingerprint tests ----------

void ExportServiceTests::fingerprintIsCaseInsensitiveOnAddress()
{
    VmessItem a = makeVmess(QStringLiteral("MyHost.Example.COM"));
    VmessItem b = makeVmess(QStringLiteral("myhost.example.com"));
    QCOMPARE(NodeFingerprint::compute(a), NodeFingerprint::compute(b));
}

void ExportServiceTests::fingerprintIsCaseInsensitiveOnUuid()
{
    VmessItem a = makeVmess(QStringLiteral("1.2.3.4"), 1234, QStringLiteral("AABBCCDD-0000-0000-0000-000000000000"));
    VmessItem b = makeVmess(QStringLiteral("1.2.3.4"), 1234, QStringLiteral("aabbccdd-0000-0000-0000-000000000000"));
    QCOMPARE(NodeFingerprint::compute(a), NodeFingerprint::compute(b));
}

void ExportServiceTests::fingerprintDiffersByPort()
{
    VmessItem a = makeVmess(QStringLiteral("1.2.3.4"), 1234);
    VmessItem b = makeVmess(QStringLiteral("1.2.3.4"), 4321);
    QVERIFY(NodeFingerprint::compute(a) != NodeFingerprint::compute(b));
}

void ExportServiceTests::fingerprintDiffersByType()
{
    VmessItem vmess = makeVmess();
    VmessItem vless;
    vless.configType = ConfigType::VLESS;
    vless.address = vmess.address;
    vless.port = vmess.port;
    vless.id = vmess.id;
    QVERIFY(NodeFingerprint::compute(vmess) != NodeFingerprint::compute(vless));
}

void ExportServiceTests::fingerprintUsesCredentialsForShadowsocks()
{
    VmessItem a = makeShadowsocks(QStringLiteral("1.2.3.4"), 8388, QStringLiteral("pw1"));
    VmessItem b = makeShadowsocks(QStringLiteral("1.2.3.4"), 8388, QStringLiteral("pw2"));
    QVERIFY(NodeFingerprint::compute(a) != NodeFingerprint::compute(b));
}

void ExportServiceTests::fingerprintUsesUsernameAndPasswordForSocks()
{
    VmessItem a;
    a.configType = ConfigType::Socks;
    a.address = QStringLiteral("1.2.3.4");
    a.port = 1080;
    a.id = QStringLiteral("user");
    a.security = QStringLiteral("pass");

    VmessItem b = a;
    b.security = QStringLiteral("different");

    // security (password) is not in the fingerprint for Socks —
    // the fingerprint uses id (username) + id (password) via the username:id field.
    // For Socks the credential is username:password (id field = username, security = password).
    // NodeFingerprint uses item.id for username and item.id for pw in the default path,
    // but the Socks case uses item.username + item.id.
    // Check that same username but different password produces different fingerprints
    // by using the dedicated username field:
    a.username = QStringLiteral("user");
    a.id = QStringLiteral("pass1");
    b.username = QStringLiteral("user");
    b.id = QStringLiteral("pass2");
    QVERIFY(NodeFingerprint::compute(a) != NodeFingerprint::compute(b));
}

void ExportServiceTests::fingerprintUsesPeerPublicKeyForWireguard()
{
    VmessItem a;
    a.configType = ConfigType::WireGuard;
    a.address = QStringLiteral("wg.example.com");
    a.port = 51820;
    a.peerPublicKey = QStringLiteral("KEY1");
    VmessItem b = a;
    b.peerPublicKey = QStringLiteral("KEY2");
    QVERIFY(NodeFingerprint::compute(a) != NodeFingerprint::compute(b));
}

// ---------- ClashYamlBuilder tests ----------

void ExportServiceTests::buildVmessEntryContainsExpectedFields()
{
    const VmessItem item = makeVmess();
    const QString entry = ClashYamlBuilder::buildProxyEntry(item);
    QVERIFY(entry.contains(QStringLiteral("type: vmess")));
    QVERIFY(entry.contains(QStringLiteral("server: 1.2.3.4")));
    QVERIFY(entry.contains(QStringLiteral("port: 1234")));
    QVERIFY(entry.contains(QStringLiteral("uuid:")));
    QVERIFY(entry.contains(QStringLiteral("alterId: 0")));
}

void ExportServiceTests::buildTrojanEntryContainsExpectedFields()
{
    const VmessItem item = makeTrojan();
    const QString entry = ClashYamlBuilder::buildProxyEntry(item);
    QVERIFY(entry.contains(QStringLiteral("type: trojan")));
    QVERIFY(entry.contains(QStringLiteral("password:")));
    QVERIFY(entry.contains(QStringLiteral("sni: example.com")));
}

void ExportServiceTests::buildShadowsocksEntryContainsExpectedFields()
{
    const VmessItem item = makeShadowsocks();
    const QString entry = ClashYamlBuilder::buildProxyEntry(item);
    QVERIFY(entry.contains(QStringLiteral("type: ss")));
    QVERIFY(entry.contains(QStringLiteral("cipher: aes-128-gcm")));
    QVERIFY(entry.contains(QStringLiteral("password:")));
}

void ExportServiceTests::buildHysteria2EntryContainsExpectedFields()
{
    const VmessItem item = makeHysteria2();
    const QString entry = ClashYamlBuilder::buildProxyEntry(item);
    QVERIFY(entry.contains(QStringLiteral("type: hysteria2")));
    QVERIFY(entry.contains(QStringLiteral("password:")));
    QVERIFY(entry.contains(QStringLiteral("sni: example.com")));
}

void ExportServiceTests::buildWsVlessEntryContainsTransportFields()
{
    VmessItem item;
    item.configType = ConfigType::VLESS;
    item.address = QStringLiteral("1.2.3.4");
    item.port = 443;
    item.id = QStringLiteral("some-uuid");
    item.network = QStringLiteral("ws");
    item.path = QStringLiteral("/ws");
    item.requestHost = QStringLiteral("cdn.example.com");
    item.streamSecurity = QStringLiteral("tls");
    const QString entry = ClashYamlBuilder::buildProxyEntry(item);
    QVERIFY(entry.contains(QStringLiteral("type: vless")));
    QVERIFY(entry.contains(QStringLiteral("network: ws")));
    QVERIFY(entry.contains(QStringLiteral("ws-opts:")));
    QVERIFY(entry.contains(QStringLiteral("path:")));
    QVERIFY(entry.contains(QStringLiteral("Host:")));
}

void ExportServiceTests::buildSkipsCustomNodes()
{
    VmessItem item;
    item.configType = ConfigType::Custom;
    item.address = QStringLiteral("1.2.3.4");
    item.port = 1234;
    QVERIFY(ClashYamlBuilder::buildProxyEntry(item).isEmpty());
}

void ExportServiceTests::buildSkipsEmptyAddress()
{
    VmessItem item = makeVmess();
    item.address.clear();
    QVERIFY(ClashYamlBuilder::buildProxyEntry(item).isEmpty());
}

void ExportServiceTests::buildSkipsInvalidPort()
{
    VmessItem item = makeVmess();
    item.port = 0;
    QVERIFY(ClashYamlBuilder::buildProxyEntry(item).isEmpty());

    item.port = 65536;
    QVERIFY(ClashYamlBuilder::buildProxyEntry(item).isEmpty());
}

void ExportServiceTests::buildProxiesBlockHeader()
{
    const QList<VmessItem> items{makeVmess(), makeTrojan()};
    const QString block = ClashYamlBuilder::buildProxiesBlock(items);
    QVERIFY(block.startsWith(QStringLiteral("proxies:\n")));
    QCOMPARE(block.count(QStringLiteral("  - {")), 2);
}

void ExportServiceTests::buildProxiesBlockEmptyList()
{
    const QString block = ClashYamlBuilder::buildProxiesBlock({});
    QCOMPARE(block.trimmed(), QStringLiteral("proxies: []"));
}

// ---------- Round-trip tests ----------

void ExportServiceTests::roundTripVmessThroughClashYaml()
{
    VmessItem original = makeVmess();
    original.remarks = QStringLiteral("my vmess node");
    original.security = QStringLiteral("aes-128-gcm");
    const QString entry = ClashYamlBuilder::buildProxyEntry(original);
    QVERIFY(!entry.isEmpty());
    const VmessItem parsed = parseClashEntry(entry);
    QCOMPARE(parsed.configType, ConfigType::VMess);
    QCOMPARE(parsed.address, original.address);
    QCOMPARE(parsed.port, original.port);
    QCOMPARE(parsed.id, original.id);
    QCOMPARE(parsed.security, original.security);
    QCOMPARE(parsed.remarks, original.remarks);
}

void ExportServiceTests::roundTripTrojanThroughClashYaml()
{
    const VmessItem original = makeTrojan();
    const QString entry = ClashYamlBuilder::buildProxyEntry(original);
    QVERIFY(!entry.isEmpty());
    const VmessItem parsed = parseClashEntry(entry);
    QCOMPARE(parsed.configType, ConfigType::Trojan);
    QCOMPARE(parsed.address, original.address);
    QCOMPARE(parsed.port, original.port);
    QCOMPARE(parsed.id, original.id);
    QCOMPARE(parsed.sni, original.sni);
}

void ExportServiceTests::roundTripShadowsocksThroughClashYaml()
{
    const VmessItem original = makeShadowsocks();
    const QString entry = ClashYamlBuilder::buildProxyEntry(original);
    QVERIFY(!entry.isEmpty());
    const VmessItem parsed = parseClashEntry(entry);
    QCOMPARE(parsed.configType, ConfigType::Shadowsocks);
    QCOMPARE(parsed.address, original.address);
    QCOMPARE(parsed.port, original.port);
    QCOMPARE(parsed.id, original.id);
    QCOMPARE(parsed.security, original.security);
}

// ---------- ExportService tests ----------

void ExportServiceTests::exportUrisSkipsCustomNodes()
{
    VmessItem custom;
    custom.configType = ConfigType::Custom;
    custom.address = QStringLiteral("1.2.3.4");
    custom.port = 1234;
    const ExportService svc;
    QVERIFY(svc.exportUris({custom}).isEmpty());
}

void ExportServiceTests::exportClashYamlProducesProxiesBlock()
{
    const ExportService svc;
    const QString yaml = svc.exportClashYaml({makeVmess(), makeTrojan()});
    QVERIFY(yaml.startsWith(QStringLiteral("proxies:\n")));
}

void ExportServiceTests::deduplicateKeepsFirstOccurrence()
{
    VmessItem a = makeVmess();
    a.remarks = QStringLiteral("first");
    VmessItem b = makeVmess();  // same fingerprint
    b.remarks = QStringLiteral("duplicate");
    const ExportService svc;
    const QList<VmessItem> result = svc.deduplicate({a, b});
    QCOMPARE(result.size(), 1);
    QCOMPARE(result.constFirst().remarks, QStringLiteral("first"));
}

void ExportServiceTests::deduplicateIgnoresRemarks()
{
    VmessItem a = makeVmess();
    a.remarks = QStringLiteral("Node A");
    VmessItem b = makeVmess();
    b.remarks = QStringLiteral("Node B — completely different name");
    const ExportService svc;
    // Same address/port/credentials => one entry
    QCOMPARE(svc.deduplicate({a, b}).size(), 1);
}

void ExportServiceTests::deduplicateIgnoresTransportDetails()
{
    VmessItem a = makeVmess();
    a.network = QStringLiteral("tcp");
    VmessItem b = makeVmess();
    b.network = QStringLiteral("ws");
    b.path = QStringLiteral("/path");
    // Same type+address+port+credentials, different transport => still a duplicate
    const ExportService svc;
    QCOMPARE(svc.deduplicate({a, b}).size(), 1);
}

void ExportServiceTests::exportDedupedUrisRemovesDuplicates()
{
    VmessItem a = makeVmess();
    a.remarks = QStringLiteral("A");
    VmessItem b = makeVmess();
    b.remarks = QStringLiteral("B");
    VmessItem c = makeTrojan();
    const ExportService svc;
    const QStringList uris = svc.exportDedupedUris({a, b, c});
    // a and b are duplicates; c is different — expect 2 URIs
    QCOMPARE(uris.size(), 2);
}

void ExportServiceTests::exportDedupedClashYamlRemovesDuplicates()
{
    VmessItem a = makeTrojan(QStringLiteral("1.2.3.4"), 443, QStringLiteral("pw"));
    VmessItem b = makeTrojan(QStringLiteral("1.2.3.4"), 443, QStringLiteral("pw"));
    b.remarks = QStringLiteral("copy");
    const ExportService svc;
    const QString yaml = svc.exportDedupedClashYaml({a, b});
    QCOMPARE(yaml.count(QStringLiteral("  - {")), 1);
}

QTEST_GUILESS_MAIN(ExportServiceTests)
#include "ExportServiceTests.moc"
