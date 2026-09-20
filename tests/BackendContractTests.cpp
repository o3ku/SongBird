#include <QtTest>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include "common/RoutingValuePattern.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/SingBoxDnsConfigSupport.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/CoreDescriptor.h"
#include "runtime/core/ICoreBackend.h"

namespace {

// Populated broadly so every protocol finds the fields it needs; the contract
// under test is "does the backend implement this protocol", not field mapping.
VmessItem minimalServerFor(ConfigType configType)
{
    VmessItem server;
    server.configType = configType;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.security = QStringLiteral("aes-128-gcm");
    server.streamSecurity = QStringLiteral("tls");
    server.sni = QStringLiteral("example.com");
    server.username = QStringLiteral("user");
    server.privateKey = QStringLiteral("cHJpdmF0ZS1rZXk=");
    server.peerPublicKey = QStringLiteral("cHVibGljLWtleQ==");
    server.localAddress = QStringLiteral("10.0.0.2");
    return server;
}

// Xray and sing-box both tag the proxy outbound "proxy" but name the protocol
// field differently; Clash-family cores emit a proxies array instead.
QString wireProtocolOf(const QJsonObject& root)
{
    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    for (const QJsonValue& value : outbounds) {
        const QJsonObject outbound = value.toObject();
        if (outbound.value(QStringLiteral("tag")).toString() != QStringLiteral("proxy")) {
            continue;
        }
        const QString xrayProtocol = outbound.value(QStringLiteral("protocol")).toString();
        return xrayProtocol.isEmpty()
            ? outbound.value(QStringLiteral("type")).toString()
            : xrayProtocol;
    }

    const QJsonArray proxies = root.value(QStringLiteral("proxies")).toArray();
    if (!proxies.isEmpty()) {
        return proxies.at(0).toObject().value(QStringLiteral("type")).toString();
    }

    return {};
}

QString describe(CoreType coreType, ConfigType configType)
{
    return QStringLiteral("%1 / %2")
        .arg(coreTypeDisplayName(coreType))
        .arg(configTypeDisplayName(configType));
}

} // namespace

class BackendContractTests : public QObject {
    Q_OBJECT

private slots:
    void everyRegisteredCoreHasBackend();
    void everyDeclaredProtocolProducesConfig();
    void declaredProtocolsMapToDistinctWireProtocols();
    void routingValueParsingDoesNotMistakeAddressesForPrefixes();
    void unsupportedRoutingValuesAreReported();
    void dnsRuleFieldsFollowTheSharedPrefixVocabulary();
};

void BackendContractTests::everyRegisteredCoreHasBackend()
{
    const QList<CoreDescriptor> descriptors = coreDescriptors();
    QVERIFY(!descriptors.isEmpty());

    for (const CoreDescriptor& descriptor : descriptors) {
        QVERIFY2(
            coreBackend(descriptor.type) != nullptr,
            qPrintable(QStringLiteral("%1 is registered but has no backend")
                           .arg(coreTypeDisplayName(descriptor.type))));
    }
}

void BackendContractTests::everyDeclaredProtocolProducesConfig()
{
    const Config config;

    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        const ICoreBackend* backend = coreBackend(descriptor.type);
        QVERIFY(backend != nullptr);

        for (const ConfigType configType : descriptor.supportedConfigTypes) {
            // Custom configs are copied verbatim rather than generated.
            if (configType == ConfigType::Custom) {
                continue;
            }

            const QJsonObject root = backend->buildClientRoot(config, minimalServerFor(configType));
            QVERIFY2(
                !root.isEmpty(),
                qPrintable(QStringLiteral("%1: declared as supported but produced no config")
                               .arg(describe(descriptor.type, configType))));
            QVERIFY2(
                !wireProtocolOf(root).isEmpty(),
                qPrintable(QStringLiteral("%1: config carries no proxy protocol")
                               .arg(describe(descriptor.type, configType))));
        }
    }
}

