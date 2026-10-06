#include <QtTest>

#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "appcore/BackgroundTaskCoordinator.h"
#include "appcore/OutboundLocationProbeService.h"
#include "appcore/ProxySession.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/ICoreProcessHost.h"

class ProxySessionTests : public QObject
{
    Q_OBJECT

private slots:
    void startFailsWhenNoActiveServer();
    void startReachesProxyingWhenCoreListensAndLocationResolves();
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
    void stopForCoreUpdateKeepsManagedProxyFlagWhenTheClearFails();

    void healthWatchWarnsAfterTwoConsecutiveFailedProbes();
    void healthWatchToleratesASingleFailedProbe();
    void healthWatchClearsTheWarningWhenTheNodeAnswersAgain();
    void healthWatchDoesNotRunWithoutAnInjectedCheck();
    void healthWatchStopsWhenTheSessionStops();
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

    // The real host reports readiness from its own thread; the fake only records the
    // callback so a test can decide when the core "comes up".
    void triggerStarted(const QString& message = QStringLiteral("started"))
    {
        if (startedCallback_) {
            startedCallback_(message);
        }
    }

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

// The location probe hardcodes QNetworkProxy::HttpProxy, so the only way to exercise
// the real probe path (rather than stubbing the service) is to answer it with a real
// proxy. Plain http:// requests reach a proxy in absolute form and get the payload
// back; the https:// ones only need to be refused quickly so they do not hold the
// probe open until its timeout.
class FakeHttpProxyServer
{
public:
    explicit FakeHttpProxyServer(QByteArray payload)
        : payload_(std::move(payload))
    {
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() {
            while (QTcpSocket* socket = server_.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    respond(socket);
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, [this, socket]() {
                    requests_.remove(socket);
                    answered_.remove(socket);
                    socket->deleteLater();
                });
            }
        });
    }

    bool listen()
    {
        return server_.listen(QHostAddress::LocalHost, 0);
    }

    int port() const
    {
        return server_.serverPort();
    }

private:
    void respond(QTcpSocket* socket)
    {
        if (answered_.contains(socket)) {
            return;
        }

        requests_[socket].append(socket->readAll());
        const QByteArray& request = requests_[socket];
        if (!request.contains("\r\n\r\n")) {
            return; // header not complete yet
        }
        answered_.insert(socket);

        if (request.startsWith("CONNECT ")) {
            socket->write("HTTP/1.1 200 Connection Established\r\n\r\n");
            socket->flush();
            socket->disconnectFromHost();
            return;
        }

        QByteArray response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + QByteArray::number(payload_.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += payload_;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
    }

    QTcpServer server_;
    QByteArray payload_;
    QHash<QTcpSocket*, QByteArray> requests_;
    QSet<QTcpSocket*> answered_;
};

QByteArray locationProbePayload()
{
    return QByteArrayLiteral(
        R"({"status":"success","country":"Japan","countryCode":"JP","city":"Tokyo","query":"203.0.113.9"})");
}

