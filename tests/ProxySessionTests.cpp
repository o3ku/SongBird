#include <QtTest>

#include <QEvent>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPointer>
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
#include "appcore/OutboundLivenessProbeService.h"
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
    void startFailsWhenTheOutboundNeverAnswers();
    void skipOutboundCheckAppliesTheProxyWithoutProbing();
    void gateFailureWithNoLocalConnectivityAppliesTheProxyAnyway();
    void theGateResultAndTheActivationIdReachTheLog();
    void anUnresolvableLocationDoesNotFailTheStartup();
    void stopDuringHeldOutboundCheckIgnoresLateResult();
    void stopDuringHeldLocationProbeIgnoresLateLocation();
    void failedLocationProbeDoesNotOverwriteHealthWarning();
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
    void stopCancelsARestartScheduledAfterTheCoreExited();
    void stopDuringTunCleanupCancelsStartupResume();
    void stopClearsAdoptedManagedSystemProxy();
    void stopForCoreUpdateKeepsManagedProxyFlagWhenTheClearFails();
    void abortedStartupUndoClearsAProxyTheSessionOwns();
    void abortedStartupUndoLeavesAProxyItDoesNotOwnAlone();
    void abortedStartupUndoKeepsTheFlagWhenTheClearFails();
    void wiredSnapshotRestoresInsteadOfClearingOnAbortedStartup();
    void stopRestoresThroughTheCapturedSnapshotWhenWired();
    void stopOnAnIdleSessionLeavesAnUnownedProxyAlone();
    void plainStopForgetsOwnershipEvenWhenTheUndoWriteFails();
    void immediateStopStaysSingleLayeredWhenTheRestoreFails();
    void stopEmitsUndoFailedAndDoesNotRetryTheRegistryWrite();
    void stopEmitsUndoFailedWhenTheClearBranchFails();
    void failedStartupForgetsOwnershipEvenWhenTheClearFails();
    void aDeadCoreForgetsOwnershipWithoutClearingTheSnapshot();
    void aDeadCoreForgetsOwnershipEvenWhenTheUndoWriteFails();
    void theDeadCoreForgetNeverWritesToADestroyedSession();

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

    void triggerExited(bool stopRequested = true, QProcess::ExitStatus status = QProcess::NormalExit)
    {
        if (exitedCallback_) {
            exitedCallback_(0, status, stopRequested);
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

// What the fake proxy does with a request it has fully received. The default answers it, which is
// what the startup path needs: the probes hardcode QNetworkProxy::HttpProxy, so plain http://
// requests reach the proxy in absolute form and get a status line back. Dropping the connection
// instead produces an EOF with no status line -- how a middlebox that resets plain HTTP behaves --
// which is how the gate's failing branch is reached without waiting out a probe timeout.
enum class FakeProxyAnswer {
    Payload,
    DropConnection
};

// Stands in for the core's local http inbound. It follows the same layout rules as the fake in
// OutboundLocationProbeServiceTests: the server is declared last and torn down in the destructor
// body, so a socket deleted by ~QTcpServer still finds a live hash when its disconnected handler
// runs. The https:// hosts only need to be refused quickly so they do not hold a probe open.
class FakeHttpProxyServer
{
public:
    explicit FakeHttpProxyServer(
        QByteArray payload,
        FakeProxyAnswer answer = FakeProxyAnswer::Payload)
        : payload_(std::move(payload))
        , answer_(answer)
    {
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() {
            while (QTcpSocket* socket = server_.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    respond(socket);
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, [this, socket]() {
                    requests_.remove(socket);
                    answered_.remove(socket);
                    heldPlainReplies_.remove(socket);
                    socket->deleteLater();
                });
            }
        });
    }

    ~FakeHttpProxyServer()
    {
        // Tear the sockets down here rather than letting ~QTcpServer do it after this destructor
        // body has run: deleting them now happens while the hash and set above are still alive, and
        // unhooking their signals first means none of the handlers above can run at all.
        server_.close();
        const QList<QTcpSocket*> sockets = server_.findChildren<QTcpSocket*>();
        for (QTcpSocket* socket : sockets) {
            socket->disconnect();
            delete socket;
        }
    }

    FakeHttpProxyServer(const FakeHttpProxyServer&) = delete;
    FakeHttpProxyServer& operator=(const FakeHttpProxyServer&) = delete;

    bool listen()
    {
        return server_.listen(QHostAddress::LocalHost, 0);
    }

    int port() const
    {
        return server_.serverPort();
    }

    bool holdPlainReplies = false;

    int heldPlainReplyCount() const
    {
        return heldPlainReplies_.size();
    }

    int heldGeoReplyCount() const
    {
        int count = 0;
        for (QTcpSocket* socket : heldPlainReplies_) {
            const QByteArray request = requests_.value(socket);
            if (request.startsWith("GET http://ip-api.com/")
                || request.startsWith("GET http://ipwho.is/")) {
                ++count;
            }
        }
        return count;
    }

    void releasePlainReplies()
    {
        holdPlainReplies = false;
        QList<QPointer<QTcpSocket>> sockets;
        for (QTcpSocket* socket : heldPlainReplies_) {
            sockets.append(QPointer<QTcpSocket>(socket));
        }
        heldPlainReplies_.clear();
        for (const QPointer<QTcpSocket>& socket : sockets) {
            if (socket && socket->state() == QAbstractSocket::ConnectedState) {
                replyToPlainRequest(socket.data());
            }
        }
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
            // CONNECT 永不 hold，TLS 请求仍快速失败，避免把探测拖到 deadline。
            if (answer_ == FakeProxyAnswer::Payload) {
                socket->write("HTTP/1.1 200 Connection Established\r\n\r\n");
                socket->flush();
            }
            socket->disconnectFromHost();
            return;
        }

        if (holdPlainReplies) {
            heldPlainReplies_.insert(socket);
            return;
        }
        replyToPlainRequest(socket);
    }

    void replyToPlainRequest(QTcpSocket* socket)
    {
        if (answer_ == FakeProxyAnswer::DropConnection) {
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

    // Declared before server_ on purpose: members are destroyed in reverse order, so server_ goes
    // first and the sockets it deletes still find a live hash when their disconnected handler runs.
    // The other way round the handler walks a destroyed QHash -- a crash this pattern has hit before
    // when a test ended while a request was still unanswered.
    QByteArray payload_;
    FakeProxyAnswer answer_ = FakeProxyAnswer::Payload;
    QHash<QTcpSocket*, QByteArray> requests_;
    QSet<QTcpSocket*> answered_;
    QSet<QTcpSocket*> heldPlainReplies_;
    QTcpServer server_;
};

// 取消后的探测回调静默返回；记录投递到会话的 MetaCall，避免把 stop 自身的状态信号
// 或服务端写完响应误当成后台完成。只在请求已 hold 且 stop 返回后安装。
class SessionQueuedCallbackSpy final : public QObject
{
public:
    explicit SessionQueuedCallbackSpy(QObject* session)
    {
        session->installEventFilter(this);
    }

    int count() const
    {
        return count_;
    }

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::MetaCall) {
            ++count_;
        }
        return false;
    }