void BackendContractTests::declaredProtocolsMapToDistinctWireProtocols()
{
    const Config config;

    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        const ICoreBackend* backend = coreBackend(descriptor.type);
        QVERIFY(backend != nullptr);

        QHash<QString, ConfigType> seenWireProtocols;
        for (const ConfigType configType : descriptor.supportedConfigTypes) {
            if (configType == ConfigType::Custom) {
                continue;
            }

            const QJsonObject root = backend->buildClientRoot(config, minimalServerFor(configType));
            const QString wireProtocol = wireProtocolOf(root);
            if (wireProtocol.isEmpty()) {
                continue; // reported by everyDeclaredProtocolProducesConfig
            }

            if (seenWireProtocols.contains(wireProtocol)) {
                QFAIL(qPrintable(
                    QStringLiteral("%1 maps both %2 and %3 to wire protocol '%4'; one of them has no "
                                   "implementation and fell through to a default branch")
                        .arg(coreTypeDisplayName(descriptor.type))
                        .arg(configTypeDisplayName(seenWireProtocols.value(wireProtocol)))
                        .arg(configTypeDisplayName(configType))
                        .arg(wireProtocol)));
            }
            seenWireProtocols.insert(wireProtocol, configType);
        }
    }
}

void BackendContractTests::routingValueParsingDoesNotMistakeAddressesForPrefixes()
{
    // Routing values carry their match type in a "word:" prefix, but two legitimate values also
    // contain a colon: an IPv6 literal and a host carrying a port. Reading either as an unknown
    // prefix would report a perfectly good rule as broken.
    for (const QString& address : {QStringLiteral("10.0.0.0/8"),
             QStringLiteral("fe80::1"),
             QStringLiteral("2001:db8::/32"),
             QStringLiteral("dead:beef::1")}) {
        const RoutingValuePattern::IpValue parsed = RoutingValuePattern::parseIp(address);
        QVERIFY2(parsed.recognised, qPrintable(address));
        QVERIFY2(parsed.kind == RoutingValuePattern::IpKind::Address, qPrintable(address));
        QCOMPARE(parsed.value, address);
    }

    const RoutingValuePattern::IpValue geoip = RoutingValuePattern::parseIp(QStringLiteral("geoip:private"));
    QVERIFY(geoip.recognised);
    QVERIFY(geoip.kind == RoutingValuePattern::IpKind::GeoIp);
    QCOMPARE(geoip.value, QStringLiteral("private"));

    const RoutingValuePattern::DomainValue hostWithPort = RoutingValuePattern::parseDomain(QStringLiteral("example.com:8080"));
    QVERIFY(hostWithPort.recognised);
    QVERIFY(hostWithPort.kind == RoutingValuePattern::DomainKind::Bare);
    QCOMPARE(hostWithPort.value, QStringLiteral("example.com:8080"));

    // A leading dot is shorthand for the same suffix match "domain:" asks for.
    const RoutingValuePattern::DomainValue dotted = RoutingValuePattern::parseDomain(QStringLiteral(".example.com"));
    QVERIFY(dotted.kind == RoutingValuePattern::DomainKind::Suffix);
    QCOMPARE(dotted.value, QStringLiteral("example.com"));
}

void BackendContractTests::unsupportedRoutingValuesAreReported()
{
    // One issue per value a core cannot honour faithfully: an unknown prefix, an "ext:" rule
    // set, the xray-only "dotless:" modifier, and a prefix with nothing after it. "domain:" and
    // "geoip:" are honoured everywhere and must raise nothing.
    const QList<RoutingValuePattern::Issue> issues = RoutingValuePattern::issuesForValues(
        {QStringLiteral("domain:example.com"),
            QStringLiteral("nosuchprefix:example.com"),
            QStringLiteral("ext:geosite.dat:cn"),
            QStringLiteral("dotless:example.com"),
            QStringLiteral("domain:")},
        {QStringLiteral("geoip:cn"), QStringLiteral("keyword:1.2.3.4")});

    QCOMPARE(issues.size(), 5);

    QVERIFY(issues.at(0).kind == RoutingValuePattern::IssueKind::UnknownPrefix);
    QCOMPARE(issues.at(0).field, QStringLiteral("domain"));
    QCOMPARE(issues.at(0).detail, QStringLiteral("nosuchprefix:"));

    QVERIFY(issues.at(1).kind == RoutingValuePattern::IssueKind::Unsupported);
    QCOMPARE(issues.at(1).detail, QStringLiteral("ext:"));

    QVERIFY(issues.at(2).kind == RoutingValuePattern::IssueKind::Approximated);
    QCOMPARE(issues.at(2).detail, QStringLiteral("dotless:"));

    QVERIFY(issues.at(3).kind == RoutingValuePattern::IssueKind::EmptyValue);
    QCOMPARE(issues.at(3).detail, QStringLiteral("domain:"));

    QVERIFY(issues.at(4).kind == RoutingValuePattern::IssueKind::UnknownPrefix);
    QCOMPARE(issues.at(4).field, QStringLiteral("ip"));
    QCOMPARE(issues.at(4).detail, QStringLiteral("keyword:"));

    // Nothing portable is ever reported, so a config built from the built-in profiles -- which
    // use only "domain:", "geosite:" and "geoip:" -- stays quiet.
    QVERIFY(RoutingValuePattern::issuesForValues(
        {QStringLiteral("domain:example-example.com"), QStringLiteral("geosite:cn"), QStringLiteral(".example.com")},
        {QStringLiteral("geoip:private"), QStringLiteral("10.0.0.0/8"), QStringLiteral("fe80::1")})
                .isEmpty());
}

