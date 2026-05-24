#include <QtTest>

#include <QSignalSpy>
#include <QThread>
#include <QTemporaryDir>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "app/BackgroundTaskCoordinator.h"
#include "app/OutboundLocationProbeService.h"
#include "app/ProxySession.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/ICoreProcessHost.h"

class ProxySessionTests : public QObject
{
    Q_OBJECT

private slots:
    void startFailsWhenNoActiveServer();
    void startCancelsBackgroundTasksOnce();
    void tunCleanupResumeDoesNotCancelBackgroundTasksAgain();
    void checklistOverlayCanBeRequestedDuringActivation();
    void serverWarningIsOwnedBySession();
    void startFailureCanRequestManualVerificationWarning();
    void stopForwardsToCoreHosts();
    void coreRunningReflectsProcessStateBeforeActive();
    void restartDoesNothingWhenCoreStopped();
    void switchServerEmitsResumeOnceAfterStopped();
    void switchServerDoesNothingWhenCoreStopped();
    void stopForCoreUpdateEmitsResumeOnceAfterStopped();
    void stopForCoreUpdateDoesNothingWhenCoreStopped();
    void stopCancelsPendingServerSwitchResume();
    void stopCancelsPendingCoreUpdateResume();
    void stopCancelsPendingRestartResume();
    void stopDuringTunCleanupCancelsStartupResume();
    void stopClearsAdoptedManagedSystemProxy();
};

namespace {

class FakeCoreProcessHost final : public ICoreProcessHost
{
public:
    OperationResult start(
        const CoreInfo& coreInfo,
        const QString& configFilePath,
        std::function<void(const QString&)> outputReceived,
        StartedCallback started = {},
        StartFailedCallback startFailed = {},
        ExitedCallback exited = {}) override
    {
        Q_UNUSED(coreInfo)
        Q_UNUSED(configFilePath)
        outputReceived_ = std::move(outputReceived);
        startedCallback_ = std::move(started);
        startFailedCallback_ = std::move(startFailed);
        exitedCallback_ = std::move(exited);
        ++startCount;
        running = true;
        return OperationResult::ok(QStringLiteral("started"));
    }

    OperationResult stop(bool immediate = false) override
    {
        ++stopCount;
        lastImmediate = immediate;
        running = false;
        if (emitExitOnStop && exitedCallback_) {
            exitedCallback_(0, QProcess::NormalExit, true);
        }
        return OperationResult::ok(QStringLiteral("stopped"));
    }

    OperationResult reload() override
    {
        ++reloadCount;
        return OperationResult::ok(QStringLiteral("reloaded"));
    }

    bool isRunning() const override
    {
        return running;
    }

    bool running = false;
    int startCount = 0;
    int stopCount = 0;
    int reloadCount = 0;
    bool lastImmediate = false;
    bool emitExitOnStop = false;

    void triggerExited(bool stopRequested = true)
    {
        if (exitedCallback_) {
            exitedCallback_(0, QProcess::NormalExit, stopRequested);
        }
    }