struct ProxySessionHarness final
    : public IRuntimeProfileResolver
    , public IRuntimeEnvironment
    , public IProxyActivationCoordinator
{
    ProxySessionHarness()
        : configWriter(tempDir.path())
    {
        session = std::make_unique<ProxySession>(ProxySession::Dependencies{
            mainCore,
            auxiliaryCore,
            configWriter,
            locationProbe,
            backgroundTasks,
            *this,
            *this,
            *this
        });
    }

    std::optional<VmessItem> resolveActiveServer() const override
    {
        return activeServer;
    }

    CoreType resolveLaunchCoreType(const VmessItem& server) const override
    {
        return ::resolveRuntimeCoreType(server.coreType);
    }

    CoreInfo resolveCoreInfo(const VmessItem&) const override
    {
        return coreInfo;
    }

    QString resolveRuntimeConfigPath(const VmessItem&) const override
    {
        return tempDir.filePath(QStringLiteral("runtime.json"));
    }

    QStringList resolveCoreCandidates(CoreType) const override
    {
        return {};
    }

    QString locateFirstExistingFile(const QStringList&) const override
    {
        return {};
    }

    QString currentIndexId() const override
    {
        return currentServerIndexId;
    }

    CoreType resolveRuntimeCoreType(CoreType type) const override
    {
        return ::resolveRuntimeCoreType(type);
    }

    QString resolveCoreInstallDirectory(CoreType) const override
    {
        return tempDir.path();
    }

    void cleanupPortProcesses() override
    {
    }

    OperationResult removeStaleTunAdapter() override
    {
        ++tunCleanupCount;
        if (tunCleanupDelayMs > 0) {
            QThread::msleep(tunCleanupDelayMs);
        }
        return tunCleanupResult;
    }

    bool skipCoreChecks() const override
    {
        return true;
    }

    bool isWindowsPlatform() const override
    {
        return false;
    }

    bool isProcessElevated() const override
    {
        return true;
    }

    void cancelBackgroundTasksForStartup() override
    {
        ++cancelBackgroundTaskCount;
    }

    void refreshExistingCoreTypes() override
    {
        ++refreshExistingCoreTypesCount;
    }

    bool isSystemProxyEnabled() const override
    {
        return systemProxyEnabled;
    }

    bool updateSystemProxyMode(SystemProxyMode mode) override
    {
        ++proxyUpdateCount;
        lastProxyMode = mode;
        if (proxyUpdateSucceeds) {
            systemProxyEnabled = expectedSystemProxyEnabled(mode);
        }
        return proxyUpdateSucceeds;
    }

    // A server and request that let start() run the real startup branch instead of
    // bailing out early: an installed core executable, a local listener the port probe
    // can reach, a location probe that answers, and a system-proxy mode that keeps the
    // session active once the core is up.
    VmessItem launchableServer() const
    {
        VmessItem server;
        server.indexId = QStringLiteral("server-1");
        server.configType = ConfigType::VMess;
        server.coreType = CoreType::SingBox;
        server.address = QStringLiteral("example.com");
        server.port = 443;
        server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
        server.security = QStringLiteral("auto");
        return server;
    }

    ProxySession::StartRequest launchableRequest() const
    {
        ProxySession::StartRequest request;
        request.config = Config{};
        // The listening probe and the location probe must reach the same fake proxy, so
        // the dedicated probe port is pinned to it rather than left to localPort + 103.
        request.config.localPort = proxyServer.port();
        request.config.localLocationProbePort = proxyServer.port();
        request.config.sysProxyType = static_cast<int>(SystemProxyMode::ForcedChange);
        // "Global" carries no geosite/geoip rule, so the generated sing-box config has no
        // remote rule_set and startup does not divert into the rule-set download.
        request.config.collection().routingModeId = QStringLiteral("builtin:global");
        return request;
    }

    QTemporaryDir tempDir;
    FakeHttpProxyServer proxyServer{locationProbePayload()};
    FakeCoreProcessHost mainCore;
    FakeCoreProcessHost auxiliaryCore;
    ClientConfigWriter configWriter;
    OutboundLocationProbeService locationProbe;
    BackgroundTaskCoordinator backgroundTasks;
    std::unique_ptr<ProxySession> session;
    std::optional<VmessItem> activeServer;
    CoreInfo coreInfo;
    QString currentServerIndexId;
    bool systemProxyEnabled = false;
    bool proxyUpdateSucceeds = true;
    int proxyUpdateCount = 0;
    int cancelBackgroundTaskCount = 0;
    int refreshExistingCoreTypesCount = 0;
    std::atomic_int tunCleanupCount = 0;
    int tunCleanupDelayMs = 0;
    OperationResult tunCleanupResult = OperationResult::ok(QStringLiteral("tun cleanup skipped"));
    SystemProxyMode lastProxyMode = SystemProxyMode::ForcedClear;
};

// Large enough that the scripted probe never recovers on its own, so a test that wants a lasting
// outage does not have to pick a number that depends on how many probes happen to run.
constexpr int kScriptAlwaysFails = 1000000;

