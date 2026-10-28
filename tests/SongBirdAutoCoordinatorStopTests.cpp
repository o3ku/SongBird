#include <QtTest>
#include <QTemporaryDir>

#include <memory>
#include <utility>

#include "auto/SongBirdAutoCoordinator.h"
#include "platform/ISystemProxyService.h"

// The coordinator's own teardown, pinned from the outside.
//
// Why this file exists: the deletion in issue #12 -- the unconditional ForcedClear that ran after
// the session's ownership undo, on the immediate path (stopProxySession(true), the destructor's
// exit) and in stop() -- had no coverage at all. The mutation check proved it: put either line
// back and every suite stayed green, because ProxySessionTests drives a ProxySession whose
// activation coordinator is the test's own and which therefore cannot see the coordinator.
//
// What had to change for this to be possible, both of them small and neither of them a policy
// change: the coordinator takes its ISystemProxyService as a constructor argument (defaulted to
// the platform implementation), and createProxySession() is split out of initialize() so a test
// can build the session without a config file, a core, or the network.

namespace {

// Counts every registry write the coordinator makes. A no-op service would reproduce exactly the
// old green: the bug was a *second* write, and only something that counts writes can see one.
// restore() is counted separately from update() so "the undo ran" and "something ran after it"
// stay distinguishable.
class RecordingSystemProxyService final : public ISystemProxyService
{
public:
    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override
    {
        Q_UNUSED(httpPort)
        Q_UNUSED(socksPort)
        Q_UNUSED(proxyExceptions)
        Q_UNUSED(advancedProtocol)
        ++updateCount;
        lastMode = mode;
        return updateSucceeds;
    }

    bool isEnabled() const override { return enabled; }
    void resetOnShutdown() const override { ++resetCount; }
    SystemProxySnapshot capture() const override { return captured; }

    bool restore(const SystemProxySnapshot& snapshot) const override
    {
        ++restoreCount;
        lastRestored = snapshot;
        return restoreSucceeds;
    }

    mutable int updateCount = 0;
    mutable int restoreCount = 0;
    mutable int resetCount = 0;
    mutable SystemProxyMode lastMode = SystemProxyMode::ForcedClear;
    mutable SystemProxySnapshot lastRestored;
    bool updateSucceeds = true;
    bool restoreSucceeds = true;
    bool enabled = false;
    SystemProxySnapshot captured;
};

// Forwards to the recorder while the coordinator owns it. The counters have to outlive the
// coordinator, because the exit case destroys the subject and then asserts on what it wrote.
class RecordingProxyForwarder final : public ISystemProxyService
{
public:
    explicit RecordingProxyForwarder(std::shared_ptr<RecordingSystemProxyService> recorder)
        : recorder_(std::move(recorder))
    {
    }

    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override
    {
        return recorder_->update(mode, httpPort, socksPort, proxyExceptions, advancedProtocol);
    }
    bool isEnabled() const override { return recorder_->isEnabled(); }
    void resetOnShutdown() const override { recorder_->resetOnShutdown(); }
    SystemProxySnapshot capture() const override { return recorder_->capture(); }
    bool restore(const SystemProxySnapshot& snapshot) const override
    {
        return recorder_->restore(snapshot);
    }

private:
    std::shared_ptr<RecordingSystemProxyService> recorder_;
};

// The real coordinator, wired by the real createProxySession(), with the registry writer replaced
// by the recorder.
class Harness {
public:
    Harness()
        : recorder_(std::make_shared<RecordingSystemProxyService>())
    {
        coordinator_ = std::make_unique<SongBirdAutoCoordinator>(
            QStringLiteral("songbird-auto-test.json"),
            std::unique_ptr<ISystemProxyService>(new RecordingProxyForwarder(recorder_)));
        // Not initialize(): the point is that the teardown half is reachable without a config
        // file, a core or the network. Nothing about initialize() is being stubbed -- the real
        // wiring that builds the session is the production one.
        coordinator_->createProxySession();
    }

    // initialize() for a case that needs to *start* something: the TUN abort runs before
    // ProxySession::start(), so it needs the core hosts, the runtime resolver and the runtime
    // environment initialize() builds -- createProxySession() alone leaves them unset.
    void initializeForStart()
    {
        if (!coordinator_->initialize()) {
            qFatal("SongBirdAutoCoordinator::initialize() failed");
        }
    }

    SongBirdAutoCoordinator& coordinator() { return *coordinator_; }
    RecordingSystemProxyService& proxy() const { return *recorder_; }

