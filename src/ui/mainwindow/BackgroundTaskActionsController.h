#pragma once

#include <QList>

class QAction;

class BackgroundTaskActionsController final {
public:
    struct Context {
        QAction* importClipboardAction = nullptr;
        QAction* updateSubscriptionsAction = nullptr;
        QAction* updateCurrentSubscriptionAction = nullptr;
        QList<QAction*> updateCoreActions;
        QAction* updateGeoResourcesAction = nullptr;
        QAction* updateCurrentSubscriptionShortcutAction = nullptr;
    };

    struct State {
        bool canStartTask = false;
    };

    explicit BackgroundTaskActionsController(const Context& context);

    void sync(const State& state);

private:
    Context context_;
};