private:
    int count_ = 0;
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
    explicit ProxySessionHarness(
        QByteArray proxyPayload = locationProbePayload(),
        FakeProxyAnswer proxyAnswer = FakeProxyAnswer::Payload)
        : proxyServer(std::move(proxyPayload), proxyAnswer)
        , configWriter(tempDir.path())
    {
        session = std::make_unique<ProxySession>(ProxySession::Dependencies{
            mainCore,
            auxiliaryCore,
            configWriter,
            locationProbe,
            livenessProbe,
            backgroundTasks,
            *this,
            *this,
            *this
        });
        // The unproxied cross-check the gate consults only after it has failed. Scripted rather than
        // left to the real probe: that one walks the public internet, so an unscripted test would
        // have its outcome decided by whichever network the machine running it happens to have.
        // The default answers "yes, this machine is fine", which is the assumption every case
        // written before the fail-open branch already made -- the node is the one at fault.
        session->setDirectConnectivityCheck([this](int) {
            directConnectivityCheckCount.fetch_add(1);
            return directConnectivityAvailable.load();
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


    // The snapshot trio, defaulted to the "host does not manage the system proxy" shape the
    // interface documents. Wiring them on in a test is the Auto-after-issue-#10 shape.
    SystemProxySnapshot capturedSystemProxySnapshot() const override
    {
        return snapshotsWired ? snapshot : SystemProxySnapshot{};
    }

    SystemProxySnapshot ensureSystemProxySnapshotCaptured() override
    {
        if (snapshotsWired && !snapshot.valid) {
            // Stands in for the registry read: the user's proxy as it looked before this session
            // wrote anything.
            snapshot = SystemProxySnapshot{};
            snapshot.valid = true;
            snapshot.enabled = true;
            snapshot.server = QStringLiteral("user-proxy.example:8080");
        }
        return capturedSystemProxySnapshot();
    }

    bool restoreSystemProxySnapshot(const SystemProxySnapshot& toRestore) override
    {
        if (!snapshotsWired) {
            return true;
        }
        ++restoreCount;
        lastRestoredServer = toRestore.server.value_or(QString());
        if (restoreSucceeds) {
            snapshot = SystemProxySnapshot{};
        }
        return restoreSucceeds;
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
    // can reach, an outbound that answers both the reachability gate and the location
    // probe, and a system-proxy mode that keeps the session active once the core is up.
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
        // The listening probe and both outbound probes must reach the same fake proxy, so the
        // dedicated probe port is pinned to it rather than left to localPort + 103.
        request.config.localPort = proxyServer.port();
        request.config.localLocationProbePort = proxyServer.port();
        request.config.sysProxyType = static_cast<int>(SystemProxyMode::ForcedChange);
        // "Global" carries no geosite/geoip rule, so the generated sing-box config has no
        // remote rule_set and startup does not divert into the rule-set download.
        request.config.collection().routingModeId = QStringLiteral("builtin:global");
        return request;
    }

    QTemporaryDir tempDir;
    FakeHttpProxyServer proxyServer;
    FakeCoreProcessHost mainCore;
    FakeCoreProcessHost auxiliaryCore;
    ClientConfigWriter configWriter;
    OutboundLocationProbeService locationProbe;
    OutboundLivenessProbeService livenessProbe;
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
    // Snapshot-trio state; snapshotsWired stays false unless a test asks for the wired host.
    bool snapshotsWired = false;
    bool restoreSucceeds = true;
    int restoreCount = 0;
    QString lastRestoredServer;
    SystemProxySnapshot snapshot;
    // Read on the worker thread the gate runs on, so both are atomic; the count is also the proof
    // that the cross-check is consulted on the failure path and nowhere else.
    std::atomic_bool directConnectivityAvailable{true};
    std::atomic_int directConnectivityCheckCount{0};
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
    return [script](const Config&) {
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

QStringList logLines(const QSignalSpy& spy)
{
    QStringList lines;
    lines.reserve(spy.count());
    for (const QList<QVariant>& call : spy) {
        lines.append(call.at(0).toString());
    }
    return lines;
}

QString findLogLine(const QStringList& lines, const QString& needle)
{
    for (const QString& line : lines) {
        if (line.contains(needle)) {
            return line;
        }
    }
    return {};
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
    QVERIFY(harness.session->outboundReachable());
    // Proxying no longer waits for the location: the gate is the reachability probe, and the
    // location is asked for afterwards, so it lands after the phase change rather than before it.
    // The unresolvable-location test below pins that ordering deterministically.
    QTRY_VERIFY(harness.session->serverLocation().contains(QStringLiteral("Japan")));
    QCOMPARE(static_cast<int>(harness.lastProxyMode), static_cast<int>(SystemProxyMode::ForcedChange));
    QVERIFY(harness.systemProxyEnabled);

    harness.session->stop(true);
    QCOMPARE(harness.mainCore.stopCount, 1);
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

void ProxySessionTests::startFailsWhenTheOutboundNeverAnswers()
{
    // A proxy that accepts the connection and then hangs up without a status line: the core is
    // listening, so the listening probe passes, but nothing gets through it. This is the one
    // condition the startup is still allowed to fail on.
    ProxySessionHarness harness(QByteArrayLiteral(""), FakeProxyAnswer::DropConnection);
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;
    // A real core exits when it is asked to stop, which is what takes the session from Stopping
    // back to Stopped after the rollback; without this the fake leaves it parked in Stopping.
    harness.mainCore.emitExitOnStop = true;

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));
    QSignalSpy activatedSpy(harness.session.get(), SIGNAL(activated(QString)));

    harness.session->start(harness.launchableRequest());
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    QTRY_COMPARE(failedSpy.count(), 1);

    // Every host has to name itself. With a single shared reason field the last writer wins, and
    // whenever nothing succeeds the last writer is the deadline -- which is how a probe ends up
    // explaining nothing at all.
    const QString reason = failedSpy.takeFirst().at(0).toString();
    QVERIFY2(reason.contains(QStringLiteral("gstatic.com")), qPrintable(reason));
    QVERIFY2(reason.contains(QStringLiteral("cloudflare.com")), qPrintable(reason));

    QCOMPARE(activatedSpy.count(), 0);
    QVERIFY(!harness.session->outboundReachable());
    QVERIFY(!harness.session->isActive());
    QVERIFY(!harness.session->isManagedProxyActive());
    // The system proxy is never applied, and the rollback clears whatever the attempt had set.
    QVERIFY(!harness.systemProxyEnabled);
    // The cross-check ran and came back "this machine is fine", which is what makes the failure the
    // node's. Without it the gate would have no way to tell this case from the one below.
    QCOMPARE(harness.directConnectivityCheckCount.load(), 1);
    QTRY_COMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Stopped));
}

// The fail-open half of the gate. The node answers nothing, but this machine cannot reach the probe
// hosts on its own either -- so the fault is here, not in the node. Failing closed in that
// situation takes the proxy away from a user whose local network is already the problem, so the
// check is waived, the warning says why, and the session comes up anyway.
void ProxySessionTests::gateFailureWithNoLocalConnectivityAppliesTheProxyAnyway()
{
    ProxySessionHarness harness(QByteArrayLiteral(""), FakeProxyAnswer::DropConnection);
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("singbox.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;
    harness.mainCore.emitExitOnStop = true;
    // The unproxied cross-check fails too, which is the evidence that the local path is the broken
    // one. Scripted rather than real: the real check walks the public internet.
    harness.directConnectivityAvailable = false;

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));
    QSignalSpy activatedSpy(harness.session.get(), SIGNAL(activated(QString)));
    QSignalSpy logSpy(harness.session.get(), SIGNAL(logMessage(QString)));

    ProxySession::StartRequest request = harness.launchableRequest();
    // A distinctive value, so the assertion below proves the *effective* resolver is logged rather
    // than whatever the defaults happen to be.
    request.config.dns().proxyServerDns = QStringLiteral("9.9.9.9");
    harness.session->start(request);
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    QTRY_COMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Proxying));

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(activatedSpy.count(), 1);
    QVERIFY(harness.session->outboundReachable());
    QVERIFY(harness.session->isManagedProxyActive());
    QVERIFY(harness.systemProxyEnabled);
    QCOMPARE(harness.directConnectivityCheckCount.load(), 1);

    // The waiver is not silent: the user has to be able to see that the check did not pass, and the
    // warning has to outlive the checklist. The location probe that follows fails too (the same
    // dead proxy), and must not overwrite a stronger warning with its own.
    QVERIFY2(!harness.session->serverWarning().isEmpty(),
        qPrintable(logLines(logSpy).join(QStringLiteral("\n"))));
    QVERIFY2(harness.session->serverWarning().contains(QStringLiteral("could not be completed")),
        qPrintable(harness.session->serverWarning()));

    // And the log says which of the two verdicts it was, so a report does not read as "the gate
    // passed" when it was in fact waived.
    const QStringList lines = logLines(logSpy);
    QVERIFY2(!findLogLine(lines, QStringLiteral("Outbound check failed")).isEmpty(),
        qPrintable(lines.join(QStringLiteral("\n"))));
    QVERIFY2(!findLogLine(lines, QStringLiteral("waived")).isEmpty(),
        qPrintable(lines.join(QStringLiteral("\n"))));

    // The one piece of evidence a "it must be DNS" explanation cannot be argued without. Asserted
    // on content rather than on the keys alone: a line that prints the keys with the wrong resolver
    // in them would answer the report with a confident lie.
    const QString dns = findLogLine(lines, QStringLiteral("DNS in effect"));
    QVERIFY2(!dns.isEmpty(), qPrintable(lines.join(QStringLiteral("\n"))));
    QVERIFY2(dns.contains(QStringLiteral("directDns=") + request.config.dns().directDns),
        qPrintable(dns));
    QVERIFY2(dns.contains(QStringLiteral("remoteDns=") + request.config.dns().remoteDns),
        qPrintable(dns));
    QVERIFY2(dns.contains(QStringLiteral("proxyServerDns=9.9.9.9")), qPrintable(dns));
    QVERIFY2(dns.contains(QStringLiteral("fakeIp=")), qPrintable(dns));

    harness.session->stop(true);
}

