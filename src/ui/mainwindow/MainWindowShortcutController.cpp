#include "ui/mainwindow/MainWindowShortcutController.h"

#include <utility>

#include <QAction>
#include <QKeySequence>
#include <QLineEdit>
#include <QTabBar>
#include <QWidget>

MainWindowShortcutController::MainWindowShortcutController(
    const Context& context,
    Callbacks callbacks,
    QObject* parent)
    : QObject(parent)
    , context_(context)
    , callbacks_(std::move(callbacks))
{
}

void MainWindowShortcutController::setup()
{
    setupFilterShortcut();
    setupUpdateShortcuts();
    setupExitShortcut();
    setupSubscriptionTabShortcuts();
}

void MainWindowShortcutController::setupFilterShortcut()
{
    if (context_.owner == nullptr || context_.serverFilterEdit == nullptr) {
        return;
    }

    auto* focusServerFilterShortcutAction = new QAction(this);
    focusServerFilterShortcutAction->setObjectName(QStringLiteral("focusServerFilterShortcutAction"));
    focusServerFilterShortcutAction->setShortcut(QKeySequence::Find);
    focusServerFilterShortcutAction->setShortcutContext(Qt::WindowShortcut);
    connect(focusServerFilterShortcutAction, &QAction::triggered, this, [this]() {
        if (context_.serverFilterEdit != nullptr) {
            context_.serverFilterEdit->setFocus();
            context_.serverFilterEdit->selectAll();
        }
    });
    context_.owner->addAction(focusServerFilterShortcutAction);
}

void MainWindowShortcutController::setupUpdateShortcuts()
{
    if (context_.owner == nullptr) {
        return;
    }

    if (context_.testAction != nullptr) {
        context_.testAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_T));
        context_.testAction->setShortcutContext(Qt::WindowShortcut);
        context_.owner->addAction(context_.testAction);
    }

    if (context_.updateSubscriptionsAction != nullptr) {
        context_.updateSubscriptionsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
        context_.updateSubscriptionsAction->setShortcutContext(Qt::WindowShortcut);
        context_.owner->addAction(context_.updateSubscriptionsAction);
    }

    if (context_.updateCurrentSubscriptionAction != nullptr) {
        context_.updateCurrentSubscriptionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
        context_.updateCurrentSubscriptionAction->setShortcutContext(Qt::WindowShortcut);
        connect(context_.updateCurrentSubscriptionAction, &QAction::triggered, this, [this]() {
            if (callbacks_.triggerCurrentSubscriptionUpdate) {
                callbacks_.triggerCurrentSubscriptionUpdate();
            }
        });
        context_.owner->addAction(context_.updateCurrentSubscriptionAction);
    }
}

void MainWindowShortcutController::setupExitShortcut()
{
    if (context_.owner == nullptr) {
        return;
    }

    auto* exitShortcutAction = new QAction(this);
    exitShortcutAction->setObjectName(QStringLiteral("exitShortcutAction"));
    exitShortcutAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_X));
    exitShortcutAction->setShortcutContext(Qt::WindowShortcut);
    connect(exitShortcutAction, &QAction::triggered, this, [this]() {
        if (callbacks_.requestExit) {
            callbacks_.requestExit();
        }
    });
    context_.owner->addAction(exitShortcutAction);
}

void MainWindowShortcutController::setupSubscriptionTabShortcuts()
{
    if (context_.owner == nullptr || context_.subscriptionTabBar == nullptr) {
        return;
    }

    for (int index = 0; index < 9; ++index) {
        auto* switchTabShortcutAction = new QAction(this);
        switchTabShortcutAction->setObjectName(QStringLiteral("subscriptionTabShortcutAction%1").arg(index + 1));
        switchTabShortcutAction->setShortcut(QKeySequence(Qt::ALT | (Qt::Key_1 + index)));
        switchTabShortcutAction->setShortcutContext(Qt::WindowShortcut);
        connect(switchTabShortcutAction, &QAction::triggered, this, [this, index]() {
            if (context_.subscriptionTabBar != nullptr && index < context_.subscriptionTabBar->count()) {
                context_.subscriptionTabBar->setCurrentIndex(index);
            }
        });
        context_.owner->addAction(switchTabShortcutAction);
    }
}
