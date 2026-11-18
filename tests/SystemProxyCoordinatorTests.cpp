#include <QtTest>

#include <memory>
#include <QStringList>

#include <QHostAddress>
#include <QTcpServer>

#include "app/SystemProxyCoordinator.h"
#include "domain/models/RoutingRule.h"
#include "persistence/IConfigRepository.h"
#include "platform/ISystemProxyService.h"
#include "services/ServerService.h"

namespace {

class MockConfigRepository : public IConfigRepository {
public:
    Config load() override { return config_; }
    bool save(const Config& config) override { config_ = config; return true; }
    QString lastLoadError() const override { return {}; }
    QString lastSaveError() const override { return {}; }

    Config config_;
};

// Stands in for the registry-backed implementation. The real service writes
// HKCU\...\Internet Settings, and the coordinator is driven through this stand-in so its failure
// branch stays independent of that class: `accepts_` is the refusal. The concrete service's own
// refusal path is covered by the system-proxy-service suite, which points it at a scratch key.
class FakeSystemProxyService : public ISystemProxyService {
public:
    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override
    {
        ++updateCalls_;
        lastMode_ = mode;
        lastHttpPort_ = httpPort;
        lastSocksPort_ = socksPort;
        lastProxyExceptions_ = proxyExceptions;
        lastAdvancedProtocol_ = advancedProtocol;
        return accepts_;
    }

    bool isEnabled() const override { return enabled_; }
    void resetOnShutdown() const override { ++resetCalls_; }

    SystemProxySnapshot capture() const override
    {
        ++captureCalls_;
        return captured_; // invalid by default, which is what most cases want
    }

    bool restore(const SystemProxySnapshot& snapshot) const override
    {
        ++restoreCalls_;
        lastRestored_ = snapshot;
        return restoreAccepts_;
    }

    bool accepts_ = true;
    bool enabled_ = false;
    SystemProxySnapshot captured_;
    bool restoreAccepts_ = true;
    mutable int updateCalls_ = 0;
    mutable int resetCalls_ = 0;
    mutable int captureCalls_ = 0;
    mutable int restoreCalls_ = 0;
    mutable SystemProxySnapshot lastRestored_;
    mutable SystemProxyMode lastMode_ = SystemProxyMode::ForcedClear;
    mutable int lastHttpPort_ = 0;
    mutable int lastSocksPort_ = 0;
    mutable QString lastProxyExceptions_;
    mutable QString lastAdvancedProtocol_;
};

} // namespace

// A port nothing is listening on: bind one, take its number, close it. The probe in the
// recovery path connects, so a number that was merely never bound would do, but a number
// the OS just handed out is a port this machine is known to accept.
int freeTcpPort()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const int port = server.serverPort();
    server.close();
    return port;
}

// Everything recoverOwnedSystemProxyOnStartup() needs, with the fake service and the
// messages it reports. The coordinator is built on demand so each test can tune the
// config and the captured registry state first; it is held by pointer because the
// coordinator stores references and is not copyable.
struct RecoveryFixture {
    Config config;
    MockConfigRepository repository;
    ServerService serverService{repository};
    FakeSystemProxyService service;
    QStringList messages;
    QStringList failures;
    std::unique_ptr<SystemProxyCoordinator> coordinator_;

    void build()
    {
        SystemProxyCoordinator::Callbacks callbacks;
        callbacks.appendResult = [&](const OperationResult& result) {
            (result.success ? messages : failures).append(result.message);
        };
        coordinator_ = std::make_unique<SystemProxyCoordinator>(
            SystemProxyCoordinator::Dependencies{config, serverService, &service, nullptr},
            callbacks);
    }

    void recover()
    {
        coordinator_->recoverOwnedSystemProxyOnStartup();
    }
};

class SystemProxyCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void updateModeReportsFailureWhenTheServiceRefusesTheWrite();
    void updateModeReportsSuccessWhenTheServiceAcceptsTheWrite();
    void updateModeTreatsAMissingServiceAsSuccess();
    void updateModePassesTheLocalPortPairToTheService();
    void buildExceptionsDerivesBypassEntriesFromDirectRoutingRules();

    // Startup with the app's proxy off: a dead leftover of ours.
    void startupRecoveryRestoresSnapshotWhenTheRegistryPointsElsewhere();
    void startupRecoveryLeavesAForeignProxyAloneWithoutASnapshot();
    void startupRecoveryDisablesADeadLeftoverWithoutASnapshot();
    void startupRecoveryLeavesALiveProxyAlone();
    void startupRecoveryRestoresTheSnapshotForADeadLeftover();
    void startupRecoveryReportsFailureWhenTheDisableIsRefused();
};