    void setExitedCallback(ExitedCallback exited)
    {
        exitedCallback_ = std::move(exited);
    }

private:
    std::function<void(const QString&)> outputReceived_;
    StartedCallback startedCallback_;
    StartFailedCallback startFailedCallback_;
    ExitedCallback exitedCallback_;
};

struct ProxySessionHarness
{
    ProxySessionHarness()
        : configWriter(tempDir.path())
    {
        resolver.resolveActiveServer = []() { return std::optional<VmessItem>{}; };
        resolver.resolveLaunchCoreType = [](const VmessItem& server) { return ::resolveRuntimeCoreType(server.coreType); };
        resolver.resolveCoreInfo = [](const VmessItem&) { return CoreInfo{}; };
        resolver.resolveRuntimeConfigPath = [this](const VmessItem&) {
            return tempDir.filePath(QStringLiteral("runtime.json"));
        };
        resolver.resolveCoreCandidates = [](CoreType) { return QStringList{}; };
        resolver.locateFirstExistingFile = [](const QStringList&) { return QString{}; };
        resolver.cleanupPortProcesses = []() {};
        resolver.removeStaleSingBoxCache = []() {};
        resolver.refreshExistingCoreTypes = []() {};
        resolver.removeStaleTunAdapter = [this]() {
            ++tunCleanupCount;
            if (tunCleanupDelayMs > 0) {
                QThread::msleep(tunCleanupDelayMs);
            }
            return tunCleanupResult;
        };
        resolver.skipCoreChecks = []() { return true; };
        resolver.isWindowsPlatform = []() { return false; };
        resolver.isProcessElevated = []() { return true; };
        resolver.isSystemProxyEnabled = [this]() { return systemProxyEnabled; };
        resolver.cancelBackgroundTasksForStartup = [this]() { ++cancelBackgroundTaskCount; };
        resolver.currentIndexId = []() { return QString{}; };
        resolver.resolveRuntimeCoreType = [](CoreType type) { return ::resolveRuntimeCoreType(type); };
        resolver.resolveCoreInstallDirectory = [this](CoreType) { return tempDir.path(); };
        resolver.updateSystemProxyMode = [this](SystemProxyMode mode) {
            ++proxyUpdateCount;
            lastProxyMode = mode;
            if (proxyUpdateSucceeds) {
                systemProxyEnabled = expectedSystemProxyEnabled(mode);
            }
            return proxyUpdateSucceeds;
        };

        session = std::make_unique<ProxySession>(ProxySession::Dependencies{
            mainCore,
            auxiliaryCore,
            configWriter,
            locationProbe,
            backgroundTasks,
            resolver
        });
    }

    QTemporaryDir tempDir;
    FakeCoreProcessHost mainCore;
    FakeCoreProcessHost auxiliaryCore;
    ClientConfigWriter configWriter;
    OutboundLocationProbeService locationProbe;
    BackgroundTaskCoordinator backgroundTasks;
    ProxySession::RuntimeResolver resolver;
    std::unique_ptr<ProxySession> session;
    bool systemProxyEnabled = false;
    bool proxyUpdateSucceeds = true;
    int proxyUpdateCount = 0;
    int cancelBackgroundTaskCount = 0;
    std::atomic_int tunCleanupCount = 0;
    int tunCleanupDelayMs = 0;
    OperationResult tunCleanupResult = OperationResult::ok(QStringLiteral("tun cleanup skipped"));
    SystemProxyMode lastProxyMode = SystemProxyMode::ForcedClear;
};

} // namespace

void ProxySessionTests::startFailsWhenNoActiveServer()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));

    ProxySession::StartRequest request;
    request.config = Config{};
    harness.session->start(request);

    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(failedSpy.takeFirst().at(0).toString().contains(QStringLiteral("No active server")));
    QCOMPARE(harness.mainCore.startCount, 0);
    QVERIFY(!harness.session->isActive());
    QVERIFY(!harness.session->isActivationInProgress());
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

void ProxySessionTests::startCancelsBackgroundTasksOnce()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    ProxySession::StartRequest request;
    request.config = Config{};
    harness.session->start(request);

    QCOMPARE(harness.cancelBackgroundTaskCount, 1);
}

void ProxySessionTests::tunCleanupResumeDoesNotCancelBackgroundTasksAgain()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    harness.tunCleanupResult = OperationResult::ok(QStringLiteral("tun cleanup completed"));

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));

    ProxySession::StartRequest request;
    request.config = Config{};
    request.config.tun().tunModeItem.enableTun = true;
    harness.session->start(request);

    QTRY_COMPARE(failedSpy.count(), 1);
    QCOMPARE(harness.cancelBackgroundTaskCount, 1);
    QCOMPARE(harness.tunCleanupCount.load(), 1);
}

