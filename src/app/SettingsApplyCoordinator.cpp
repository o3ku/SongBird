#include "app/SettingsApplyCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QSet>

#include "app/SettingsDialogApplyPlan.h"
#include "app/StartupAdminElevation.h"
#include "app/TunAdminMessages.h"
#include "platform/windows/WindowsAutoRunService.h"
#include "services/ServerService.h"
#include "services/SubscriptionService.h"

namespace {

QSet<QString> collectSubscriptionIds(const Config& config)
{
    QSet<QString> ids;
    for (const SubItem& item : config.collection().subscriptions) {
        const QString id = item.id.trimmed();
        if (!id.isEmpty()) {
            ids.insert(id);
        }
    }
    return ids;
}

QStringList collectNewEnabledSubscriptionIds(const Config& config, const QSet<QString>& previousIds)
{
    QStringList ids;
    for (const SubItem& item : config.collection().subscriptions) {
        if (!item.enabled) {
            continue;
        }

        const QString id = item.id.trimmed();
        if (!id.isEmpty() && !previousIds.contains(id)) {
            ids.append(id);
        }
    }
    return ids;
}

} // namespace

SettingsApplyCoordinator::SettingsApplyCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

void SettingsApplyCoordinator::apply(const Config& updatedConfig)
{
    const Config previousConfig = deps_.config;
    const QSet<QString> previousSubIds = collectSubscriptionIds(previousConfig);

    const SettingsDialogApplyPlan plan = evaluateSettingsDialogApplyPlan(
        previousConfig,
        updatedConfig,
        isWindowsPlatform(),
        isProcessElevated(),
        isCoreRunning());

    if (!plan.dirty) {
        syncWindow();
        return;
    }

    deps_.config = plan.tunSaveBehavior.configToPersist;
    SubscriptionService::normalizeSubscriptionIds(deps_.config.collection().subscriptions);

    const QStringList newSubIds = collectNewEnabledSubscriptionIds(deps_.config, previousSubIds);
    if (!deps_.serverService.save(deps_.config)) {
        deps_.config = previousConfig;
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to save settings.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Settings saved.")));
    if (plan.tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }
    syncWindow();

    if (!newSubIds.isEmpty() && callbacks_.ui.selectSubscriptionTab && callbacks_.workflow.updateSubscriptionsByIds) {
        callbacks_.ui.selectSubscriptionTab(newSubIds.constLast());
        callbacks_.workflow.updateSubscriptionsByIds(
            newSubIds,
            false,
            QCoreApplication::translate("AppBootstrap", "Updating new subscriptions in the background..."));
    }

    if (plan.autoRunChanged && deps_.autoRunService != nullptr) {
        const bool success = deps_.autoRunService->setEnabled(updatedConfig.ui().autoRunEnabled);
        if (!success) {
            appendResult(OperationResult::fail(QStringLiteral("Failed to update auto run setting.")));
        }
    }

    if (plan.systemProxySettingsChanged && hasSystemProxyService() && isCoreReady()) {
        const SystemProxyMode mode = normalizeSystemProxyMode(deps_.config.sysProxyType);
        const bool proxyUpdated = callbacks_.systemProxy.updateMode && callbacks_.systemProxy.updateMode(mode);
        if (!proxyUpdated) {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Failed to reapply system proxy settings.")));
        } else if (callbacks_.systemProxy.adoptManagedProxy) {
            callbacks_.systemProxy.adoptManagedProxy(expectedSystemProxyEnabled(mode));
        }
    }

    if (plan.runtimeSettingsChanged) {
        if (callbacks_.runtime.restartCoreIfRunning) {
            callbacks_.runtime.restartCoreIfRunning(
                QCoreApplication::translate(
                    "AppBootstrap",
                    "Reloading core after applying settings changes."),
                true);
        }
    } else {
        syncStatusIndicators();
    }

    const bool adminRestartInitiated =
        plan.tunSaveBehavior.shouldPromptForAdminRestart
        && callbacks_.workflow.promptRestartAsAdministratorForTun
        && callbacks_.workflow.promptRestartAsAdministratorForTun();

    if (shouldPromptForLanguageRestartAfterSettingsSave(plan.languageChanged, adminRestartInitiated)
        && callbacks_.workflow.promptRestartForLanguageChange) {
        callbacks_.workflow.promptRestartForLanguageChange();
    }
}

bool SettingsApplyCoordinator::isWindowsPlatform() const
{
    return callbacks_.platform.isWindowsPlatform && callbacks_.platform.isWindowsPlatform();
}

bool SettingsApplyCoordinator::isProcessElevated() const
{
    return callbacks_.platform.isProcessElevated && callbacks_.platform.isProcessElevated();
}

bool SettingsApplyCoordinator::isCoreRunning() const
{
    return callbacks_.runtime.isCoreRunning && callbacks_.runtime.isCoreRunning();
}

bool SettingsApplyCoordinator::isCoreReady() const
{
    return callbacks_.runtime.isCoreReady && callbacks_.runtime.isCoreReady();
}

bool SettingsApplyCoordinator::hasSystemProxyService() const
{
    return callbacks_.systemProxy.hasService && callbacks_.systemProxy.hasService();
}

void SettingsApplyCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.ui.appendResult) {
        callbacks_.ui.appendResult(result);
    }
}

void SettingsApplyCoordinator::syncWindow() const
{
    if (callbacks_.ui.syncWindow) {
        callbacks_.ui.syncWindow();
    }
}

void SettingsApplyCoordinator::syncStatusIndicators() const
{
    if (callbacks_.ui.syncStatusIndicators) {
        callbacks_.ui.syncStatusIndicators();
    }
}
