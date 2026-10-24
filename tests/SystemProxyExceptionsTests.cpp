#include <QtTest>

#include "common/SystemProxyExceptions.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/RoutingRule.h"

// The IE/WinINET bypass list is written by both front ends and was, until now, two copies of the
// same literal that then diverged: SongBird.exe merged the hosts its routing rules send direct,
// SongBirdAuto.exe returned the literal untouched. Same config file, two different bypass lists --
// a `direct` rule bypassed the proxy in one executable and went through it in the other. These
// cases pin the shared resolver both call now.
class SystemProxyExceptionsTests : public QObject {
    Q_OBJECT

private slots:
    void defaultListBypassesLoopbackAndPrivateRanges();
    void splitAcceptsBothSeparatorsAndDropsEmpties();
    void appendUniqueIgnoresCaseAndDuplicates();
    void resolveFallsBackToTheDefaultList();
    void resolveKeepsTheUsersOwnList();
    void resolveMergesHostsTheRoutingRulesSendDirect();
    void resolveKeepsTheProxyOutOfDomainsRoutedThroughIt();
    void resolveMergesIpLiteralsTheRoutingRulesSendDirect();
    void resolveReadsTheRulesInTheCoresOrder();
    void resolveWritesIpv6AsTheUrlSpellsIt();
};

void SystemProxyExceptionsTests::defaultListBypassesLoopbackAndPrivateRanges()
{
    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::defaultList());

    // Local traffic is local by definition; proxying it makes a local service unreachable for no
    // benefit. Spot-check one entry from each family rather than the whole 174 characters.
    QVERIFY(entries.contains(QStringLiteral("localhost")));
    QVERIFY(entries.contains(QStringLiteral("127.*")));
    QVERIFY(entries.contains(QStringLiteral("10.*")));
    QVERIFY(entries.contains(QStringLiteral("172.16.*")));
    QVERIFY(entries.contains(QStringLiteral("172.31.*")));
    QVERIFY(entries.contains(QStringLiteral("192.168.*")));
}

void SystemProxyExceptionsTests::splitAcceptsBothSeparatorsAndDropsEmpties()
{
    const QStringList entries =
        SystemProxyExceptions::split(QStringLiteral(" a.com ;; b.com, c.com ; "));

    QCOMPARE(entries, (QStringList{QStringLiteral("a.com"), QStringLiteral("b.com"), QStringLiteral("c.com")}));
}

void SystemProxyExceptionsTests::appendUniqueIgnoresCaseAndDuplicates()
{
    QStringList entries{QStringLiteral("localhost")};
    SystemProxyExceptions::appendUnique(entries, QStringLiteral(" LocalHost "));
    SystemProxyExceptions::appendUnique(entries, QStringLiteral("*.example.com"));
    SystemProxyExceptions::appendUnique(entries, QString());

    // Case-insensitive because Windows matches the bypass list that way: two spellings are one
    // entry, and adding the second would only make the list longer.
    QCOMPARE(entries, (QStringList{QStringLiteral("localhost"), QStringLiteral("*.example.com")}));
}

void SystemProxyExceptionsTests::resolveFallsBackToTheDefaultList()
{
    Config config;
    config.defaults().defIeProxyExceptions.clear();

    QCOMPARE(SystemProxyExceptions::resolve(config), SystemProxyExceptions::defaultList());
}

void SystemProxyExceptionsTests::resolveKeepsTheUsersOwnList()
{
    // A user who narrowed the list meant it: the routing-derived entries are added to it, but the
    // default ranges are not put back.
    Config config;
    config.defaults().defIeProxyExceptions = QStringLiteral("localhost;corp.example");

    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::resolve(config));
    QVERIFY(entries.contains(QStringLiteral("corp.example")));
    QVERIFY(!entries.contains(QStringLiteral("192.168.*")));
}

void SystemProxyExceptionsTests::resolveMergesHostsTheRoutingRulesSendDirect()
{
    Config config;
    config.defaults().defIeProxyExceptions = QStringLiteral("localhost");

    RoutingRule suffixRule;
    suffixRule.outboundTag = QStringLiteral("direct");
    suffixRule.domain = QStringList{QStringLiteral("domain:example.com")};
    config.collection().routingCustomRules.append(suffixRule);

    RoutingRule exactRule;
    exactRule.outboundTag = QStringLiteral("Direct");
    exactRule.domain = QStringList{QStringLiteral("full:exact.test")};
    config.collection().routingCustomRules.append(exactRule);

    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::resolve(config));

    // "domain:" means the host and its subdomains, so the system proxy needs both forms.
    QVERIFY(entries.contains(QStringLiteral("example.com")));
    QVERIFY(entries.contains(QStringLiteral("*.example.com")));
    // "full:" is the host alone -- no subdomain entry.
    QVERIFY(entries.contains(QStringLiteral("exact.test")));
    QVERIFY(!entries.contains(QStringLiteral("*.exact.test")));
    // The user's own entry survives the merge.
    QVERIFY(entries.contains(QStringLiteral("localhost")));
}

void SystemProxyExceptionsTests::resolveKeepsTheProxyOutOfDomainsRoutedThroughIt()
{
    Config config;

    RoutingRule proxiedRule;
    proxiedRule.outboundTag = QStringLiteral("proxy");
    proxiedRule.domain = QStringList{QStringLiteral("domain:proxied.test")};
    config.collection().routingCustomRules.append(proxiedRule);

    RoutingRule disabledRule;
    disabledRule.outboundTag = QStringLiteral("direct");
    disabledRule.domain = QStringList{QStringLiteral("domain:disabled.test")};
    disabledRule.enabled = false;
    config.collection().routingCustomRules.append(disabledRule);

    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::resolve(config));

    // Bypassing a proxied host would send it straight out, which is the opposite of what the rule
    // says; a disabled rule is not part of the effective configuration at all.
    QVERIFY(!entries.contains(QStringLiteral("proxied.test")));
    QVERIFY(!entries.contains(QStringLiteral("disabled.test")));
}