void SystemProxyCoordinatorTests::updateModeReportsFailureWhenTheServiceRefusesTheWrite()
{
    Config config;
    config.localPort = 10808;

    MockConfigRepository repository;
    ServerService serverService(repository);
    FakeSystemProxyService systemProxyService;
    systemProxyService.accepts_ = false;

    SystemProxyCoordinator coordinator(
        SystemProxyCoordinator::Dependencies{config, serverService, &systemProxyService, nullptr},
        SystemProxyCoordinator::Callbacks{});

    // The refusal has to come back as false, otherwise every caller's `if (!updated)` branch --
    // and the user-visible "Failed to reapply system proxy settings." behind it -- is unreachable.
    QVERIFY(!coordinator.updateMode(SystemProxyMode::ForcedChange));
    QCOMPARE(systemProxyService.updateCalls_, 1);
}

void SystemProxyCoordinatorTests::updateModeReportsSuccessWhenTheServiceAcceptsTheWrite()
{
    Config config;
    config.localPort = 10808;

    MockConfigRepository repository;
    ServerService serverService(repository);
    FakeSystemProxyService systemProxyService;

    SystemProxyCoordinator coordinator(
        SystemProxyCoordinator::Dependencies{config, serverService, &systemProxyService, nullptr},
        SystemProxyCoordinator::Callbacks{});

    // Counterpart to the failure case: without it, the assertion above would also pass if
    // updateMode() returned false unconditionally, which would prove nothing.
    QVERIFY(coordinator.updateMode(SystemProxyMode::ForcedChange));
    QCOMPARE(systemProxyService.updateCalls_, 1);
}

void SystemProxyCoordinatorTests::updateModeTreatsAMissingServiceAsSuccess()
{
    Config config;
    config.localPort = 10808;

    MockConfigRepository repository;
    ServerService serverService(repository);

    // A null service means "this build has no system-proxy integration", not "the write failed".
    // Reporting failure here would make every caller show an error on a platform without one.
    SystemProxyCoordinator coordinator(
        SystemProxyCoordinator::Dependencies{config, serverService, nullptr, nullptr},
        SystemProxyCoordinator::Callbacks{});

    QVERIFY(coordinator.updateMode(SystemProxyMode::ForcedChange));
}

void SystemProxyCoordinatorTests::updateModePassesTheLocalPortPairToTheService()
{
    Config config;
    config.localPort = 10808;
    config.systemProxyAdvancedProtocol = QStringLiteral("{ip}:{http_port}/{socks_port}");

    MockConfigRepository repository;
    ServerService serverService(repository);
    FakeSystemProxyService systemProxyService;

    SystemProxyCoordinator coordinator(
        SystemProxyCoordinator::Dependencies{config, serverService, &systemProxyService, nullptr},
        SystemProxyCoordinator::Callbacks{});

    QVERIFY(coordinator.updateMode(SystemProxyMode::ForcedChange));

    // The HTTP inbound sits one above the SOCKS inbound; swapping them produces a system proxy
    // that points at the wrong listener and fails silently for every browser request.
    QCOMPARE(systemProxyService.lastMode_, SystemProxyMode::ForcedChange);
    QCOMPARE(systemProxyService.lastHttpPort_, 10809);
    QCOMPARE(systemProxyService.lastSocksPort_, 10808);
    QCOMPARE(systemProxyService.lastAdvancedProtocol_, QStringLiteral("{ip}:{http_port}/{socks_port}"));
    QVERIFY(!systemProxyService.lastProxyExceptions_.trimmed().isEmpty());
}