// A user who reports "it says the node is unreachable" has to be answerable from the log alone:
// which attempt, which port, how many hosts were tried, how long it took, and which resolvers were
// in effect. Every one of those was missing before, and each is asserted here so it cannot quietly
// go missing again.
void ProxySessionTests::theGateResultAndTheActivationIdReachTheLog()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("singbox.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;

    QSignalSpy logSpy(harness.session.get(), SIGNAL(logMessage(QString)));

    harness.session->start(harness.launchableRequest());
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    QTRY_COMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Proxying));

    const QString activationId = harness.session->activationId();
    QVERIFY2(!activationId.isEmpty(), "every activation must have a handle to correlate its lines");

    const QStringList lines = logLines(logSpy);
    const QString activating = findLogLine(lines, QStringLiteral("Activating"));
    QVERIFY2(!activating.isEmpty(), qPrintable(lines.join(QStringLiteral("\n"))));
    QVERIFY2(activating.contains(QStringLiteral("id=") + activationId), qPrintable(activating));

    const QString gate = findLogLine(lines, QStringLiteral("Outbound check passed"));
    QVERIFY2(!gate.isEmpty(), qPrintable(lines.join(QStringLiteral("\n"))));
    // The same handle as the Activating line: that is the whole point of it.
    QVERIFY2(gate.contains(QStringLiteral("id=") + activationId), qPrintable(gate));
    QVERIFY2(gate.contains(QStringLiteral("port=")), qPrintable(gate));
    QVERIFY2(gate.contains(QStringLiteral("hosts=")), qPrintable(gate));
    QVERIFY2(gate.contains(QStringLiteral("elapsed=")), qPrintable(gate));

    // The location line is emitted after the session is already up, so it is waited for rather than
    // read straight away.
    QTRY_VERIFY_WITH_TIMEOUT(
        findLogLine(logLines(logSpy), QStringLiteral("Detecting outbound IP location")).contains(
            QStringLiteral("id=") + activationId),
        5000);

    harness.session->stop(true);
}

