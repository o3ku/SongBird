#include <QtTest>

#include "platform/ITerminalProxyService.h"
#include "platform/TerminalProxyPolicy.h"

// Characterization tests for the terminal-proxy decision logic.
//
// The write path itself (HKEY_CURRENT_USER\Environment plus a window broadcast) cannot be driven
// from a unit test, which is exactly why the decisions live in TerminalProxyPolicy: the URL that
// gets written, the default bypass list, and the two "is this value ours?" questions are ordinary
// string logic and are pinned here.
class TerminalProxyPolicyTests : public QObject {
    Q_OBJECT

private slots:
    // --- proxyUrl ---
    void proxyUrlUsesLoopbackIpv4NotLocalhost();
    void proxyUrlRejectsAnInvalidPort();

    // --- defaultNoProxy ---
    void defaultNoProxyCoversLoopbackAndPrivateRanges();
    void defaultNoProxyHasNoSpacesAfterCommas();

    // --- isOurValue ---
    void isOurValueMatchesTheExactUrl();
    void isOurValueIgnoresSchemeCase();
    void isOurValueIgnoresATrailingSlash();
    void isOurValueRejectsADifferentPort();
    void isOurValueRejectsADifferentHost();
    void isOurValueRejectsAnEmptyValue();

    // --- shouldRestoreOnShutdown ---
    void invalidSnapshotIsNotRestored();
    void userSnapshotIsRestored();
    void snapshotPointingAtOurPortIsNotRestored();
    void snapshotWithAbsentHttpProxyIsRestored();
};

void TerminalProxyPolicyTests::proxyUrlUsesLoopbackIpv4NotLocalhost()
{
    // A resolver that maps localhost to ::1 first would reach a listener bound only to IPv4 and fail,
    // and the failure would look like a broken proxy rather than a wrong loopback spelling.
    const QString url = TerminalProxyPolicy::proxyUrl(2080);
    QCOMPARE(url, QStringLiteral("http://127.0.0.1:2080"));
    QVERIFY(!url.contains(QStringLiteral("localhost")));
}

void TerminalProxyPolicyTests::proxyUrlRejectsAnInvalidPort()
{
    QVERIFY(TerminalProxyPolicy::proxyUrl(0).isEmpty());
    QVERIFY(TerminalProxyPolicy::proxyUrl(-1).isEmpty());
    QVERIFY(TerminalProxyPolicy::proxyUrl(65536).isEmpty());
}

void TerminalProxyPolicyTests::defaultNoProxyCoversLoopbackAndPrivateRanges()
{
    const QString noProxy = TerminalProxyPolicy::defaultNoProxy();
    QVERIFY(noProxy.contains(QStringLiteral("localhost")));
    QVERIFY(noProxy.contains(QStringLiteral("127.0.0.1")));
    QVERIFY(noProxy.contains(QStringLiteral("::1")));
    QVERIFY(noProxy.contains(QStringLiteral("10.0.0.0/8")));
    QVERIFY(noProxy.contains(QStringLiteral("172.16.0.0/12")));
    QVERIFY(noProxy.contains(QStringLiteral("192.168.0.0/16")));
}

void TerminalProxyPolicyTests::defaultNoProxyHasNoSpacesAfterCommas()
{
    // At least one consumer splits on commas and keeps the space, turning " 127.0.0.1" into an
    // entry that never matches.
    const QString noProxy = TerminalProxyPolicy::defaultNoProxy();
    QVERIFY(!noProxy.contains(QStringLiteral(", ")));
}

void TerminalProxyPolicyTests::isOurValueMatchesTheExactUrl()
{
    QVERIFY(TerminalProxyPolicy::isOurValue(QStringLiteral("http://127.0.0.1:2080"), 2080));
}

void TerminalProxyPolicyTests::isOurValueIgnoresSchemeCase()
{
    QVERIFY(TerminalProxyPolicy::isOurValue(QStringLiteral("HTTP://127.0.0.1:2080"), 2080));
}

void TerminalProxyPolicyTests::isOurValueIgnoresATrailingSlash()
{
    QVERIFY(TerminalProxyPolicy::isOurValue(QStringLiteral("http://127.0.0.1:2080/"), 2080));
}

void TerminalProxyPolicyTests::isOurValueRejectsADifferentPort()
{
    QVERIFY(!TerminalProxyPolicy::isOurValue(QStringLiteral("http://127.0.0.1:9999"), 2080));
}

void TerminalProxyPolicyTests::isOurValueRejectsADifferentHost()
{
    QVERIFY(!TerminalProxyPolicy::isOurValue(QStringLiteral("http://10.0.0.1:2080"), 2080));
}

void TerminalProxyPolicyTests::isOurValueRejectsAnEmptyValue()
{
    QVERIFY(!TerminalProxyPolicy::isOurValue(QString(), 2080));
    QVERIFY(!TerminalProxyPolicy::isOurValue(QStringLiteral("   "), 2080));
}

void TerminalProxyPolicyTests::invalidSnapshotIsNotRestored()
{
    TerminalProxySnapshot snapshot;  // valid == false
    QVERIFY(!TerminalProxyPolicy::shouldRestoreOnShutdown(snapshot, 2080));
}

void TerminalProxyPolicyTests::userSnapshotIsRestored()
{
    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = QStringLiteral("http://corp-proxy.example.com:3128");
    snapshot.httpsProxy = QStringLiteral("http://corp-proxy.example.com:3128");
    QVERIFY(TerminalProxyPolicy::shouldRestoreOnShutdown(snapshot, 2080));
}

void TerminalProxyPolicyTests::snapshotPointingAtOurPortIsNotRestored()
{
    // The crashed-session case: the value captured was ours, so restoring it would re-apply a dead
    // endpoint. The caller clears instead.
    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = QStringLiteral("http://127.0.0.1:2080");
    QVERIFY(!TerminalProxyPolicy::shouldRestoreOnShutdown(snapshot, 2080));
}

void TerminalProxyPolicyTests::snapshotWithAbsentHttpProxyIsRestored()
{
    // No HTTP_PROXY before us, but a user ALL_PROXY: still the user's environment to get back.
    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = std::nullopt;
    snapshot.allProxy = QStringLiteral("socks5://127.0.0.1:1080");
    QVERIFY(TerminalProxyPolicy::shouldRestoreOnShutdown(snapshot, 2080));
}

QTEST_GUILESS_MAIN(TerminalProxyPolicyTests)
#include "TerminalProxyPolicyTests.moc"