// Covers the routing-derived half of the bypass list, now shared through
// common/SystemProxyExceptions and reachable here through buildExceptions(). The system proxy needs
// a bypass entry for every host the user routed *directly*, and needs none for the hosts routed
// through the proxy -- a wrong direction here silently sends proxied traffic straight out.
void SystemProxyCoordinatorTests::buildExceptionsDerivesBypassEntriesFromDirectRoutingRules()
{
    Config config;
    config.localPort = 10808;

    RoutingRule suffixRule;
    suffixRule.outboundTag = QStringLiteral("direct");
    suffixRule.domain = QStringList{QStringLiteral("domain:example.com")};
    config.collection().routingCustomRules.append(suffixRule);

    RoutingRule exactRule;
    exactRule.outboundTag = QStringLiteral("direct");
    exactRule.domain = QStringList{QStringLiteral("full:exact.test")};
    config.collection().routingCustomRules.append(exactRule);

    // "direct" is matched case-insensitively, so a differently-cased tag must still be honoured.
    RoutingRule upperCaseRule;
    upperCaseRule.outboundTag = QStringLiteral("Direct");
    upperCaseRule.domain = QStringList{QStringLiteral(".dot.example")};
    config.collection().routingCustomRules.append(upperCaseRule);

    // A disabled rule is not part of the effective configuration.
    RoutingRule disabledRule;
    disabledRule.outboundTag = QStringLiteral("direct");
    disabledRule.domain = QStringList{QStringLiteral("domain:disabled.test")};
    disabledRule.enabled = false;
    config.collection().routingCustomRules.append(disabledRule);

    // Routed through the proxy: bypassing it would make Windows connect directly to a host the
    // user explicitly asked to proxy.
    RoutingRule proxiedRule;
    proxiedRule.outboundTag = QStringLiteral("proxy");
    proxiedRule.domain = QStringList{QStringLiteral("domain:proxied.test")};
    config.collection().routingCustomRules.append(proxiedRule);

    MockConfigRepository repository;
    ServerService serverService(repository);
    FakeSystemProxyService systemProxyService;

    SystemProxyCoordinator coordinator(
        SystemProxyCoordinator::Dependencies{config, serverService, &systemProxyService, nullptr},
        SystemProxyCoordinator::Callbacks{});

    const QStringList entries = coordinator.buildExceptions().split(QChar(';'));

    // "domain:" means the host and its subdomains, so the system proxy needs both forms.
    QVERIFY(entries.contains(QStringLiteral("example.com")));
    QVERIFY(entries.contains(QStringLiteral("*.example.com")));
    // A leading dot is the same shorthand, and "direct" was written with a capital D.
    QVERIFY(entries.contains(QStringLiteral("dot.example")));
    QVERIFY(entries.contains(QStringLiteral("*.dot.example")));

    // "full:" is the host alone -- no subdomain entry, and no proxy entry either.
    QVERIFY(entries.contains(QStringLiteral("exact.test")));
    QVERIFY(!entries.contains(QStringLiteral("*.exact.test")));

    QVERIFY(!entries.contains(QStringLiteral("disabled.test")));
    QVERIFY(!entries.contains(QStringLiteral("proxied.test")));

    // The built-in bypass list must survive: dropping it would route LAN and loopback traffic
    // through the proxy.
    QVERIFY(entries.contains(QStringLiteral("localhost")));
    QVERIFY(entries.contains(QStringLiteral("192.168.*")));
}

void SystemProxyCoordinatorTests::startupRecoveryRestoresSnapshotWhenTheRegistryPointsElsewhere()
{
    // Preserved behaviour: the registry holds the user's own proxy and a snapshot survived,
    // so startup puts the user's settings back -- the probe never runs on an entry that is
    // not ours.
    RecoveryFixture fixture;
    fixture.config.localPort = 10808;

    SystemProxySnapshot stored;
    stored.valid = true;
    stored.enabled = false;
    fixture.config.systemProxySnapshot() = stored;

    SystemProxySnapshot current;
    current.valid = true;
    current.enabled = true;
    current.server = QStringLiteral("10.0.0.1:3128");
    fixture.service.captured_ = current;

    fixture.build();
    fixture.recover();

    QCOMPARE(fixture.service.restoreCalls_, 1);
    QCOMPARE(fixture.service.lastRestored_, stored);
    QVERIFY(fixture.failures.isEmpty());
    QCOMPARE(fixture.messages.size(), 1);
    QVERIFY(fixture.messages.first().contains(
        QStringLiteral("Restored the system proxy settings captured before the previous session.")));
}

void SystemProxyCoordinatorTests::startupRecoveryLeavesAForeignProxyAloneWithoutASnapshot()
{
    // The registry holds the user's own proxy and no snapshot survived: nothing of ours to
    // undo, so the registry is left exactly as it was.
    RecoveryFixture fixture;
    fixture.config.localPort = 10808;

    SystemProxySnapshot current;
    current.valid = true;
    current.enabled = true;
    current.server = QStringLiteral("10.0.0.1:3128");
    fixture.service.captured_ = current;

    fixture.build();
    fixture.recover();

    QCOMPARE(fixture.service.restoreCalls_, 0);
    QVERIFY(fixture.messages.isEmpty());
    QVERIFY(fixture.failures.isEmpty());
}

