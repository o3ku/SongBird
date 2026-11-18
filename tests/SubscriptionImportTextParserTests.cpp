#include <QtTest>

#include "subscription/SubscriptionImportTextParser.h"

class SubscriptionImportTextParserTests : public QObject
{
    Q_OBJECT

private slots:
    void loopbackHostAcceptsOnlyTheThreeNamedHosts();
    void httpsIsAllowedForAnyHost();
    void httpIsAllowedOnlyForLoopbackHosts();
    void otherSchemesAndHostlessUrlsAreNotAllowed();
    void isSubscriptionUrlStillClassifiesBothSchemes();
};

void SubscriptionImportTextParserTests::loopbackHostAcceptsOnlyTheThreeNamedHosts()
{
    QVERIFY(SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("127.0.0.1")));
    QVERIFY(SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("localhost")));
    QVERIFY(SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("LOCALHOST")));
    QVERIFY(SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("::1")));
    QVERIFY(SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("[::1]")));
    QVERIFY(SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("  localhost  ")));

    // The whole 127/8 range is loopback, but the policy names exactly 127.0.0.1: a
    // neighbouring loopback address is not an allowed http host.
    QVERIFY(!SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("127.0.0.2")));
    QVERIFY(!SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("127.1")));
    QVERIFY(!SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("example.com")));
    QVERIFY(!SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("localhost.example.com")));
    QVERIFY(!SubscriptionImportTextParser::isLoopbackHost(QStringLiteral("")));
}

void SubscriptionImportTextParserTests::httpsIsAllowedForAnyHost()
{
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("https://example.com/sub")));
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("https://127.0.0.1:8080/sub")));
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("https://[::1]:8080/sub")));
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("HTTPS://EXAMPLE.COM/SUB")));
}

void SubscriptionImportTextParserTests::httpIsAllowedOnlyForLoopbackHosts()
{
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://127.0.0.1:8080/sub")));
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://localhost/sub")));
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://[::1]/sub")));
    QVERIFY(SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://127.0.0.1/sub")));

    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://example.com/sub")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://127.0.0.2/sub")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://192.168.1.1/sub")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("http://10.0.0.1/sub")));
}

void SubscriptionImportTextParserTests::otherSchemesAndHostlessUrlsAreNotAllowed()
{
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("ftp://example.com/sub")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("file:///etc/passwd")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("https:///sub")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("https://")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("")));
    QVERIFY(!SubscriptionImportTextParser::isAllowedSubscriptionUrl(QStringLiteral("not-a-url://??")));
}

void SubscriptionImportTextParserTests::isSubscriptionUrlStillClassifiesBothSchemes()
{
    // Classification is deliberately wider than the fetch policy: a pasted http URL is
    // still recognised as a subscription URL so the fetch can refuse it with the
    // actionable "use https" error, instead of the import silently treating the line
    // as node content and reporting "no servers found".
    QVERIFY(SubscriptionImportTextParser::isSubscriptionUrl(QStringLiteral("http://example.com/sub")));
    QVERIFY(SubscriptionImportTextParser::isSubscriptionUrl(QStringLiteral("https://example.com/sub")));
    QVERIFY(!SubscriptionImportTextParser::isSubscriptionUrl(QStringLiteral("vmess://eyJhZGQiOiIxMjcuMC4wLjEifQ==")));
}

QTEST_MAIN(SubscriptionImportTextParserTests)
#include "SubscriptionImportTextParserTests.moc"