void ProxySessionTests::checklistOverlayCanBeRequestedDuringActivation()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    harness.tunCleanupDelayMs = 100;
    harness.tunCleanupResult = OperationResult::ok(QStringLiteral("tun cleanup completed"));

    QSignalSpy checklistSpy(harness.session.get(), SIGNAL(checklistUpdated(QStringList)));
    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));

    ProxySession::StartRequest request;
    request.config = Config{};
    request.config.tun().tunModeItem.enableTun = true;
    request.showOverlay = false;
    harness.session->start(request);

    QCOMPARE(checklistSpy.count(), 0);
    harness.session->requestChecklistOverlay();

    QTRY_VERIFY(checklistSpy.count() > 0);
    const QStringList checklist = checklistSpy.takeFirst().at(0).toStringList();
    QVERIFY(!checklist.isEmpty());
    QVERIFY(checklist.join(QStringLiteral("\n")).contains(QStringLiteral("Environment cleanup")));

    QTRY_COMPARE(failedSpy.count(), 1);
}

void ProxySessionTests::serverWarningIsOwnedBySession()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    QSignalSpy syncSpy(harness.session.get(), SIGNAL(statusSyncRequested()));

    harness.session->setServerWarning(QStringLiteral("Please verify manually"));

    QCOMPARE(harness.session->serverWarning(), QStringLiteral("Please verify manually"));
    QCOMPARE(syncSpy.count(), 1);

    harness.session->setServerWarning({});

    QCOMPARE(harness.session->serverWarning(), QString());
    QCOMPARE(syncSpy.count(), 2);
}

void ProxySessionTests::startFailureCanRequestManualVerificationWarning()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));

    ProxySession::StartRequest request;
    request.config = Config{};
    request.warnOnFailure = true;
    harness.session->start(request);

    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(harness.session->serverWarning(), QStringLiteral("Please verify manually"));
}

void ProxySessionTests::stopForwardsToCoreHosts()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;
    harness.auxiliaryCore.running = true;

    harness.session->stop(true);

    QCOMPARE(harness.mainCore.stopCount, 1);
    QVERIFY(harness.mainCore.lastImmediate);
    QCOMPARE(harness.auxiliaryCore.stopCount, 1);
    QVERIFY(harness.auxiliaryCore.lastImmediate);
    QVERIFY(!harness.session->isActive());
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

void ProxySessionTests::coreRunningReflectsProcessStateBeforeActive()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;

    QVERIFY(harness.session->isCoreRunning());
    QVERIFY(!harness.session->isActive());
}

void ProxySessionTests::restartDoesNothingWhenCoreStopped()
{
    ProxySessionHarness harness;

    ProxySession::StartRequest request;
    request.config = Config{};
    request.showOverlay = true;
    harness.session->restartIfRunning(request);
    harness.mainCore.triggerExited(true);

    QCOMPARE(harness.mainCore.stopCount, 0);
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

void ProxySessionTests::switchServerEmitsResumeOnceAfterStopped()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;
    harness.mainCore.emitExitOnStop = true;
    harness.mainCore.setExitedCallback([&harness](int code, QProcess::ExitStatus status, bool stopRequested) {
        harness.session->handleCoreExitedForTest(code, status, stopRequested, false);
    });

    QSignalSpy resumeSpy(harness.session.get(), SIGNAL(serverSwitchResumeRequested(QString,bool,bool)));

    harness.session->switchServer(QStringLiteral("server-2"), false, true);

    QCOMPARE(harness.mainCore.stopCount, 1);
    QCOMPARE(resumeSpy.count(), 1);
    const QList<QVariant> arguments = resumeSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("server-2"));
    QCOMPARE(arguments.at(1).toBool(), false);
    QCOMPARE(arguments.at(2).toBool(), true);

    harness.mainCore.triggerExited(true);
    QCOMPARE(resumeSpy.count(), 0);
}

