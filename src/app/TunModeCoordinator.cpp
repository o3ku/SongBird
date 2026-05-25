#include "app/TunModeCoordinator.h"

#include <utility>

#include <QCoreApplication>

#include "app/TunAdminMessages.h"
#include "app/TunSettingsApplyDecision.h"
#include "common/SystemProxyMode.h"
#include "domain/models/VmessItem.h"
#include "services/ServerService.h"

TunModeCoordinator::TunModeCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

void TunModeCoordinator::setEnabled(bool enabled)
{
    if (deps_.config.tun().tunModeItem.enableTun == enabled) {
        syncStatusIndicators();
        return;
    }

    if (enabled && !hasActiveServer()) {
        appendResult(OperationResult::fail(QCoreApplication::translate("AppBootstrap", "Please select a server first.")));
        syncWindow();
        return;
    }

    const Config previousConfig = deps_.config;
    const Config updatedConfig = prepareTunToggleConfigForSave(
        deps_.config,
        enabled,
        isWindowsPlatform(),
        isProcessElevated());
    const bool shouldEnableProxyWithTun =
        enabled
        && !previousConfig.tun().tunModeItem.enableTun
        && !isCoreRunning();

    const TunSettingsSaveBehavior tunSaveBehavior = evaluateTunSettingsSaveBehavior(
        previousConfig,
        updatedConfig,
        isWindowsPlatform(),
        isProcessElevated(),
        isCoreRunning());
    const TunSettingsApplyDecision& tunDecision = tunSaveBehavior.applyDecision;

    if (tunSaveBehavior.shouldPromptForAdminRestart
        && callbacks_.askRestartAsAdministratorForTun
        && !callbacks_.askRestartAsAdministratorForTun()) {
        syncWindow();
        return;
    }

    deps_.config = tunSaveBehavior.configToPersist;
    if (!deps_.serverService.save(deps_.config)) {
        deps_.config = previousConfig;
        appendResult(OperationResult::fail(QStringLiteral("Failed to save the TUN mode setting.")));
        syncWindow();
        return;
    }

    appendResult(OperationResult::ok(
        enabled
            ? QStringLiteral("TUN mode enabled.")
            : QStringLiteral("TUN mode disabled.")));

    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        appendResult(OperationResult::fail(tunAdminRequiredSaveMessage()));
    }

    syncWindow();

    if (tunDecision.shouldRestartRunningCore) {
        if (callbacks_.restartCoreIfRunning) {
            callbacks_.restartCoreIfRunning(
                QCoreApplication::translate("AppBootstrap", "Reloading core after applying TUN mode changes."),
                true);
        }
    } else if (shouldEnableProxyWithTun && !tunSaveBehavior.shouldPromptForAdminRestart) {
        if (callbacks_.enableSystemProxy) {
            callbacks_.enableSystemProxy(true);
        }
    } else {
        syncStatusIndicators();
    }

    if (tunSaveBehavior.shouldPromptForAdminRestart) {
        if (callbacks_.persistUiState) {
            callbacks_.persistUiState();
        }
        deps_.config.ui().mainProxyEnabled = true;
        deps_.config.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);
        deps_.serverService.save(deps_.config);
        if (!callbacks_.restartApplication || !callbacks_.restartApplication(true)) {
            deps_.config = previousConfig;
            deps_.serverService.save(deps_.config);
            appendResult(OperationResult::fail(tunAdminRestartFailureMessage()));
            syncWindow();
        }
    }
}

bool TunModeCoordinator::isWindowsPlatform() const
{
    return callbacks_.isWindowsPlatform ? callbacks_.isWindowsPlatform() : false;
}

bool TunModeCoordinator::isProcessElevated() const
{
    return callbacks_.isProcessElevated ? callbacks_.isProcessElevated() : false;
}

bool TunModeCoordinator::isCoreRunning() const
{
    return callbacks_.isCoreRunning ? callbacks_.isCoreRunning() : false;
}

bool TunModeCoordinator::hasActiveServer() const
{
    return callbacks_.resolveActiveServer && callbacks_.resolveActiveServer().has_value();
}

void TunModeCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void TunModeCoordinator::syncWindow() const
{
    if (callbacks_.syncWindow) {
        callbacks_.syncWindow();
    }
}

void TunModeCoordinator::syncStatusIndicators() const
{
    if (callbacks_.syncStatusIndicators) {
        callbacks_.syncStatusIndicators();
    }
}