// The advanced escape hatch. The same proxy that fails the gate above must not be consulted at all
// once the user has turned the check off: the system proxy is applied and the session comes up.
void ProxySessionTests::skipOutboundCheckAppliesTheProxyWithoutProbing()
{
    ProxySessionHarness harness(QByteArrayLiteral(""), FakeProxyAnswer::DropConnection);
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));
    QSignalSpy activatedSpy(harness.session.get(), SIGNAL(activated(QString)));

    ProxySession::StartRequest request = harness.launchableRequest();
    request.config.skipOutboundCheck = true;
    harness.session->start(request);
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    QTRY_COMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Proxying));
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(activatedSpy.count(), 1);
    // Waived, not confirmed -- but the session must still be reportable as active, otherwise the
    // option would leave the UI stuck on "inconsistent".
    QVERIFY(harness.session->outboundReachable());
    QVERIFY(harness.session->isActive());
    QVERIFY(harness.session->isManagedProxyActive());
    QVERIFY(harness.systemProxyEnabled);

    harness.session->stop(true);
}

void ProxySessionTests::anUnresolvableLocationDoesNotFailTheStartup()
{
    // The proxy answers everything with 200 and a body the location parser cannot read. The gate
    // only looks at the status code, so the session must come up: the location decorates the status
    // bar and decides nothing. This is the case that used to take the whole proxy down whenever one
    // of the third-party geo services was rate limiting.
    ProxySessionHarness harness(QByteArrayLiteral("<html>not a location</html>"));
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;

    QSignalSpy failedSpy(harness.session.get(), SIGNAL(failed(QString)));
    QSignalSpy activatedSpy(harness.session.get(), SIGNAL(activated(QString)));

    harness.session->start(harness.launchableRequest());
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    // Reaching Proxying at all is the deterministic proof that the startup did not wait for the
    // location: the location probe in this harness can never produce one.
    QTRY_COMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Proxying));
    QCOMPARE(activatedSpy.count(), 1);
    QVERIFY(harness.session->outboundReachable());
    QVERIFY(harness.session->isActive());
    QVERIFY(harness.session->isManagedProxyActive());

    // The location probe still reports, so the user and the log can see that the exit is unknown.
    QTRY_VERIFY_WITH_TIMEOUT(!harness.session->serverWarning().isEmpty(), 10000);

    QVERIFY(harness.session->serverLocation().isEmpty());
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(static_cast<int>(harness.session->phase()), static_cast<int>(ProxySession::Phase::Proxying));
    QVERIFY(harness.session->isActive());
    QVERIFY(harness.systemProxyEnabled);
}

void ProxySessionTests::stopDuringHeldOutboundCheckIgnoresLateResult()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());

    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;
    harness.proxyServer.holdPlainReplies = true;

    QSignalSpy failedSpy(harness.session.get(), &ProxySession::failed);
    QSignalSpy activatedSpy(harness.session.get(), &ProxySession::activated);
    QSignalSpy statusSpy(harness.session.get(), &ProxySession::statusSyncRequested);

    harness.session->start(harness.launchableRequest());
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));

    QTRY_VERIFY_WITH_TIMEOUT(harness.proxyServer.heldPlainReplyCount() > 0, 2000);
    QCOMPARE(static_cast<int>(harness.session->phase()),
        static_cast<int>(ProxySession::Phase::CheckOutboundConnection));
    QCOMPARE(activatedSpy.count(), 0);
    QVERIFY(!harness.session->outboundReachable());
    QVERIFY(!harness.systemProxyEnabled);

    harness.session->stop(true);
    const int stoppedStatusCount = statusSpy.count();
    SessionQueuedCallbackSpy completionSpy(harness.session.get());
    harness.proxyServer.releasePlainReplies();

    // qWaitFor 在事件投递结束后检查条件，此时过期结果的回调也已执行完。
    QVERIFY(QTest::qWaitFor([&completionSpy]() { return completionSpy.count() > 0; }, 5000));

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(activatedSpy.count(), 0);
    QCOMPARE(statusSpy.count(), stoppedStatusCount);
    QVERIFY(!harness.session->outboundReachable());
    QVERIFY(!harness.session->isActive());
    QVERIFY(!harness.session->isActivationInProgress());
    QVERIFY(!harness.session->isManagedProxyActive());
    QCOMPARE(static_cast<int>(harness.session->phase()),
        static_cast<int>(ProxySession::Phase::Stopped));
    QVERIFY(!harness.systemProxyEnabled);
}

void ProxySessionTests::stopDuringHeldLocationProbeIgnoresLateLocation()
{
    ProxySessionHarness harness;
    QSignalSpy activatedSpy(harness.session.get(), &ProxySession::activated);
    QSignalSpy failedSpy(harness.session.get(), &ProxySession::failed);
    QSignalSpy locationSpy(harness.session.get(), &ProxySession::serverLocationChanged);
    QSignalSpy statusSpy(harness.session.get(), &ProxySession::statusSyncRequested);

    // activated 同线程同步发出，Tier1 已通过，随后启动的 geo 请求才被 hold。
    QObject::connect(harness.session.get(), &ProxySession::activated, harness.session.get(),
        [&harness]() { harness.proxyServer.holdPlainReplies = true; }, Qt::DirectConnection);
    QVERIFY2(driveToProxying(harness), "会话未进入 Proxying");
    QTRY_VERIFY_WITH_TIMEOUT(harness.proxyServer.heldGeoReplyCount() > 0, 2000);
    QCOMPARE(activatedSpy.count(), 1);
    QVERIFY(harness.session->serverLocation().isEmpty());

    harness.session->stop(true);
    const int stoppedLocationCount = locationSpy.count();
    const int stoppedStatusCount = statusSpy.count();
    SessionQueuedCallbackSpy completionSpy(harness.session.get());
    harness.proxyServer.releasePlainReplies();

    QVERIFY(QTest::qWaitFor([&completionSpy]() { return completionSpy.count() > 0; }, 5000));

    QCOMPARE(locationSpy.count(), stoppedLocationCount);
    QCOMPARE(statusSpy.count(), stoppedStatusCount);
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(failedSpy.count(), 0);
    QVERIFY(harness.session->serverLocation().isEmpty());
    QVERIFY(!harness.session->outboundReachable());
    QVERIFY(!harness.session->isActive());
    QVERIFY(!harness.session->isManagedProxyActive());
    QCOMPARE(static_cast<int>(harness.session->phase()),
        static_cast<int>(ProxySession::Phase::Stopped));
    QVERIFY(!harness.systemProxyEnabled);
}