// A scripted stand-in for the injected availability probe. Probes fail while
// `failuresRemaining` is positive and succeed afterwards, which is how the watch's whole
// tolerate-then-warn-then-clear sequence is driven without a real node. Both counters are atomic
// because the probe runs on a worker thread while the test reads them from the main one.
struct ScriptedAvailability {
    std::atomic_int calls{0};
    std::atomic_int failuresRemaining{0};
};

ProxySession::AvailabilityCheck makeScriptedCheck(const std::shared_ptr<ScriptedAvailability>& script)
{
    return [script](int, const QString&) {
        script->calls.fetch_add(1);
        return script->failuresRemaining.fetch_sub(1) > 0
            ? OperationResult::fail(QStringLiteral("Availability check: -1 ms"))
            : OperationResult::ok(QStringLiteral("Availability check: 12 ms"));
    };
}

// Drives the session through the real startup cascade to Proxying, the only phase the health watch
// runs in. Returns false instead of asserting so the caller can QVERIFY2 it with a message: the
// QTest macros need to `return` from the test function and cannot be used from here.
bool driveToProxying(ProxySessionHarness& harness)
{
    if (!harness.tempDir.isValid() || !harness.proxyServer.listen()) {
        return false;
    }
    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;

    harness.session->start(harness.launchableRequest());
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    return QTest::qWaitFor(
        [&harness]() { return harness.session->phase() == ProxySession::Phase::Proxying; },
        5000);
}

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

void ProxySessionTests::startReachesProxyingWhenCoreListensAndLocationResolves()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;

    QSignalSpy activatedSpy(harness.session.get(), SIGNAL(activated(QString)));
    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));

    harness.session->start(harness.launchableRequest());

    // start() runs the whole synchronous cascade, so the core is already launched by the
    // time it returns and only its readiness callback is still outstanding.
    QCOMPARE(harness.mainCore.startCount, 1);
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::StartCoreProcess));
    QVERIFY(harness.session->isActivationInProgress());

    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    QTRY_COMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Proxying));
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(activatedSpy.count(), 1);
    QVERIFY(harness.session->isActive());
    QVERIFY(!harness.session->isActivationInProgress());
    QVERIFY(harness.session->isManagedProxyActive());
    QVERIFY(harness.session->serverLocation().contains(QStringLiteral("Japan")));
    QCOMPARE(static_cast<int>(harness.lastProxyMode), static_cast<int>(SystemProxyMode::ForcedChange));
    QVERIFY(harness.systemProxyEnabled);

    harness.session->stop(true);
    QCOMPARE(harness.mainCore.stopCount, 1);
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

void ProxySessionTests::stopForCoreUpdateKeepsManagedProxyFlagWhenTheClearFails()
{
    ProxySessionHarness harness;
    harness.mainCore.running = true;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.proxyUpdateSucceeds = false;

    QSignalSpy logSpy(harness.session.get(), SIGNAL(logMessage(QString)));

    // This goes through stopInternal(..., clearPostStopAction = false), the only shape where the
    // clear result decides the flag: a plain stop() also runs clearProxyStateAfterStopped(), which
    // drops it unconditionally. The core is about to restart, so the system proxy stays pointed at
    // the local listener -- which is exactly why the session must not forget that it owns it.
    // The branch became reachable only once the concrete service stopped reporting every
    // ForcedClear as a success.
    harness.session->stopForCoreUpdate();

    QCOMPARE(harness.proxyUpdateCount, 1);
    QCOMPARE(static_cast<int>(harness.lastProxyMode), static_cast<int>(SystemProxyMode::ForcedClear));
    QVERIFY(harness.session->isManagedProxyActive());
    QVERIFY(harness.systemProxyEnabled);
    QVERIFY(logSpy.count() > 0);
    QVERIFY(logSpy.last().at(0).toString().contains(QStringLiteral("Failed to disable")));
}

