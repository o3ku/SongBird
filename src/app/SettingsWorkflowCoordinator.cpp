#include "app/SettingsWorkflowCoordinator.h"

#include <utility>

#include "ui/dialogs/SettingsDialog.h"

SettingsWorkflowCoordinator::SettingsWorkflowCoordinator(Dependencies dependencies, QObject* parent)
    : QObject(parent)
    , deps_(std::move(dependencies))
{
}

void SettingsWorkflowCoordinator::openSettingsDialog(int initialTab)
{
    if (deps_.isAvailable && !deps_.isAvailable()) {
        return;
    }

    QWidget* parent = deps_.dialogParent ? deps_.dialogParent() : nullptr;
    if (parent == nullptr) {
        return;
    }

    if (deps_.refreshExistingCoreTypes) {
        deps_.refreshExistingCoreTypes();
    }

    SettingsDialogRunner session(
        parent,
        deps_.trackBackgroundThread,
        deps_.detectCoreVersion,
        [this](CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard) {
            handleCoreDownload(requestedCoreType, dialogGuard);
        },
        [this]() {
            if (deps_.cancelCoreUpdate) {
                deps_.cancelCoreUpdate();
            }
        });

    const SettingsDialogRunner::Result sessionResult = session.exec(
        deps_.currentConfig ? deps_.currentConfig() : Config(),
        initialTab,
        deps_.existingCoreTypes ? deps_.existingCoreTypes() : QList<CoreType>{},
        deps_.lifetimeGuard ? deps_.lifetimeGuard() : std::weak_ptr<char>{});

    if (sessionResult.outcome == SettingsDialogRunner::Outcome::Cancelled) {
        return;
    }

    if (sessionResult.outcome == SettingsDialogRunner::Outcome::RestoreBackup) {
        if (deps_.restoreConfigFromBackup) {
            deps_.restoreConfigFromBackup();
        }
        return;
    }

    if (sessionResult.outcome == SettingsDialogRunner::Outcome::UpdateSubscriptions) {
        if (deps_.saveAndUpdateSubscriptions) {
            deps_.saveAndUpdateSubscriptions(sessionResult.selectedSubscriptionIds, sessionResult.config);
        }
        return;
    }

    if (deps_.applySettings) {
        deps_.applySettings(sessionResult.config);
    }
}

void SettingsWorkflowCoordinator::handleCoreDownload(CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard)
{
    if (!deps_.updateCore) {
        return;
    }

    const auto updateStarted = std::make_shared<bool>(false);
    // Both observers can be the first to arrive (a byte-level progress event may
    // precede the first text milestone), so the "this core is now updating"
    // transition is idempotent and shared instead of living in one of them.
    const auto ensureUpdateStarted = [dialogGuard, requestedCoreType, updateStarted]() {
        if (dialogGuard.isNull() || *updateStarted) {
            return;
        }
        dialogGuard->beginCoreUpdate(requestedCoreType);
        *updateStarted = true;
    };
    deps_.updateCore(
        requestedCoreType,
        dialogGuard.data(),
        dialogGuard.data(),
        [dialogGuard, requestedCoreType, ensureUpdateStarted](const QString& message) {
            if (dialogGuard.isNull()) {
                return;
            }
            ensureUpdateStarted();
            dialogGuard->setCoreUpdateProgress(requestedCoreType, message);
        },
        [dialogGuard, requestedCoreType, ensureUpdateStarted](qint64 bytesReceived, qint64 bytesTotal) {
            if (dialogGuard.isNull()) {
                return;
            }
            ensureUpdateStarted();
            dialogGuard->setCoreUpdateDownloadProgress(requestedCoreType, bytesReceived, bytesTotal);
        },
        [this, dialogGuard, requestedCoreType](const OperationResult& result) {
            if (dialogGuard.isNull()) {
                return;
            }

            if (result.success && deps_.existingCoreTypes && deps_.detectCoreVersion) {
                dialogGuard->setExistingCoreTypes(deps_.existingCoreTypes());
                dialogGuard->setCoreVersion(requestedCoreType, deps_.detectCoreVersion(requestedCoreType));
            }
            dialogGuard->finishCoreUpdate(requestedCoreType, result.success, result.message);
        });
}
