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

QTEST_MAIN(SystemProxyExceptionsTests)

#include "SystemProxyExceptionsTests.moc"
