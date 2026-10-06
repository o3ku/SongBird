#include <QtTest>

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include "common/RoutingValuePattern.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/AuxiliaryTunConfig.h"
#include "runtime/ProtocolConfigMapper.h"
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

// Outbounds are a flat list of objects. A nested array is not an outbound, and sing-box rejects the
// whole file over it, so the entries are flattened here and the emptiness of each one is asserted
// by the caller rather than silently counted as present.
QList<QJsonObject> tunOutboundObjects(const QJsonObject& root)
{
    QList<QJsonObject> outbounds;
    for (const QJsonValue& value : root.value(QStringLiteral("outbounds")).toArray()) {
        outbounds.append(value.toObject());
    }
    return outbounds;
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
    void dnsDomainStrategyMapsUiVocabularyToSingBoxValues();
    void auxiliaryTunRootIsUsableInBothRoutingModes();
    void auxiliaryTunRootComesFromTheRuntimeLayer();
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

void BackendContractTests::dnsDomainStrategyMapsUiVocabularyToSingBoxValues()
{
    // The settings combo stores Xray's address-strategy names; sing-box needs its own four
    // `strategy` values. A bare family restricts resolution to that family, a combined spelling
    // prefers one family and falls back to the other, and the legacy Prefer*/Only* names older
    // configs may still carry keep meaning the same two things.
    const auto mapped = [](const QString& value) {
        return SingBoxDnsConfigSupport::mapDomainStrategy(value);
    };

    QCOMPARE(mapped(QStringLiteral("UseIPv4")), QStringLiteral("ipv4_only"));
    QCOMPARE(mapped(QStringLiteral("UseIPv6")), QStringLiteral("ipv6_only"));
    QCOMPARE(mapped(QStringLiteral("UseIPv4v6")), QStringLiteral("prefer_ipv4"));
    QCOMPARE(mapped(QStringLiteral("UseIPv6v4")), QStringLiteral("prefer_ipv6"));
    QCOMPARE(mapped(QStringLiteral("ForceIPv4")), QStringLiteral("ipv4_only"));
    QCOMPARE(mapped(QStringLiteral("ForceIPv6")), QStringLiteral("ipv6_only"));
    QCOMPARE(mapped(QStringLiteral("ForceIPv4v6")), QStringLiteral("prefer_ipv4"));
    QCOMPARE(mapped(QStringLiteral("ForceIPv6v4")), QStringLiteral("prefer_ipv6"));

    // Legacy spellings kept for saved configs.
    QCOMPARE(mapped(QStringLiteral("PreferIPv4")), QStringLiteral("prefer_ipv4"));
    QCOMPARE(mapped(QStringLiteral("PreferIPv6")), QStringLiteral("prefer_ipv6"));
    QCOMPARE(mapped(QStringLiteral("OnlyIPv4")), QStringLiteral("ipv4_only"));
    QCOMPARE(mapped(QStringLiteral("OnlyIPv6")), QStringLiteral("ipv6_only"));

    // Case and surrounding whitespace are part of the same vocabulary.
    QCOMPARE(mapped(QStringLiteral("  useipv4 ")), QStringLiteral("ipv4_only"));

    // Everything else stays unmapped rather than becoming an invalid sing-box strategy: AsIs
    // leaves the choice to the core, UseIP / ForceIP name both families, and Xray's routing-only
    // IPIfNonMatch / IPOnDemand are no longer offered by the page but may still sit in a saved
    // config.
    QVERIFY(mapped(QStringLiteral("AsIs")).isEmpty());
    QVERIFY(mapped(QStringLiteral("UseIP")).isEmpty());
    QVERIFY(mapped(QStringLiteral("ForceIP")).isEmpty());
    QVERIFY(mapped(QStringLiteral("IPIfNonMatch")).isEmpty());
    QVERIFY(mapped(QStringLiteral("IPOnDemand")).isEmpty());
    QVERIFY(mapped(QString()).isEmpty());
}

void BackendContractTests::auxiliaryTunRootIsUsableInBothRoutingModes()
{
    // The TUN device is created by a core process of its own, so its root is a core artifact and is
    // built by the core backend. It used to be assembled by hand in the auto front end, where the
    // outbound list was appended as a whole array instead of element by element -- producing
    // "outbounds": [[...]] and a file sing-box refuses to load ("cannot unmarshal array into Go
    // struct field _Options.outbounds of type option._Outbound").
    const ICoreBackend* backend = coreBackend(CoreType::SingBox);
    QVERIFY(backend != nullptr);

    Config config;
    config.tun().tunModeItem.enableTun = true;

    const QJsonObject relayRoot =
        backend->buildAuxiliaryTunClientRoot(config, AuxiliaryTunRouting::RelayToLocalProxy);
    QVERIFY(!relayRoot.isEmpty());

    const QList<QJsonObject> relayOutbounds = tunOutboundObjects(relayRoot);
    QCOMPARE(relayOutbounds.size(), 3);
    for (const QJsonObject& outbound : relayOutbounds) {
        QVERIFY2(!outbound.isEmpty(), "outbounds must be a flat list of outbound objects");
    }
    QCOMPARE(relayOutbounds.at(0).value(QStringLiteral("tag")).toString(), QStringLiteral("proxy"));
    QCOMPARE(relayRoot.value(QStringLiteral("route")).toObject().value(QStringLiteral("final")).toString(),
             QStringLiteral("proxy"));

    // Direct mode carries no proxy traffic at all: it exists to keep the adapter alive while the
    // proxy session is down, so it has neither a proxy outbound nor a proxy final hop.
    const QJsonObject directRoot =
        backend->buildAuxiliaryTunClientRoot(config, AuxiliaryTunRouting::DirectOnly);
    QVERIFY(!directRoot.isEmpty());

    const QList<QJsonObject> directOutbounds = tunOutboundObjects(directRoot);
    QCOMPARE(directOutbounds.size(), 2);
    for (const QJsonObject& outbound : directOutbounds) {
        QVERIFY2(!outbound.isEmpty(), "outbounds must be a flat list of outbound objects");
    }
    QCOMPARE(directOutbounds.at(0).value(QStringLiteral("tag")).toString(), QStringLiteral("direct"));
    QCOMPARE(directOutbounds.at(1).value(QStringLiteral("tag")).toString(), QStringLiteral("block"));
    QCOMPARE(directRoot.value(QStringLiteral("route")).toObject().value(QStringLiteral("final")).toString(),
             QStringLiteral("direct"));

    // Both modes create the same TUN inbound and follow the configured log level rather than a
    // hardcoded one.
    for (const QJsonObject& root : {relayRoot, directRoot}) {
        const QJsonArray inbounds = root.value(QStringLiteral("inbounds")).toArray();
        QCOMPARE(inbounds.size(), 1);
        QCOMPARE(inbounds.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("tun"));
        QCOMPARE(root.value(QStringLiteral("log")).toObject().value(QStringLiteral("level")).toString(),
                 ProtocolConfigMapper::normalizeSingBoxLogLevel(config.logLevel));
    }

    // The sidecar keeps the level but must never name a log file. It only exists when the real core
    // is Xray, so a singbox.log written by a process the user did not choose -- while their actual
    // core writes Vaccess.log/Verror.log -- would be misleading rather than useful.
    Config loggingConfig = config;
    loggingConfig.logEnabled = true;
    for (const AuxiliaryTunRouting routing : {AuxiliaryTunRouting::RelayToLocalProxy,
                                              AuxiliaryTunRouting::DirectOnly}) {
        const QJsonObject log =
            backend->buildAuxiliaryTunClientRoot(loggingConfig, routing)
                .value(QStringLiteral("log"))
                .toObject();
        QVERIFY2(!log.contains(QStringLiteral("output")), "the TUN sidecar must not name a log file");
        QCOMPARE(log.value(QStringLiteral("disabled")).toBool(), false);
    }
}

void BackendContractTests::auxiliaryTunRootComesFromTheRuntimeLayer()
{
    // Front ends are not allowed to include a concrete backend, so they ask the runtime layer for
    // the root. The helper has to hand back exactly what the backend builds, and has to stay empty
    // for a core that cannot back a TUN device.
    Config config;
    config.tun().tunModeItem.enableTun = true;

    const ICoreBackend* backend = coreBackend(CoreType::SingBox);
    QVERIFY(backend != nullptr);

    const QJsonObject viaRuntime =
        AuxiliaryTunConfig::buildRoot(CoreType::SingBox, config, AuxiliaryTunRouting::RelayToLocalProxy);
    const QJsonObject viaBackend =
        backend->buildAuxiliaryTunClientRoot(config, AuxiliaryTunRouting::RelayToLocalProxy);
    QVERIFY(!viaRuntime.isEmpty());
    QVERIFY(viaRuntime == viaBackend);

    QVERIFY(AuxiliaryTunConfig::buildRoot(CoreType::Unknown, config, AuxiliaryTunRouting::RelayToLocalProxy).isEmpty());
}

QTEST_MAIN(BackendContractTests)
#include "BackendContractTests.moc"
