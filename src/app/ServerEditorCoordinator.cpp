#include "app/ServerEditorCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QDialog>

#include "services/ServerService.h"
#include "ui/dialogs/AddServerDialog.h"
#include "ui/dialogs/CustomServerDialog.h"

ServerEditorCoordinator::ServerEditorCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

void ServerEditorCoordinator::addServer()
{
    AddServerDialog dialog(dialogParent());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    appendResult(deps_.serverService.addServer(deps_.config, dialog.server()));
    syncWindow();
}

void ServerEditorCoordinator::editServer(const QString& indexId)
{
    const std::optional<VmessItem> existing = findServer(indexId);
    if (!existing.has_value()) {
        appendLog(QStringLiteral("The selected server could not be found for editing."));
        return;
    }

    const std::optional<VmessItem> activeServer = resolveActiveServer();
    const QString activeServerId = activeServer.has_value() ? activeServer->indexId : QString();
    const bool shouldRestartCore = isCoreRunning() && activeServerId == indexId;

    OperationResult result;
    if (existing->configType == ConfigType::Custom) {
        CustomServerDialog dialog(dialogParent());
        dialog.setWindowTitle(QCoreApplication::translate("ServerEditorCoordinator", "Edit Custom Server"));
        dialog.setServer(*existing, deps_.serverService.resolveCustomConfigPath(existing->address));
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        result = deps_.serverService.updateServer(deps_.config, indexId, dialog.server());
    } else {
        AddServerDialog dialog(dialogParent());
        dialog.setWindowTitle(QCoreApplication::translate("ServerEditorCoordinator", "Edit Server"));
        dialog.setServer(*existing);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        result = deps_.serverService.updateServer(deps_.config, indexId, dialog.server());
    }

    appendResult(result);
    syncWindow();
    if (result.success && shouldRestartCore) {
        restartCoreIfRunning(QStringLiteral("Reloading core after editing the active server."));
    }
}

QWidget* ServerEditorCoordinator::dialogParent() const
{
    return callbacks_.dialogParent ? callbacks_.dialogParent() : nullptr;
}

void ServerEditorCoordinator::appendLog(const QString& message) const
{
    if (callbacks_.appendLog) {
        callbacks_.appendLog(message);
    }
}

void ServerEditorCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void ServerEditorCoordinator::syncWindow() const
{
    if (callbacks_.syncWindow) {
        callbacks_.syncWindow();
    }
}

bool ServerEditorCoordinator::isCoreRunning() const
{
    return callbacks_.isCoreRunning ? callbacks_.isCoreRunning() : false;
}

std::optional<VmessItem> ServerEditorCoordinator::findServer(const QString& indexId) const
{
    return callbacks_.findServer ? callbacks_.findServer(indexId) : std::nullopt;
}

std::optional<VmessItem> ServerEditorCoordinator::resolveActiveServer() const
{
    return callbacks_.resolveActiveServer ? callbacks_.resolveActiveServer() : std::nullopt;
}

void ServerEditorCoordinator::restartCoreIfRunning(const QString& reason) const
{
    if (callbacks_.restartCoreIfRunning) {
        callbacks_.restartCoreIfRunning(reason, true);
    }
}
