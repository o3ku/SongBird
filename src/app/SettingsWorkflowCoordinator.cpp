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

    SettingsDialogSession session(
        parent,
        deps_.trackBackgroundThread,
        deps_.detectCoreVersion,
        [this](CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard) {
            handleCoreDownload(requestedCoreType, dialogGuard);
        });

    const SettingsDialogSession::Result sessionResult = session.exec(
        deps_.currentConfig ? deps_.currentConfig() : Config(),
        initialTab,
        deps_.existingCoreTypes ? deps_.existingCoreTypes() : QList<CoreType>{},
        deps_.lifetimeGuard ? deps_.lifetimeGuard() : std::weak_ptr<char>{});

    if (sessionResult.outcome == SettingsDialogSession::Outcome::Cancelled) {
        return;
    }

    if (sessionResult.outcome == SettingsDialogSession::Outcome::RestoreBackup) {
        if (deps_.restoreConfigFromBackup) {
            deps_.restoreConfigFromBackup();
        }
        return;
    }

    if (sessionResult.outcome == SettingsDialogSession::Outcome::UpdateSubscriptions) {
        if (deps_.saveAndUpdateSubscriptions) {
            deps_.saveAndUpdateSubscriptions(sessionResult.selectedSubscriptionRows, sessionResult.config);
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
    deps_.updateCore(
        requestedCoreType,
        dialogGuard.data(),
        dialogGuard.data(),
        [dialogGuard, requestedCoreType, updateStarted](const QString& message) {
            if (!dialogGuard.isNull()) {
                if (!*updateStarted) {
                    dialogGuard->beginCoreUpdate(requestedCoreType);
                    *updateStarted = true;
                }
                dialogGuard->setCoreUpdateProgress(requestedCoreType, message);
            }
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