void ProxySessionTests::failedLocationProbeDoesNotOverwriteHealthWarning()
{
    ProxySessionHarness harness(QByteArrayLiteral("<html>not a location</html>"));
    QSignalSpy activatedSpy(harness.session.get(), &ProxySession::activated);
    QSignalSpy failedSpy(harness.session.get(), &ProxySession::failed);
    QSignalSpy logSpy(harness.session.get(), &ProxySession::logMessage);

    QObject::connect(harness.session.get(), &ProxySession::activated, harness.session.get(),
        [&harness]() { harness.proxyServer.holdPlainReplies = true; }, Qt::DirectConnection);
    QVERIFY2(driveToProxying(harness), "会话未进入 Proxying");
    QTRY_VERIFY_WITH_TIMEOUT(harness.proxyServer.heldGeoReplyCount() > 0, 2000);

    const QString healthWarning = QStringLiteral("health outage");
    harness.session->setServerWarning(healthWarning);
    logSpy.clear();
    harness.proxyServer.releasePlainReplies();

    const auto locationProbeFinished = [&logSpy]() {
        for (const QList<QVariant>& arguments : logSpy) {
            const QString message = arguments.at(0).toString();
            if (message.contains(QStringLiteral("ip-api.com"))
                && message.contains(QStringLiteral("Outbound location response was empty."))) {
                return true;
            }
        }
        return false;
    };
    // 只有各轮 geo 探测结束后才会输出这个 payload 解析失败的日志，不能只等 warning 非空。
    QTRY_VERIFY_WITH_TIMEOUT(locationProbeFinished(),
        OutboundLocationProbeService::LocationProbeTotalTimeoutMs + 1000);

    QCOMPARE(harness.session->serverWarning(), healthWarning);
    QVERIFY(harness.session->serverLocation().isEmpty());
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(static_cast<int>(harness.session->phase()),
        static_cast<int>(ProxySession::Phase::Proxying));
    QVERIFY(harness.session->isActive());
    QVERIFY(harness.session->outboundReachable());
    QVERIFY(harness.session->isManagedProxyActive());
    QVERIFY(harness.systemProxyEnabled);

    harness.session->stop(true);
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

// Issue #14, item 2. handleCoreExited() reacts to a crash by scheduling a start 50 ms out, and
// that decision is made before the timer runs -- nothing recorded a stop arriving in between. The
// start it re-enters is guarded by rejectOverlappingStart(), which refuses only a live core or the
// Stopping phase, and the stop has already taken both away. So the session resumed itself after
// the user asked it to stop: the connection they gave up came back on its own.
//
// The window is what made this invisible by hand -- the crash and the click are two events from
// two origins, and the test drives exactly that order.
void ProxySessionTests::stopCancelsARestartScheduledAfterTheCoreExited()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());

    // No exit callback here: the stop below runs before the host gets to report the exit, which
    // is the scheduling decision on its own.
    harness.mainCore.running = true;
    harness.session->handleCoreExitedForTest(1, QProcess::CrashExit, false, false);
    // The user stops inside the gap. This is the last word, and it must be.
    harness.session->stop(true);

    // The armed start's only visible effect is beginActivation() asking the activation
    // coordinator to cancel the startup background work before it does anything else, so that
    // counter proves whether it ran at all -- and a resumed start would also start a core.
    const int cancelsAfterStop = harness.cancelBackgroundTaskCount;
    const int startsAfterStop = harness.mainCore.startCount;
    QTest::qWait(200);

    QCOMPARE(harness.cancelBackgroundTaskCount, cancelsAfterStop);
    QCOMPARE(harness.mainCore.startCount, startsAfterStop);
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

// undoManagedSystemProxyAfterAbortedStartup() is what SongBirdAuto's TUN-failure branch calls
// (issue #8): the start is aborted before ProxySession::start() runs, so this -- not a failed
// startup -- is the only thing that can undo a proxy an earlier round of this session applied.
// The harness wires no snapshot capture, exactly like Auto, so the ownership decision can only
// land on Clear; what these cases pin is that the decision is made at all, and by ownership.
void ProxySessionTests::abortedStartupUndoClearsAProxyTheSessionOwns()
{
    ProxySessionHarness harness;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);

    harness.session->undoManagedSystemProxyAfterAbortedStartup();

    QCOMPARE(harness.proxyUpdateCount, 1);
    QCOMPARE(static_cast<int>(harness.lastProxyMode), static_cast<int>(SystemProxyMode::ForcedClear));
    QVERIFY(!harness.session->isManagedProxyActive());
    QVERIFY(!harness.systemProxyEnabled);
}

void ProxySessionTests::abortedStartupUndoLeavesAProxyItDoesNotOwnAlone()
{
    ProxySessionHarness harness;
    // The registry is on, but this session never wrote it: the user's own proxy. The ownership
    // rule says LeaveAlone -- "no live port" is not evidence of "we wrote this".
    harness.systemProxyEnabled = true;

    harness.session->undoManagedSystemProxyAfterAbortedStartup();

    QCOMPARE(harness.proxyUpdateCount, 0);
    QVERIFY(harness.systemProxyEnabled);
}

void ProxySessionTests::abortedStartupUndoKeepsTheFlagWhenTheClearFails()
{
    ProxySessionHarness harness;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.proxyUpdateSucceeds = false;

    harness.session->undoManagedSystemProxyAfterAbortedStartup();

    // Same rule as stopForCoreUpdate: a failed write must not make the session forget it owns
    // the registry -- a later stop would then leave the dead values in place for good.
    QCOMPARE(harness.proxyUpdateCount, 1);
    QVERIFY(harness.session->isManagedProxyActive());
}

// The two issue-#10 pins: a host that wires the snapshot trio gets the user's own proxy put
// back on every undo, including the aborted-startup entry and a plain stop, instead of the
// ForcedClear a snapshotless host (Auto before the wiring, and every other test here) pays.
void ProxySessionTests::wiredSnapshotRestoresInsteadOfClearingOnAbortedStartup()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.snapshot.valid = true;
    harness.snapshot.enabled = true;
    harness.snapshot.server = QStringLiteral("user-proxy.example:8080");

    harness.session->undoManagedSystemProxyAfterAbortedStartup();

    // Restore, not clear: the registry write goes through restoreSystemProxySnapshot, and no
    // ForcedClear may run for a host that can put the user's values back.
    QCOMPARE(harness.restoreCount, 1);
    QCOMPARE(harness.lastRestoredServer, QStringLiteral("user-proxy.example:8080"));
    QCOMPARE(harness.proxyUpdateCount, 0);
    QVERIFY(!harness.session->isManagedProxyActive());
    QVERIFY(!harness.snapshot.valid);
}

