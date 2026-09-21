#include <QtTest>

#include "app/SystemProxyCoordinator.h"
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
// HKCU\...\Internet Settings and cannot be made to refuse that write from a test, which is why
// the coordinator's failure branch had no coverage: `accepts_` is the refusal.
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

QTEST_MAIN(SystemProxyCoordinatorTests)

#include "SystemProxyCoordinatorTests.moc"
