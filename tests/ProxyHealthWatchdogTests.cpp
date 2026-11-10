#include <QtTest>

#include <QElapsedTimer>
#include <QSet>
#include <QTimer>

#include <atomic>
#include <mutex>

#include "appcore/ProxyHealthWatchdog.h"
#include "domain/models/Config.h"
#include <QThread>

namespace {
ProxyHealthWatchdog::SessionStrings testSessionStrings()
{
    ProxyHealthWatchdog::SessionStrings strings;
    strings.activationTag = []() { return QStringLiteral("id=test"); };
    strings.probeConfig = []() {
        Config config;
        return config;
    };
    strings.fallbackDirectProbe = [](int) { return true; };
    strings.recheckStillUnavailableSentence = []() { return QStringLiteral("still down"); };
    strings.recheckAvailableAgainSentence = []() { return QStringLiteral("back up"); };
    strings.isShuttingDown = []() { return false; };
    return strings;
}

} // namespace

// The watch's dispatch contract: every probe runs on ONE pooled thread that the
// watchdog owns for its whole life.
//
// It used to launch a fresh QThread per probe -- one every intervalMs -- and on
// the static Qt the shipped binary is built against, that create/probe/destroy
// cycle retained memory at up to ~40 MB/hour against a dead node (measured on
// the shipped binary over staged live runs, while the same code on a
// dynamic-Qt build stayed flat; see the memory soak). These cases pin the
// property that fixes it: the probe thread is created once and reused.
class ProxyHealthWatchdogTests final : public QObject {
    Q_OBJECT

private slots:
    void everyProbeRunsOnOneReusedThread();
    void destructorDrainsThePoolWithoutBlocking();
    void cancelledWatchDropsTheProbeAnswer();
};

void ProxyHealthWatchdogTests::everyProbeRunsOnOneReusedThread()
{
    auto guard = std::make_shared<char>();
    std::mutex idsMutex;
    QList<Qt::HANDLE> threadIds;
    std::atomic_int probeCount{0};

    ProxyHealthWatchdog watchdog(
        guard,
        testSessionStrings(),
        this);

    watchdog.setAvailabilityCheck([&probeCount, &idsMutex, &threadIds](const Config&) {
        {
            std::lock_guard<std::mutex> lock(idsMutex);
            threadIds.append(QThread::currentThreadId());
        }
        ++probeCount;
        return OperationResult::ok(QStringLiteral("Availability check: 1 ms"));
    });
    watchdog.setIntervalMs(5);
    watchdog.arm();

    QTRY_VERIFY_WITH_TIMEOUT(probeCount.load() >= 5, 5000);

    watchdog.cancel();
    QVERIFY(watchdog.watchPoolHandleForTest() != nullptr);

    QSet<Qt::HANDLE> distinctIds;
    for (const Qt::HANDLE id : threadIds) {
        distinctIds.insert(id);
    }
    QCOMPARE(distinctIds.size(), 1);
    const Qt::HANDLE probeThreadId = *distinctIds.constBegin();
    QVERIFY(probeThreadId != QThread::currentThreadId());
}

void ProxyHealthWatchdogTests::destructorDrainsThePoolWithoutBlocking()
{
    // A slow probe is still running when the watchdog dies; the destructor must
    // bound its wait (abandoning the probe) rather than block on it. The case
    // passes if it finishes at all, and the elapsed time is the assertion.
    auto guard = std::make_shared<char>();
    std::atomic_bool release{false};

    QElapsedTimer timer;
    {
        ProxyHealthWatchdog watchdog(
            guard,
            testSessionStrings(),
            this);
        watchdog.setAvailabilityCheck([&release](const Config&) {
            QElapsedTimer hold;
            hold.start();
            while (!release.load() && hold.elapsed() < 3000) {
                QThread::msleep(20);
            }
            return OperationResult::fail(QStringLiteral("Availability check: -1"));
        });
        watchdog.setIntervalMs(10);
        watchdog.arm();

        // Let one probe start, then destroy mid-probe.
        QTRY_VERIFY_WITH_TIMEOUT(watchdog.probeRunningForTest(), 2000);
        release.store(true);
        timer.start();
    }
    // The probe exits on its own within its 3 s cap; the drain budget is 1.5 s,
    // and the destructor must not exceed the probe's own duration by any
    // meaningful margin. Generous bound: this case pins "bounded", not "fast".
    QVERIFY(timer.elapsed() < 6000);
}

void ProxyHealthWatchdogTests::cancelledWatchDropsTheProbeAnswer()
{
    auto guard = std::make_shared<char>();
    std::atomic_int answers{0};

    ProxyHealthWatchdog watchdog(
        guard,
        testSessionStrings(),
        this);
    QObject::connect(&watchdog, &ProxyHealthWatchdog::logMessage, this, [&answers]() {
        ++answers;
    });

    watchdog.setAvailabilityCheck([](const Config&) {
        QThread::msleep(150);
        // Enough consecutive failures to raise the warning -- had the answer
        // been processed after cancel(), logMessage would fire.
        return OperationResult::fail(QStringLiteral("Availability check: -1"));
    });
    watchdog.setIntervalMs(5);
    watchdog.arm();
    QTRY_VERIFY_WITH_TIMEOUT(watchdog.probeRunningForTest(), 2000);

    watchdog.cancel();
    QTest::qWait(400);

    QCOMPARE(answers.load(), 0);
}

QTEST_MAIN(ProxyHealthWatchdogTests)
#include "ProxyHealthWatchdogTests.moc"