void ProxySessionTests::stopRestoresThroughTheCapturedSnapshotWhenWired()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");

    // applySystemProxyAfterOutboundCheck captured the snapshot before its write, so the undo a
    // plain stop runs must put those values back rather than clearing.
    QVERIFY(harness.snapshot.valid);
    QCOMPARE(harness.snapshot.server.value_or(QString()), QStringLiteral("user-proxy.example:8080"));

    harness.session->stop(false);

    QCOMPARE(harness.restoreCount, 1);
    // The only registry write remains the startup's own ForcedChange; the stop restored.
    QCOMPARE(harness.proxyUpdateCount, 1);
    QVERIFY(!harness.session->isManagedProxyActive());
    // The spent evidence is behavioural, not the double's own bookkeeping: ownership is gone,
    // so a second stop must be a no-op -- the restore must not fire twice.
    harness.session->stop(false);
    QCOMPARE(harness.restoreCount, 1);
}

// The idle-session shapes SongBirdAutoCoordinator::stop() relies on since the unconditional
// ForcedClear was removed (issue #12): a session that never wrote the registry must leave a
// proxy it does not own alone even when the registry is on, and a plain stop drops the
// ownership flag on a failed undo write while leaving the snapshot valid -- spent only by a
// successful restore, whatever retries on it later.
void ProxySessionTests::stopOnAnIdleSessionLeavesAnUnownedProxyAlone()
{
    ProxySessionHarness harness;
    // The registry is on, but with a user's own proxy: this session never applied one, so the
    // ownership decision must be LeaveAlone even though the user pressed stop.
    harness.systemProxyEnabled = true;

    harness.session->stop(false);

    QCOMPARE(harness.proxyUpdateCount, 0);
    QCOMPARE(harness.restoreCount, 0);
    QVERIFY(harness.systemProxyEnabled);
    QVERIFY(!harness.session->isManagedProxyActive());
}

void ProxySessionTests::plainStopForgetsOwnershipEvenWhenTheUndoWriteFails()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;
    harness.restoreSucceeds = false;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.snapshot.valid = true;
    harness.snapshot.enabled = true;
    harness.snapshot.server = QStringLiteral("user-proxy.example:8080");

    const SystemProxySnapshot before = harness.snapshot;

    harness.session->stop(false);

    // The restore was attempted and failed, and a plain stop still drops the flag: only
    // stopForCoreUpdate keeps it on a failed write, because a core restart is about to retry.
    QCOMPARE(harness.restoreCount, 1);
    // No ForcedClear may follow the failed restore. "The undo did not succeed" is not a reason
    // to clear, and a clear could not see the snapshot anyway -- the deleted coordinator line
    // used to be exactly this second write.
    QCOMPARE(harness.proxyUpdateCount, 0);
    // The snapshot survives a failed restore verbatim, spent only by a successful one, so
    // whatever retries later (as wired today, the next launch's recovery) sees exactly what to
    // put back. Compared field by field on purpose: "still valid" alone is vacuous in a harness
    // whose restore only spends on success.
    QVERIFY(harness.snapshot == before);
    QVERIFY(!harness.session->isManagedProxyActive());
}

// The session-level half of the issue-#12 pin: the coordinator's immediate path (the
// destructor's exit) used to fire an unconditional ForcedClear after stop(true) returned, and
// what this case covers is that nothing *inside* the session does the same -- an immediate stop
// performs exactly one registry operation, and a failed restore is not retried with a Clear.
// The coordinator half is pinned directly in SongBirdAutoCoordinatorStopTests, now that the
// coordinator takes its ISystemProxyService by constructor and createProxySession() can be built
// without initialize(). Neither case alone would have caught the deletion: this one cannot see
// the coordinator, and that one does not reach a failed restore.
void ProxySessionTests::immediateStopStaysSingleLayeredWhenTheRestoreFails()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;
    harness.restoreSucceeds = false;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.snapshot.valid = true;
    harness.snapshot.enabled = true;
    harness.snapshot.server = QStringLiteral("user-proxy.example:8080");

    harness.session->stop(true);

    QCOMPARE(harness.restoreCount, 1);
    QCOMPARE(harness.proxyUpdateCount, 0);
    QVERIFY(!harness.session->isManagedProxyActive());
}


// Issue #14 item A: the undo write failing is invisible in the log alone -- a user offline
// behind a dead proxy cannot read the log. The failure must be announced, once, and the
// announced shape is display-only: a second stop must not write the registry again (the
// plain-stop contract), the retry belongs to the next launch's recovery.
void ProxySessionTests::stopEmitsUndoFailedAndDoesNotRetryTheRegistryWrite()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;
    harness.restoreSucceeds = false;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.snapshot.valid = true;
    harness.snapshot.enabled = true;
    harness.snapshot.server = QStringLiteral("user-proxy.example:8080");

    QSignalSpy spy(harness.session.get(), &ProxySession::systemProxyUndoFailed);
    harness.session->stop(false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(harness.restoreCount, 1);

    harness.session->stop(false);

    // No re-announcement and no second write: the flag is gone after the first plain stop.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(harness.restoreCount, 1);
    // And the snapshot is still there, untouched: it is spent only by a successful restore, so
    // the next launch's recovery is the retry the announcement promised. (This line also pins
    // the contract a withdrawn review patch wanted to flip -- spend-on-attempt would fail it.)
    QVERIFY(harness.snapshot.valid);
    QVERIFY(!harness.session->isManagedProxyActive());
}

// The other failure branch: no snapshot survived, so the undo was a Clear and it failed.
// The message here cannot promise a retry -- nothing retries -- but the announcement itself
// is the deliverable; the wording lives in the tr() strings and is not pinned.
void ProxySessionTests::stopEmitsUndoFailedWhenTheClearBranchFails()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;
    harness.proxyUpdateSucceeds = false;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    // No valid snapshot: the undo decision falls to Clear.

    QSignalSpy spy(harness.session.get(), &ProxySession::systemProxyUndoFailed);
    harness.session->stop(false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(harness.proxyUpdateCount, 1);
}

