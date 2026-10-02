#include <QtTest>

#include <QStringList>

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

    bool accepts_ = true;
    bool enabled_ = false;
    mutable int updateCalls_ = 0;
    mutable int resetCalls_ = 0;
    mutable SystemProxyMode lastMode_ = SystemProxyMode::ForcedClear;
    mutable int lastHttpPort_ = 0;
    mutable int lastSocksPort_ = 0;
    mutable QString lastProxyExceptions_;
    mutable QString lastAdvancedProtocol_;
};

} // namespace

class SystemProxyCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void updateModeReportsFailureWhenTheServiceRefusesTheWrite();
    void updateModeReportsSuccessWhenTheServiceAcceptsTheWrite();
    void updateModeTreatsAMissingServiceAsSuccess();
    void updateModePassesTheLocalPortPairToTheService();
    void buildExceptionsDerivesBypassEntriesFromDirectRoutingRules();
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

// Covers collectRouteDerivedProxyExceptions(), which lives in an anonymous namespace in
// SystemProxyCoordinator.cpp and is reachable only through buildExceptions(). The system proxy
// needs a bypass entry for every host the user routed *directly*, and needs none for the hosts
// routed through the proxy -- a wrong direction here silently sends proxied traffic straight out.
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

QTEST_MAIN(SystemProxyCoordinatorTests)

#include "SystemProxyCoordinatorTests.moc"