    // Destroys the coordinator while the recorder stays alive: this is the exit path, where the
    // destructor's stopProxySession(true) is the last thing that can write.
    void destroyCoordinator() { coordinator_.reset(); }

private:
    std::shared_ptr<RecordingSystemProxyService> recorder_;
    std::unique_ptr<SongBirdAutoCoordinator> coordinator_;
};

} // namespace

class SongBirdAutoCoordinatorStopTests : public QObject
{
    Q_OBJECT

private slots:
    void initializeBuildsTheEnvironmentItWires();
    void stopLeavesAnUnownedProxyAlone();
    void exitLeavesAnUnownedProxyAlone();
    void aFailedTunStartGivesEveryFlagBack();
    void aFailureAfterTheScheduledStopEndsTheAttemptOnce();
};

// Regression pin for the seam refactor: initialize() called wireRuntimeEnvironment() before
// anything had constructed runtimeEnvironment_ -- only the standalone createProxySession()
// path built one -- which null-derefed on the first fn assignment. Invisible while no test
// drove initialize(); this is the first that does (a real one: real config files in a real
// temporary directory, the injected recorder as the only double).
void SongBirdAutoCoordinatorStopTests::initializeBuildsTheEnvironmentItWires()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto recorder = std::make_shared<RecordingSystemProxyService>();
    // Declaration order is destruction order: the coordinator must die before the directory it
    // writes into.

    // A machine with a leftover songbird_tun adapter pays two 30s PowerShell cleanup
    // timeouts here (startup sync + destructor); CI is Linux and skips the cleanup entirely.
    SongBirdAutoCoordinator coordinator(
        directory.filePath(QStringLiteral("songbird-auto-test.json")),
        std::unique_ptr<ISystemProxyService>(new RecordingProxyForwarder(recorder)));

    QVERIFY(coordinator.initialize());
    QCOMPARE(recorder->updateCount, 0);
    QCOMPARE(recorder->restoreCount, 0);
}

// The stop the user presses on a session that never applied a proxy. The deleted line wrote
// ForcedClear unconditionally at the end of stop(); the registry is reported as on, which is what
// a user's own proxy looks like from here, so the ownership decision must LeaveAlone and a session
// that never wrote must not write.
void SongBirdAutoCoordinatorStopTests::stopLeavesAnUnownedProxyAlone()
{
    Harness harness;
    harness.proxy().enabled = true;

    harness.coordinator().stop();

    QCOMPARE(harness.proxy().updateCount, 0);
    QCOMPARE(harness.proxy().restoreCount, 0);
    QVERIFY(harness.proxy().enabled);
    QVERIFY(!harness.coordinator().isProxyActive());
}

// The exit. The destructor calls stopProxySession(true), and the deleted line sat *after* that
// call: the ordering is what made it a second undo rather than a fallback, so what this pins is
// that the exit performs no registry write of its own. Same idle-session shape as the stop above,
// because that is the shape where the second write was visible as a deletion of somebody else's
// proxy rather than a rewrite of ours.
void SongBirdAutoCoordinatorStopTests::exitLeavesAnUnownedProxyAlone()
{
    Harness harness;
    harness.proxy().enabled = true;

    harness.destroyCoordinator();

    QCOMPARE(harness.proxy().updateCount, 0);
    QCOMPARE(harness.proxy().restoreCount, 0);
    QVERIFY(harness.proxy().enabled);
}