// The failed-startup intent, pinned where the ledger asked for it: a dead session forgets the
// claim even when the undo write failed -- unlike a plain stop, which also drops it, and unlike
// undoManagedSystemProxyAfterAbortedStartup(), which keeps it -- and the failure is announced
// through the item-A channel while it happens. Driven through the real failStartup() entry:
// no active server fails profile resolution on the synchronous half of start().
void ProxySessionTests::failedStartupForgetsOwnershipEvenWhenTheClearFails()
{
    ProxySessionHarness harness;
    harness.proxyUpdateSucceeds = false;
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);

    QSignalSpy undoFailed(harness.session.get(), &ProxySession::systemProxyUndoFailed);
    QSignalSpy failedSpy(harness.session.get(), &ProxySession::failed);

    harness.session->start(ProxySession::StartRequest{});

    QCOMPARE(harness.proxyUpdateCount, 1);
    QCOMPARE(undoFailed.count(), 1);
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(!harness.session->isManagedProxyActive());
}
// --- Node health watch ---
//
// These cover the decision the watch makes, not the probe itself: the probe is injected, so what
// is under test here is when ProxySession starts and stops watching and what it does with each
// verdict. The policy's own arithmetic is pinned separately by proxy-health-watch-policy.

// The DisableRestart branch of handleCoreExited, reached the only way it is reachable: through the
// real crash-restart policy rather than by naming the branch. Five crashes are restarted, the sixth
// exhausts the budget and the policy answers DisableRestart -- #14 item B's second hiding place.
//
// Four conditions make the branch's forget the thing under test instead of an artefact:
//
// - The budget has to accumulate across the restarts, so the core has to be back up before the next
//   crash. A round whose start does not reach Proxying resets the count and every round is then
//   "crash 1"; the loop replays what the restart does (start, answer, crash unprompted), which is
//   the route the policy sees in production.
// - The rounds after the first do not go through start(). The previous crash left a restart timer
//   armed, and start() answers a request that arrives while a slot is still scheduled by opening
//   the checklist and returning -- the crash budget never advances. What the timer itself calls at
//   the end of its delay is startInternal(false, false, ...), which is the entry those rounds use.
// - The exit is reported with stopRequested false, like an unprompted crash. The handler ends at
//   `if (stopRequested) emit stopped()`, and the window that connects `stopped` calls stop(), so a
//   stopRequested exit would hand the forgetting to the plain-stop branch.
// - Ownership is read after the crash settles and before the next start. Arming the claim is the
//   startup's job (applySystemProxyAfterOutboundCheck sets it after a successful write), so a read
//   taken after the restart pins startInternal instead of this branch.
//
// What the pin is worth, stated plainly: it covers the handler's own ownership decision. It does
// not claim a lost claim is user-visible -- an orphaned flag is an internal invariant, and the
// user-facing failure it would cause (the next launch clearing a proxy it no longer owns) is
// closed by the retired-PID guard in the recovery test, not by this flag.
//
// Out of reach from here: the checklist-visible route, the only one that leaves keepOnNextStop_
// armed. Every crash leaves Proxying, and leaving Proxying clears the checklist; a start issued
// while the core still runs reaches keepForUserDismissal, but the exit that follows carries
// `stopRequested = true` and the handler's own stop branch then stops the session first.
ProxySession::StartRequest crashLoopRequest(const ProxySessionHarness& harness)
{
    ProxySession::StartRequest request = harness.launchableRequest();
    // The TUN-conflict retry sits in front of the crash budget and would send the same exit down a
    // different branch, so its precondition is off: no conflict was detected at this exit.
    request.config.tun().tunModeItem.enableTun = false;
    return request;
}

// One round: the core starts, answers, and exits unprompted.
void crashOnce(ProxySessionHarness& harness, bool firstRound)
{
    if (firstRound) {
        harness.session->start(crashLoopRequest(harness));
    } else {
        harness.session->startInternalForTest(false, false, false);
    }
    harness.mainCore.triggerStarted(QStringLiteral("core is up"));
    QTest::qWait(20);
    harness.mainCore.triggerExited(false);
}

void ProxySessionTests::aDeadCoreForgetsOwnershipWithoutClearingTheSnapshot()
{
    ProxySessionHarness harness;
    harness.snapshotsWired = true;

    QVERIFY2(driveToProxying(harness), "session did not reach Proxying");
    QVERIFY(harness.session->isManagedProxyActive());
    QVERIFY(harness.snapshot.valid);
    const int writesAfterStartup = harness.proxyUpdateCount;

    // Nothing re-arms the health watch after a crash, but the watch would probe the real node if it
    // did, so the injected check stays empty on purpose: the crash loop is the subject.
    QSignalSpy logSpy(harness.session.get(), SIGNAL(logMessage(QString)));
    QSignalSpy stoppedSpy(harness.session.get(), SIGNAL(stopped()));

    for (int round = 1; round <= 5; ++round) {
        harness.mainCore.running = false;
        crashOnce(harness, round == 1);
        // Budget still in hand, so the branch under test has not run. These say "nothing else
        // moved" rather than "ownership was still held": ProxySession's own crash-cooldown stops a
        // restarted core within three seconds, and the undo that follows an owned session with a
        // spent capture is a Clear -- so ownership does not survive a restart by construction. The
        // assert below pins the restart count, the write count pins the registry, and the stop
        // count pins that no exit ran the plain-stop branch. Ownership itself is asserted after
        // the sixth exit, where the flag is the branch's decision and no restart follows it.
        QVERIFY2(harness.mainCore.startCount == round + 1, "a crash round did not restart the core");
        QCOMPARE(harness.proxyUpdateCount, writesAfterStartup);
        QVERIFY2(stoppedSpy.count() == 0, "a crash round completed a stop");
    }

    harness.mainCore.running = false;
    crashOnce(harness, false);

    // The sixth exceeds kMaxCrashRestarts, so this exit took the DisableRestart branch and nothing
    // else -- the policy's own message is the evidence, not a count assumed here.
    const QStringList lines = logLines(logSpy);
    QVERIFY(findLogLine(lines, QStringLiteral("Auto-restart disabled after 6 consecutive failures."))
                .isEmpty() == false);
    QVERIFY(findLogLine(lines, QStringLiteral("System proxy restored because the core stopped."))
                .isEmpty() == false);
    QCOMPARE(stoppedSpy.count(), 1);
    // The undo at the top of the handler restored through the capture the startup took; the branch
    // then forgot. One write, one restore, so no earlier round undid anything.
    QCOMPARE(harness.proxyUpdateCount, writesAfterStartup);
    QCOMPARE(harness.restoreCount, 1);
    QVERIFY(!harness.session->isManagedProxyActive());
    // The forget dropped the claim and nothing else. The capture is spent here, but a successful
    // restore is what spends it -- a different decision, taken by the undo. A version that cleared
    // the claim from inside clearProxyStateAfterStopped() would look identical on the flag while
    // the rest of that helper's state went with it, which is the reason the clear lives at the
    // call site.
    QVERIFY(!harness.snapshot.valid);
}

