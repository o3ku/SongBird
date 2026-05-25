#pragma once

#include <functional>

#include <QObject>

class QAction;
class QLineEdit;
class QTabBar;
class QWidget;

class MainWindowShortcutController final : public QObject {
public:
    struct Context {
        QWidget* owner = nullptr;
        QLineEdit* serverFilterEdit = nullptr;
        QTabBar* subscriptionTabBar = nullptr;
        QAction* testAction = nullptr;
        QAction* updateSubscriptionsAction = nullptr;
        QAction* updateCurrentSubscriptionAction = nullptr;
    };

    struct Callbacks {
        std::function<void()> triggerCurrentSubscriptionUpdate;
        std::function<void()> requestExit;
    };

    MainWindowShortcutController(const Context& context, Callbacks callbacks, QObject* parent = nullptr);

    void setup();

private:
    void setupFilterShortcut();
    void setupUpdateShortcuts();
    void setupExitShortcut();
    void setupSubscriptionTabShortcuts();

    Context context_;
    Callbacks callbacks_;
};
