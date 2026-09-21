#include <QtTest>

#include <QStringList>

#include "app/SettingsApplyCoordinator.h"
#include "persistence/IConfigRepository.h"
#include "platform/IAutoRunService.h"
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

// Stands in for the registry-backed implementation, which cannot be made to refuse a write from a
// test: the real service would only fail if Windows denied the Run-key write, and there is no way
// to arrange that from here. `succeeds_` is that denial.
class FakeAutoRunService : public IAutoRunService {
public:
    bool isEnabled() const override { return enabled_; }

    bool setEnabled(bool enabled) const override
    {
        ++writeAttempts_;
        if (succeeds_) {
            enabled_ = enabled;
        }
        return succeeds_;
    }

    bool succeeds_ = true;
    mutable bool enabled_ = false;
    mutable int writeAttempts_ = 0;
};

} // namespace

class SettingsApplyCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void reportsFailureWhenAutoRunWriteIsRefused();
    void appliesAutoRunChangeWhenWriteSucceeds();
    void reportsFailureWhenSystemProxyReapplyIsRefused();
    void reappliesSystemProxyWhenTheUpdateSucceeds();
};

void SettingsApplyCoordinatorTests::reportsFailureWhenAutoRunWriteIsRefused()
{
    Config config;
    config.ui().autoRunEnabled = false;

    MockConfigRepository repository;
    ServerService serverService(repository);
    FakeAutoRunService autoRunService;
    autoRunService.succeeds_ = false;

    QStringList messages;
    SettingsApplyCoordinator::Callbacks callbacks;
    callbacks.ui.appendResult = [&messages](const OperationResult& result) {
        messages.append(result.message);
    };

    SettingsApplyCoordinator coordinator(
        SettingsApplyCoordinator::Dependencies{config, serverService, &autoRunService},
        callbacks);

    Config updated = config;
    updated.ui().autoRunEnabled = true;
    coordinator.apply(updated);

    // The write was attempted and refused, so the user has to be told: silently keeping the old
    // Run key while the settings file says otherwise is the failure this branch exists to prevent.
    QCOMPARE(autoRunService.writeAttempts_, 1);
    QVERIFY(messages.contains(QStringLiteral("Failed to update auto run setting.")));
}

void SettingsApplyCoordinatorTests::appliesAutoRunChangeWhenWriteSucceeds()
{
    Config config;
    config.ui().autoRunEnabled = false;

    MockConfigRepository repository;
    ServerService serverService(repository);
    FakeAutoRunService autoRunService;

    QStringList messages;
    SettingsApplyCoordinator::Callbacks callbacks;
    callbacks.ui.appendResult = [&messages](const OperationResult& result) {
        messages.append(result.message);
    };

    SettingsApplyCoordinator coordinator(
        SettingsApplyCoordinator::Dependencies{config, serverService, &autoRunService},
        callbacks);

    Config updated = config;
    updated.ui().autoRunEnabled = true;
    coordinator.apply(updated);

    // Counterpart to the failure case: without this, the assertion above would also pass if the
    // message were emitted unconditionally, which would prove nothing about the refusal path.
    QCOMPARE(autoRunService.writeAttempts_, 1);
    QVERIFY(autoRunService.enabled_);
    QVERIFY(!messages.contains(QStringLiteral("Failed to update auto run setting.")));
}

void SettingsApplyCoordinatorTests::reportsFailureWhenSystemProxyReapplyIsRefused()
{
    Config config;
    config.localPort = 10808;

    MockConfigRepository repository;
    ServerService serverService(repository);

    QStringList messages;
    SettingsApplyCoordinator::Callbacks callbacks;
    callbacks.ui.appendResult = [&messages](const OperationResult& result) {
        messages.append(result.message);
    };
    callbacks.platform.isWindowsPlatform = []() { return true; };
    callbacks.runtime.isCoreReady = []() { return true; };
    callbacks.systemProxy.hasService = []() { return true; };
    // The registry write behind this callback is what the stand-in refuses.
    callbacks.systemProxy.updateMode = [](SystemProxyMode) { return false; };

    SettingsApplyCoordinator coordinator(
        SettingsApplyCoordinator::Dependencies{config, serverService, nullptr},
        callbacks);

    Config updated = config;
    updated.localPort = 20808;
    coordinator.apply(updated);

    QVERIFY(messages.contains(QStringLiteral("Failed to reapply system proxy settings.")));
}

void SettingsApplyCoordinatorTests::reappliesSystemProxyWhenTheUpdateSucceeds()
{
    Config config;
    config.localPort = 10808;
    // adoptManagedProxy is called with expectedSystemProxyEnabled(mode), and only ForcedChange
    // counts as enabled -- so the configured mode has to be set for the assertion to mean anything.
    config.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);

    MockConfigRepository repository;
    ServerService serverService(repository);

    QStringList messages;
    bool adopted = false;
    bool adoptedValue = false;
    SettingsApplyCoordinator::Callbacks callbacks;
    callbacks.ui.appendResult = [&messages](const OperationResult& result) {
        messages.append(result.message);
    };
    callbacks.platform.isWindowsPlatform = []() { return true; };
    callbacks.runtime.isCoreReady = []() { return true; };
    callbacks.systemProxy.hasService = []() { return true; };
    callbacks.systemProxy.updateMode = [](SystemProxyMode) { return true; };
    callbacks.systemProxy.adoptManagedProxy = [&adopted, &adoptedValue](bool enabled) {
        adopted = true;
        adoptedValue = enabled;
    };

    SettingsApplyCoordinator coordinator(
        SettingsApplyCoordinator::Dependencies{config, serverService, nullptr},
        callbacks);

    Config updated = config;
    updated.localPort = 20808;
    coordinator.apply(updated);

    QVERIFY(!messages.contains(QStringLiteral("Failed to reapply system proxy settings.")));
    // Success has to be distinguishable from refusal by more than the absence of an error: the
    // managed-proxy bookkeeping only runs on the success path.
    QVERIFY(adopted);
    QVERIFY(adoptedValue);
}

QTEST_MAIN(SettingsApplyCoordinatorTests)

#include "SettingsApplyCoordinatorTests.moc"
