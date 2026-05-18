#pragma once

class QAction;

class BackgroundTaskActionsController final {
public:
    struct Context {
        QAction* copySubscriptionUrlAction = nullptr;
        QAction* importClipboardAction = nullptr;
        QAction* updateSubscriptionsAction = nullptr;
        QAction* updateCurrentSubscriptionAction = nullptr;
        QAction* testMeAction = nullptr;
        QAction* updateXrayCoreAction = nullptr;
        QAction* updateSingBoxCoreAction = nullptr;
        QAction* updateGeoResourcesAction = nullptr;
        QAction* updateCurrentSubscriptionShortcutAction = nullptr;
    };

    struct State {
        bool hasSubscriptionUrl = false;
        bool canStartTask = false;
    };

    explicit BackgroundTaskActionsController(const Context& context);

    void sync(const State& state);

private:
    Context context_;
};
