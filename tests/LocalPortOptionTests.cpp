#include <QtTest>

#include "common/LocalPortOption.h"

class LocalPortOptionTests : public QObject {
    Q_OBJECT

private slots:
    void keepsAValidPort();
    void foldsAnUnsetPortIntoTheDefault();
    void foldsOutOfRangePortsIntoTheDefault();
    void keepsTheRangeBoundaries();
    void agreesWithTheRangeTheConfigStores();
};

void LocalPortOptionTests::keepsAValidPort()
{
    QCOMPARE(effectiveLocalPort(10808), 10808);
    QCOMPARE(effectiveLocalPort(20808), 20808);
}

void LocalPortOptionTests::foldsAnUnsetPortIntoTheDefault()
{
    // 0 is what a config that never set a port holds, because 10808 is the field's default and
    // the serializer only writes non-default values.
    QCOMPARE(effectiveLocalPort(0), kDefaultLocalPort);
}

void LocalPortOptionTests::foldsOutOfRangePortsIntoTheDefault()
{
    QCOMPARE(effectiveLocalPort(-1), kDefaultLocalPort);
    QCOMPARE(effectiveLocalPort(65536), kDefaultLocalPort);
    QCOMPARE(effectiveLocalPort(100000), kDefaultLocalPort);
}

void LocalPortOptionTests::keepsTheRangeBoundaries()
{
    QCOMPARE(effectiveLocalPort(kMinTcpPort), kMinTcpPort);
    QCOMPARE(effectiveLocalPort(kMaxTcpPort), kMaxTcpPort);
}

void LocalPortOptionTests::agreesWithTheRangeTheConfigStores()
{
    // The fallback belongs to PortValidator's range, not to a second copy of it: a port the
    // validator accepts must never be replaced, or the two would disagree about what is unset.
    for (int port = -2; port <= kMaxTcpPort + 2; ++port) {
        const int resolved = effectiveLocalPort(port);
        if (isValidTcpPort(port)) {
            QCOMPARE(resolved, port);
        } else {
            QCOMPARE(resolved, kDefaultLocalPort);
        }
    }
}

QTEST_MAIN(LocalPortOptionTests)

#include "LocalPortOptionTests.moc"