// The same branch with the undo write failing: the forget is the caller's intent, not a consequence
// of the write succeeding. That contrast is the whole difference between this branch and
// undoManagedSystemProxyAfterAbortedStartup, whose session stays alive and therefore keeps the
// claim when the write fails.
void ProxySessionTests::aDeadCoreForgetsOwnershipEvenWhenTheUndoWriteFails()
{
    ProxySessionHarness harness;
    QVERIFY(harness.tempDir.isValid());
    QVERIFY(harness.proxyServer.listen());
    harness.coreInfo.type = CoreType::SingBox;
    harness.coreInfo.program = harness.tempDir.filePath(QStringLiteral("sing-box.exe"));
    harness.activeServer = harness.launchableServer();
    harness.currentServerIndexId = harness.activeServer->indexId;
    // Adopted the way the aborted-startup cases adopt it, and for the reason the harness comment
    // gives: with no capture wired the undo decision lands on Clear, which is the branch whose
    // failure is observable. Applying the proxy through the startup path instead would make the
    // startup's own write the failing one, and a failed startup forgets too -- this case then could
    // not say which forget it saw.
    harness.systemProxyEnabled = true;
    harness.session->adoptManagedSystemProxy(true);
    harness.proxyUpdateSucceeds = false;

    QSignalSpy logSpy(harness.session.get(), SIGNAL(logMessage(QString)));
    QSignalSpy undoFailedSpy(harness.session.get(), &ProxySession::systemProxyUndoFailed);

    for (int round = 1; round <= 5; ++round) {
        harness.mainCore.running = false;
        crashOnce(harness, round == 1);
        // One failed undo and one announcement per restart decision, and the claim survives each:
        // that is the shape the branch below has to break.
        QCOMPARE(undoFailedSpy.count(), round);
    }

    harness.mainCore.running = false;
    crashOnce(harness, false);

    const QStringList lines = logLines(logSpy);
    QVERIFY(findLogLine(lines, QStringLiteral("Auto-restart disabled after 6 consecutive failures."))
                .isEmpty() == false);
    QVERIFY(findLogLine(lines, QStringLiteral("Failed to disable system proxy after the core stopped."))
                .isEmpty() == false);
    QCOMPARE(undoFailedSpy.count(), 6);
    QVERIFY(!harness.session->isManagedProxyActive());
}

// The half of the two cases above that is about a write arriving after the session is gone.
//
// The forget the cases above pin is a *write* to managedSystemProxyActive_, so the question this
// case has to answer is whether such a write can land on a destroyed session. It cannot, and the
// contract has two halves. First, every result handler the session posts (activationFinished,
// runOutboundCheck, the location probe) is a QMetaObject::invokeMethod(this, ..., QueuedConnection)
// -- queued to `this`, so it runs on the GUI thread, the *receiver's* thread, not the posting
// worker's; ~QObject then drops whatever is still queued to it. Second, the handlers that matter
// open with the session's lifetime gate: `if (guard.expired() || shuttingDown_.load()) return;`
// (`ProxySession.cpp:962`, `:1013`, `:1345`, `:1459`), where `guard` is the weak view of
// `lifetimeGuard_` captured by each posted task. Both gates are set by the destructor and only
// there: `~ProxySession()` writes `shuttingDown_ = true` and then `lifetimeGuard_.reset()`
// (`:198-199`), which is what expires every outstanding `guard`. stopInternal does not set either
// one; it requests cancellation, and the destructor is what closes the gate.
//
// An earlier version of this comment claimed those handlers land on the worker's thread and that
// ThreadSanitizer reports managedSystemProxyActive_ being raced from two threads. Running the
// suite under -fsanitize=thread does not produce that report: with the binary built against
// Debian's ship-without-TSAN Qt5, every reported race is inside libQt5Core's internals (see
// `tests/README-tsan.md`). The thread claim was wrong, so it is not written down here as fact.
// The mechanism written in its place was wrong too -- it named a `stateMutex_` and a `stopping`
// flag that do not exist in ProxySession, and credited the set to stopInternal -- so it is spelled
// out above against the real identifiers rather than left as a plausible-sounding story.
//
// The two cases above steer around the race by asserting ownership after the sixth crash, when the
// crash cooldown has already stopped a restarted core and dropped the claim. This one keeps the
// crash loop producing orphaned writes on purpose -- a capturing connection to an atomic worker
// count, no ownership read until the end -- and insists nothing lands after the session is gone.
// `tests/README-tsan.md` has the recipe for running the suite under -fsanitize=thread.
void ProxySessionTests::theDeadCoreForgetNeverWritesToADestroyedSession()
{
    auto harness = std::make_unique<ProxySessionHarness>();
    harness->snapshotsWired = true;
    QVERIFY(harness->tempDir.isValid());
    // A capturing connection, not one to the harness: the trigger side is reached from the crash
    // handler on the GUI thread, and a connection to a member that outlives the session is exactly
    // the thing whose absence is being asserted.
    auto rounds = std::make_shared<std::atomic_int>(0);
    QObject::connect(harness->session.get(), &ProxySession::stopped, harness->session.get(),
                     [rounds]() { rounds->fetch_add(1); });

    QVERIFY2(driveToProxying(*harness), "session did not reach Proxying");
    QVERIFY(harness->session->isManagedProxyActive());
    QVERIFY(harness->snapshot.valid);

    for (int round = 1; round <= 5; ++round) {
        harness->mainCore.running = false;
        crashOnce(*harness, round == 1);
        QCOMPARE(harness->mainCore.startCount, round + 1);
    }
    harness->mainCore.running = false;
    crashOnce(*harness, false);
    QCOMPARE(rounds->load(), 1);

    // Ownership is read once, here. Every write before it -- the undo's, the branch's, and the
    // orphans the crash cooldown produced -- is what the sanitizer is being asked about.
    QVERIFY(!harness->session->isManagedProxyActive());

    // Destroying the session while a queued activation result may still be in flight is the case
    // the handler prologue has to survive; the processing below is what would expose it.
    harness->session.reset();
    QCoreApplication::processEvents();
    QTest::qWait(50);

    // And the connection above did not resurrect anything: the round count is the handler's, not a
    // count of calls into a dead object.
    QCOMPARE(rounds->load(), 1);
}

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
