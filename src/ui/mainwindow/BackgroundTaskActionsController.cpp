#include "ui/mainwindow/BackgroundTaskActionsController.h"

#include <QAction>

BackgroundTaskActionsController::BackgroundTaskActionsController(const Context& context)
    : context_(context)
{
}

void BackgroundTaskActionsController::sync(const State& state)
{
    if (context_.importClipboardAction != nullptr) {
        context_.importClipboardAction->setEnabled(state.canStartTask);
    }

    if (context_.updateSubscriptionsAction != nullptr) {
        context_.updateSubscriptionsAction->setEnabled(state.canStartTask);
    }

    if (context_.updateCurrentSubscriptionAction != nullptr) {
        context_.updateCurrentSubscriptionAction->setEnabled(state.canStartTask);
    }

    for (QAction* action : context_.updateCoreActions) {
        if (action != nullptr) {
            action->setEnabled(state.canStartTask);
        }
    }

    if (context_.updateGeoResourcesAction != nullptr) {
        context_.updateGeoResourcesAction->setEnabled(state.canStartTask);
    }

    if (context_.updateCurrentSubscriptionShortcutAction != nullptr) {
        context_.updateCurrentSubscriptionShortcutAction->setEnabled(state.canStartTask);
    }
}
