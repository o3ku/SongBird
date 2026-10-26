#include <QtTest>

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
    void stopLeavesAnUnownedProxyAlone();
    void exitLeavesAnUnownedProxyAlone();
};

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

QTEST_MAIN(SongBirdAutoCoordinatorStopTests)
#include "SongBirdAutoCoordinatorStopTests.moc"