void BackendContractTests::dnsRuleFieldsFollowTheSharedPrefixVocabulary()
{
    // The DNS rule builder used to carry its own copy of the prefix chain, and the copy had
    // drifted from the routing mappers. It now reads the shared vocabulary, so every kind has to
    // land on the same field the routing side would choose.
    const auto fieldFor = [](const QString& value, bool plainAsDomain) {
        QJsonObject rule;
        if (!SingBoxDnsConfigSupport::appendDomainField(rule, value, plainAsDomain) || rule.isEmpty()) {
            return QString();
        }

        const QString key = rule.keys().constFirst();
        return QStringLiteral("%1=%2").arg(key, rule.value(key).toArray().at(0).toString());
    };

    QCOMPARE(fieldFor(QStringLiteral("domain:example.com"), true), QStringLiteral("domain_suffix=example.com"));
    QCOMPARE(fieldFor(QStringLiteral("full:example.com"), true), QStringLiteral("domain=example.com"));
    QCOMPARE(fieldFor(QStringLiteral("keyword:tracker"), true), QStringLiteral("domain_keyword=tracker"));
    QCOMPARE(fieldFor(QStringLiteral("dotless:example"), true), QStringLiteral("domain_keyword=example"));
    QCOMPARE(fieldFor(QStringLiteral("geosite:cn"), true), QStringLiteral("geosite=cn"));
    QCOMPARE(fieldFor(QStringLiteral("regexp:^ad\\."), true), QStringLiteral("domain_regex=^ad\\."));

    // A bare value here is a hostname read from a hosts file, so it matches that host exactly.
    // The same bare value in a routing rule is a keyword, which is what the false flag asks for.
    QCOMPARE(fieldFor(QStringLiteral("example.com"), true), QStringLiteral("domain=example.com"));
    QCOMPARE(fieldFor(QStringLiteral("example.com"), false), QStringLiteral("domain_keyword=example.com"));

    // A leading dot means the host and its subdomains, exactly like "domain:". It used to reach
    // the bare branch and become an exact match instead.
    QCOMPARE(fieldFor(QStringLiteral(".example.com"), true), QStringLiteral("domain_suffix=example.com"));

    // A host carrying a port is not a prefix, and must survive as a bare hostname.
    QCOMPARE(fieldFor(QStringLiteral("example.com:8080"), true), QStringLiteral("domain=example.com:8080"));

    // Values no core can express as a DNS match are dropped rather than emitted as a literal
    // host that could never be queried. A comment and a blank entry are not values at all.
    for (const QString& value : {QStringLiteral("ext:geosite.dat:cn"),
             QStringLiteral("ext-domain:geosite.dat:cn"),
             QStringLiteral("nosuchprefix:example.com"),
             QStringLiteral("domain:"),
             QStringLiteral("  "),
             QStringLiteral("# a comment"),
             QString()}) {
        QJsonObject rule;
        QVERIFY2(!SingBoxDnsConfigSupport::appendDomainField(rule, value, true), qPrintable(value));
        QVERIFY2(rule.isEmpty(), qPrintable(value));
    }
}

QTEST_MAIN(BackendContractTests)
#include "BackendContractTests.moc"