void SystemProxyCoordinatorTests::startupRecoveryDisablesADeadLeftoverWithoutASnapshot()
{
    // The crash case with the snapshot lost: the registry still points at our own port and
    // nothing listens on it, so the leftover is turned off by restoring the sanitized
    // capture -- proxy off, leftover keys deleted.
    const int port = freeTcpPort();
    QVERIFY(port > 0);

    RecoveryFixture fixture;
    fixture.config.localHttpPort = port;

    SystemProxySnapshot current;
    current.valid = true;
    current.enabled = true;
    current.server = QStringLiteral("127.0.0.1:%1").arg(port);
    fixture.service.captured_ = current;

    fixture.build();
    fixture.recover();

    QCOMPARE(fixture.service.restoreCalls_, 1);
    QVERIFY(fixture.service.lastRestored_.valid);
    QVERIFY(!fixture.service.lastRestored_.enabled);
    QVERIFY(!fixture.service.lastRestored_.server.has_value());
    QVERIFY(fixture.failures.isEmpty());
    QCOMPARE(fixture.messages.size(), 1);
    QVERIFY(fixture.messages.first().contains(QStringLiteral("Removed a leftover system proxy")));
    QVERIFY(fixture.messages.first().contains(QStringLiteral("127.0.0.1:%1").arg(port)));
}

void SystemProxyCoordinatorTests::startupRecoveryLeavesALiveProxyAlone()
{
    // A listener on our own port means the proxy may be working -- another instance, or
    // another tool on the same port -- so the registry is left alone even with no snapshot.
    QTcpServer occupant;
    QVERIFY(occupant.listen(QHostAddress::LocalHost, 0));
    const int port = occupant.serverPort();

    RecoveryFixture fixture;
    fixture.config.localHttpPort = port;

    SystemProxySnapshot current;
    current.valid = true;
    current.enabled = true;
    current.server = QStringLiteral("127.0.0.1:%1").arg(port);
    fixture.service.captured_ = current;

    fixture.build();
    fixture.recover();

    QCOMPARE(fixture.service.restoreCalls_, 0);
    QVERIFY(fixture.messages.isEmpty());
    QVERIFY(fixture.failures.isEmpty());
}

void SystemProxyCoordinatorTests::startupRecoveryRestoresTheSnapshotForADeadLeftover()
{
    // The crash case with a surviving snapshot: the registry still points at our dead port,
    // and the snapshot -- not a blind clear -- says what was there before.
    const int port = freeTcpPort();
    QVERIFY(port > 0);

    RecoveryFixture fixture;
    fixture.config.localHttpPort = port;

    SystemProxySnapshot stored;
    stored.valid = true;
    stored.enabled = false;
    fixture.config.systemProxySnapshot() = stored;

    SystemProxySnapshot current;
    current.valid = true;
    current.enabled = true;
    current.server = QStringLiteral("127.0.0.1:%1").arg(port);
    fixture.service.captured_ = current;

    fixture.build();
    fixture.recover();

    QCOMPARE(fixture.service.restoreCalls_, 1);
    QCOMPARE(fixture.service.lastRestored_, stored);
    QVERIFY(fixture.failures.isEmpty());
    QCOMPARE(fixture.messages.size(), 1);
    QVERIFY(fixture.messages.first().contains(
        QStringLiteral("Restored the system proxy settings captured before the previous session.")));
}

void SystemProxyCoordinatorTests::startupRecoveryReportsFailureWhenTheDisableIsRefused()
{
    // The registry write can be refused (policy-locked machine): the failure is reported
    // instead of silently pretending the leftover is gone.
    const int port = freeTcpPort();
    QVERIFY(port > 0);

    RecoveryFixture fixture;
    fixture.config.localHttpPort = port;
    fixture.service.restoreAccepts_ = false;

    SystemProxySnapshot current;
    current.valid = true;
    current.enabled = true;
    current.server = QStringLiteral("127.0.0.1:%1").arg(port);
    fixture.service.captured_ = current;

    fixture.build();
    fixture.recover();

    QCOMPARE(fixture.service.restoreCalls_, 1);
    QVERIFY(fixture.messages.isEmpty());
    QCOMPARE(fixture.failures.size(), 1);
    QVERIFY(fixture.failures.first().contains(QStringLiteral("Could not remove the leftover system proxy")));
}

QTEST_MAIN(SystemProxyCoordinatorTests)

#include "SystemProxyCoordinatorTests.moc"