void ProxySessionTests::switchServerDoesNothingWhenCoreStopped()
{
    ProxySessionHarness harness;

    QSignalSpy resumeSpy(harness.session.get(), SIGNAL(serverSwitchResumeRequested(QString,bool,bool)));

    harness.session->switchServer(QStringLiteral("server-2"), false, true);
    harness.mainCore.triggerExited(true);

    QCOMPARE(harness.mainCore.stopCount, 0);
    QCOMPARE(resumeSpy.count(), 0);
}

void ProxySessionTests::stopForCoreUpdateEmitsResumeOnceAfterStopped()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;
    harness.mainCore.emitExitOnStop = true;
    harness.mainCore.setExitedCallback([&harness](int code, QProcess::ExitStatus status, bool stopRequested) {
        harness.session->handleCoreExitedForTest(code, status, stopRequested, false);
    });

    QSignalSpy resumeSpy(harness.session.get(), SIGNAL(coreUpdateResumeRequested()));

    harness.session->stopForCoreUpdate();

    QCOMPARE(harness.mainCore.stopCount, 1);
    QCOMPARE(resumeSpy.count(), 1);

    harness.mainCore.triggerExited(true);
    QCOMPARE(resumeSpy.count(), 1);
}

void ProxySessionTests::stopForCoreUpdateDoesNothingWhenCoreStopped()
{
    ProxySessionHarness harness;

    QSignalSpy resumeSpy(harness.session.get(), SIGNAL(coreUpdateResumeRequested()));

    harness.session->stopForCoreUpdate();
    harness.mainCore.triggerExited(true);

    QCOMPARE(harness.mainCore.stopCount, 0);
    QCOMPARE(resumeSpy.count(), 0);
}

void ProxySessionTests::stopCancelsPendingServerSwitchResume()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;

    QSignalSpy resumeSpy(harness.session.get(), SIGNAL(serverSwitchResumeRequested(QString,bool,bool)));

    harness.session->switchServer(QStringLiteral("server-2"), false, true);
    harness.session->stop(true);
    harness.mainCore.triggerExited(true);

    QCOMPARE(resumeSpy.count(), 0);
}

void ProxySessionTests::stopCancelsPendingCoreUpdateResume()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;

    QSignalSpy resumeSpy(harness.session.get(), SIGNAL(coreUpdateResumeRequested()));

    harness.session->stopForCoreUpdate();
    harness.session->stop(true);
    harness.mainCore.triggerExited(true);

    QCOMPARE(resumeSpy.count(), 0);
}

void ProxySessionTests::stopCancelsPendingRestartResume()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;

    ProxySession::StartRequest request;
    request.config = Config{};
    request.showOverlay = true;

    harness.session->restartIfRunning(request);
    harness.session->stop(true);
    harness.mainCore.triggerExited(true);

    QVERIFY(harness.mainCore.stopCount >= 1);
    QCOMPARE(harness.mainCore.startCount, 0);
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

void ProxySessionTests::stopDuringTunCleanupCancelsStartupResume()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    harness.tunCleanupDelayMs = 100;
    harness.tunCleanupResult = OperationResult::ok(QStringLiteral("tun cleanup completed"));

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));

    ProxySession::StartRequest request;
    request.config = Config{};
    request.config.tun().tunModeItem.enableTun = true;
    harness.session->start(request);

    QTRY_COMPARE(harness.tunCleanupCount.load(), 1);
    harness.session->stop(true);

    QTest::qWait(150);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(harness.mainCore.startCount, 0);
    QVERIFY(!harness.session->isActivationInProgress());
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

void ProxySessionTests::stopClearsAdoptedManagedSystemProxy()
{
    ProxySessionHarness harness;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);

    harness.session->stop(false);

    QCOMPARE(harness.proxyUpdateCount, 1);
    QCOMPARE(static_cast<int>(harness.lastProxyMode), static_cast<int>(SystemProxyMode::ForcedClear));
    QVERIFY(!harness.session->isManagedProxyActive());
    QVERIFY(!harness.systemProxyEnabled);
}

QTEST_MAIN(ProxySessionTests)

#include "ProxySessionTests.moc"