// Issue #14, item 1. A TUN failure aborts the start before ProxySession::start() ever runs, and
// that path used to return with running_ true and busy_ stuck: the window showed "Stopped" with
// its Stop button disabled by busy_, so the user could neither see what was happening nor end it.
// The rollback has to land before the flags are announced, too -- busyChanged is emitted inside
// setBusy(), and its listeners read the status line.
void SongBirdAutoCoordinatorStopTests::aFailedTunStartGivesEveryFlagBack()
{
    // initialize() rather than createProxySession(): the abort is decided before
    // ProxySession::start() is ever called, so the core hosts, the runtime resolver and the
    // runtime environment all have to exist. Nothing is scripted -- the environment is the
    // production one, and on a machine where TUN cannot start (no elevation, no sing-box, no
    // adapter to clean) it reports exactly the failure this case is about.
    Harness harness;
    harness.initializeForStart();
    harness.coordinator().setTunEnabled(true);
    QVERIFY(harness.coordinator().isTunEnabled());

    struct Observation {
        int busyEvents = 0;
        bool busyDuringLastEvent = false;
        bool runningDuringLastEvent = false;
        QString status;
        QString statusDuringLastBusyEvent;
    };
    auto observed = std::make_shared<Observation>();
    SongBirdAutoCoordinator& coordinator = harness.coordinator();
    QObject::connect(&coordinator, &SongBirdAutoCoordinator::statusMessageChanged, &coordinator,
        [observed](const QString& message) { observed->status = message; });
    QObject::connect(&coordinator, &SongBirdAutoCoordinator::busyChanged, &coordinator,
        [observed, &coordinator](bool busy) {
            observed->busyEvents += 1;
            observed->busyDuringLastEvent = busy;
            observed->runningDuringLastEvent = coordinator.isRunning();
            observed->statusDuringLastBusyEvent = observed->status;
        });

    coordinator.startSessionForTest();

    // The pair the failure used to leave out of step: the status says stopped, the flags said
    // otherwise, and busy_ is what disables the Stop button the user would need.
    QVERIFY(!coordinator.isRunning());
    QVERIFY(observed->status.startsWith(QStringLiteral("Stopped")));
    QVERIFY2(observed->status.contains(QStringLiteral("TUN direct mode failed")),
        qPrintable(QStringLiteral("status was: %1").arg(observed->status)));

    // Ordering: the busy flag has to be released *after* the status is final. Emitted before it,
    // the listener reacting to "not busy any more" renders the line from before the failure --
    // the inversion the live UI only hid because Qt coalesces event-loop turns.
    // Two announcements: the start claims the flag, the rollback releases it. Only the release is
    // observed here; the claim precedes the status that describes the failure.
    QCOMPARE(observed->busyEvents, 2);
    QVERIFY(!observed->busyDuringLastEvent);
    QVERIFY(!observed->runningDuringLastEvent);
    QCOMPARE(observed->statusDuringLastBusyEvent, observed->status);
}

// Issue #14 item 2, one layer out. ProxySession gives its scheduled resume an identity so a stop
// cancels it; the coordinator arms its *own* "start again once the session is down" behind it, and
// that flag used to be read only by the two handlers that resume. It is now read by the failure
// handler as well, because a failure is an ending too:
//
//   user starts -> a restart is armed behind a stop (pendingStartAfterStop_)
//   user stops  -> the session schedules `stopped()`
//   the TUN cleanup the exit began fails -> `failed()` arrives *synchronously, before* `stopped()`
//   `stopped()` then arrives -> the flag is still set -> startProxySession() on a stopped session
//
// The ordering is what this drives, through the two production handlers: first the failure, which
// has to end the attempt and drop the flag, then the `stopped()` it was scheduled ahead of, which
// must find nothing left to resume.
void SongBirdAutoCoordinatorStopTests::aFailureAfterTheScheduledStopEndsTheAttemptOnce()
{
    Harness harness;
    harness.proxy().enabled = true;
    SongBirdAutoCoordinator& coordinator = harness.coordinator();

    int runningEvents = 0;
    bool runningDuringLastEvent = true;
    QObject::connect(&coordinator, &SongBirdAutoCoordinator::runningChanged, &coordinator,
        [&runningEvents, &runningDuringLastEvent](bool running) {
            ++runningEvents;
            runningDuringLastEvent = running;
        });

    // The shape the session is in when the user presses Stop with a restart armed: running and
    // busy, waiting on an exit it has scheduled. stop() itself is not called -- it releases
    // busy_ whenever the session has no core running, which this one never had, so the interrupt
    // has to be simulated. What the case is about is the two endings that follow it.
    coordinator.setRunningForTest(true);
    coordinator.setBusyForTest(true);
    coordinator.armPendingStartAfterStopForTest();
    coordinator.stopRequestedForTest();

    QVERIFY(!coordinator.isRunning());
    QVERIFY(coordinator.isBusyForTest());

    // The TUN cleanup the exit began reports a failure -- synchronously, ahead of the `stopped`
    // that same exit scheduled. finishStopped() reconfigures the adapter, so the settlement is
    // driven over the second seam: what this case is about is the flags, not what the adapter
    // says, and a real TUN start would make the outcome depend on the machine running the test.
    coordinator.settleStoppedStateForTest(QStringLiteral("TUN adapter cleanup failed"));

    // Then the scheduled exit arrives.
    coordinator.handleSessionStoppedForTest();

    // One close-out: the flags went down once, in the failure handler, and the `stopped` that
    // followed found the attempt already over. A resume would have put running back to true.
    QVERIFY(!coordinator.isRunning());
    QVERIFY(!coordinator.isBusyForTest());
    QCOMPARE(runningEvents, 1);
    QVERIFY(!runningDuringLastEvent);
}

QTEST_MAIN(SongBirdAutoCoordinatorStopTests)
#include "SongBirdAutoCoordinatorStopTests.moc"