// --- Node health watch ---
//
// These cover the decision the watch makes, not the probe itself: the probe is injected, so what
// is under test here is when ProxySession starts and stops watching and what it does with each
// verdict. The policy's own arithmetic is pinned separately by proxy-health-watch-policy.

void ProxySessionTests::healthWatchWarnsAfterTwoConsecutiveFailedProbes()
{
    ProxySessionHarness harness;
    auto script = std::make_shared<ScriptedAvailability>();
    script->failuresRemaining = kScriptAlwaysFails;
    harness.session->setAvailabilityCheck(makeScriptedCheck(script), 10);

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");

    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->serverWarning().isEmpty(), 5000);

    // The warning has to name the node and carry the probe's own reason, so a user can tell a dead
    // node from a dead local inbound without opening a log.
    QVERIFY(harness.session->serverWarning().contains(
        QStringLiteral("consecutive health checks failed")));
    QVERIFY(harness.session->serverWarning().contains(QStringLiteral("Availability check")));

    // Two failures are needed, so a single probe must not have been enough.
    QVERIFY2(
        script->calls.load() >= 2,
        qPrintable(QStringLiteral("only %1 probe(s) ran").arg(script->calls.load())));
}

void ProxySessionTests::healthWatchToleratesASingleFailedProbe()
{
    ProxySessionHarness harness;
    auto script = std::make_shared<ScriptedAvailability>();
    script->failuresRemaining = 1; // one blip, then healthy
    harness.session->setAvailabilityCheck(makeScriptedCheck(script), 50);

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");

    // Long enough for several probes: the first fails, the rest succeed, and the warning must never
    // appear. The core re-dials the node per connection, so a single missed probe is ordinary
    // jitter that heals itself; warning on it would only train the user to ignore the status bar.
    QTest::qWait(700);

    QVERIFY2(
        script->calls.load() >= 3,
        qPrintable(QStringLiteral("only %1 probe(s) ran").arg(script->calls.load())));
    QVERIFY(harness.session->serverWarning().isEmpty());
}

void ProxySessionTests::healthWatchClearsTheWarningWhenTheNodeAnswersAgain()
{
    ProxySessionHarness harness;
    auto script = std::make_shared<ScriptedAvailability>();
    script->failuresRemaining = kScriptAlwaysFails;
    harness.session->setAvailabilityCheck(makeScriptedCheck(script), 10);

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");
    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->serverWarning().isEmpty(), 5000);

    script->failuresRemaining = 0; // the node answers again

    QTRY_VERIFY_WITH_TIMEOUT(harness.session->serverWarning().isEmpty(), 5000);
}

void ProxySessionTests::healthWatchDoesNotRunWithoutAnInjectedCheck()
{
    // This is the auto front end's opt-out, and it is load-bearing: SongBirdAuto already runs its
    // own health check every 60 s, so a second watch inside ProxySession would probe the same node
    // twice and could raise a warning that window does not render.
    ProxySessionHarness harness;

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");

    QTest::qWait(300);

    QVERIFY(harness.session->serverWarning().isEmpty());
}

void ProxySessionTests::healthWatchStopsWhenTheSessionStops()
{
    ProxySessionHarness harness;
    auto script = std::make_shared<ScriptedAvailability>();
    script->failuresRemaining = kScriptAlwaysFails;
    harness.session->setAvailabilityCheck(makeScriptedCheck(script), 10);

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");
    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->serverWarning().isEmpty(), 5000);

    harness.session->stop(true);

    // Let a probe that was already in flight land before taking the baseline, otherwise its
    // increment would be mistaken for the watch continuing to run.
    QTest::qWait(200);
    const int settled = script->calls.load();
    QTest::qWait(400);

    QCOMPARE(script->calls.load(), settled);
    // Stopping clears the warning along with the rest of the server state, so a stopped session
    // never shows a stale "node unavailable".
    QVERIFY(harness.session->serverWarning().isEmpty());
}

QTEST_MAIN(ProxySessionTests)

#include "ProxySessionTests.moc"