void SystemProxyExceptionsTests::resolveMergesIpLiteralsTheRoutingRulesSendDirect()
{
    Config config;
    config.defaults().defIeProxyExceptions = QStringLiteral("localhost");

    RoutingRule directRule;
    directRule.outboundTag = QStringLiteral("direct");
    directRule.ip = QStringList{
        QStringLiteral("203.0.113.7"),
        QStringLiteral("geoip:cn"),
        QStringLiteral("10.20.30.0/24"),
        QStringLiteral("2001:db8::1"),
    };
    config.collection().routingCustomRules.append(directRule);

    RoutingRule proxiedRule;
    proxiedRule.outboundTag = QStringLiteral("proxy");
    proxiedRule.ip = QStringList{QStringLiteral("198.51.100.9")};
    config.collection().routingCustomRules.append(proxiedRule);

    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::resolve(config));

    // A plain literal is the one ip value WinINET can say back; the ranged forms (geoip:, CIDR)
    // have no bypass-list spelling and stay core-only, and a proxied rule's ip never bypasses.
    QVERIFY(entries.contains(QStringLiteral("203.0.113.7")));
    QVERIFY(entries.contains(QStringLiteral("2001:db8::1")));
    QVERIFY(!entries.contains(QStringLiteral("cn")));
    QVERIFY(!entries.contains(QStringLiteral("10.20.30.0/24")));
    QVERIFY(!entries.contains(QStringLiteral("198.51.100.9")));
    // The entries this resolver produces are what macOS consumes through macProxyBypassDomains();
    // whether that platform honours an IPv6 literal is a question about networksetup, pinned in
    // MacOSPlatformServicesTests, not here.
}

void SystemProxyExceptionsTests::resolveReadsTheRulesInTheCoresOrder()
{
    Config config;
    config.defaults().defIeProxyExceptions = QStringLiteral("localhost");

    // Appended first; the proxy rule that follows is appended later, so a resolver that walked the
    // list in append order would express them in the opposite order.
    RoutingRule directIp;
    directIp.outboundTag = QStringLiteral("direct");
    directIp.ip = QStringList{QStringLiteral("203.0.113.7")};
    config.collection().routingCustomRules.append(directIp);

    RoutingRule proxyCatchAll;
    proxyCatchAll.outboundTag = QStringLiteral("proxy");
    proxyCatchAll.domain = QStringList{QStringLiteral("full:proxied.test")};
    config.collection().routingCustomRules.append(proxyCatchAll);

    // A profile rule comes after every custom rule in the list the core reads, so a profile that
    // sends a host direct still contributes its host -- the two sources are one ordered list, not
    // two lists concatenated in whatever order the resolver happened to use.
    RoutingItem profile;
    // Custom profiles are normalised to "custom:<name>"; matching that is what makes this the
    // selected profile instead of the first built-in entry.
    profile.id = QStringLiteral("custom:test-profile");
    RoutingRule profileDirect;
    profileDirect.outboundTag = QStringLiteral("direct");
    profileDirect.domain = QStringList{QStringLiteral("full:profile.test")};
    profile.rules.append(profileDirect);
    config.collection().customRoutingItems.append(profile);
    config.collection().routingModeId = QStringLiteral("custom:test-profile");

    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::resolve(config));

    // Nothing extra has to be configured for this: the selected profile's rules travel through the
    // same list as the custom rules, and every entry that has no bypass spelling is skipped rather
    // than written out as raw text.
    QVERIFY(entries.contains(QStringLiteral("203.0.113.7")));
    QVERIFY(entries.contains(QStringLiteral("profile.test")));
    QVERIFY(!entries.contains(QStringLiteral("proxied.test")));
}

void SystemProxyExceptionsTests::resolveWritesIpv6AsTheUrlSpellsIt()
{
    Config config;
    config.defaults().defIeProxyExceptions = QStringLiteral("localhost");

    RoutingRule directRule;
    directRule.outboundTag = QStringLiteral("direct");
    directRule.ip = QStringList{
        // One address, written three ways, plus a range that is not one.
        QStringLiteral("2001:0db8:0000:0000:0000:0000:0000:0001"),
        QStringLiteral("2001:DB8::1"),
        QStringLiteral("2001:db8::1/128"),
        QStringLiteral("2001:db8::2"),
        // Wider than one address: no WinINET spelling, so it stays with the core.
        QStringLiteral("2001:db8::/32"),
    };
    config.collection().routingCustomRules.append(directRule);

    const QStringList entries = SystemProxyExceptions::split(SystemProxyExceptions::resolve(config));

    // A browser sends the compressed form as the host of a URL, so all three spellings of the same
    // address are one entry and it is written compressed.
    QVERIFY(entries.contains(QStringLiteral("2001:db8::1")));
    QVERIFY(!entries.contains(QStringLiteral("2001:0db8:0000:0000:0000:0000:0000:0001")));
    QVERIFY(!entries.contains(QStringLiteral("2001:db8::1/128")));
    QCOMPARE(entries.count(QStringLiteral("2001:db8::1")), 1);
    QVERIFY(entries.contains(QStringLiteral("2001:db8::2")));
    QVERIFY(!entries.contains(QStringLiteral("2001:db8::/32")));
}

QTEST_MAIN(SystemProxyExceptionsTests)

#include "SystemProxyExceptionsTests.moc"
