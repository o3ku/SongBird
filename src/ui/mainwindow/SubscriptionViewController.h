#pragma once

#include <functional>

#include <QList>
#include <QPoint>
#include <QString>

#include "domain/models/SubItem.h"

class QLineEdit;
class QTabBar;
class ServerFilterProxyModel;
class QWidget;

class SubscriptionViewController final {
public:
    struct Context {
        QWidget* owner = nullptr;
        QTabBar* subscriptionTabBar = nullptr;
        QLineEdit* serverFilterEdit = nullptr;
        ServerFilterProxyModel* serverFilterModel = nullptr;
    };

    struct MenuCallbacks {
        std::function<bool()> canStartBackgroundTask;
        std::function<void()> copyCurrentSubscriptionUrl;
        std::function<void(const QString&)> testSubscriptionServers;
        std::function<void(const QString&)> updateCurrentSubscription;
        std::function<void(const QString&)> updateCurrentSubscriptionViaProxy;
        std::function<void(const QString&)> hideSubscription;
        std::function<bool(const QString&)> confirmAndDeleteSubscription;
        std::function<void()> openSubscriptionManagement;
    };

    SubscriptionViewController(
        const Context& context,
        MenuCallbacks callbacks);

    void setSubscriptions(const QList<SubItem>& subscriptions);
    void rebuildTabs(const QString& previousTabKey);
    void applyFilter(const QString& filterText);
    void applyCurrentTabFilter();

    bool selectSubscriptionTab(const QString& selectionId);
    QString currentTabKey() const;
    QString currentSubscriptionUrl() const;
    QString persistedSelectionId() const;
    bool isUngroupedTabSelected() const;

    void showTabContextMenu(const QPoint& position);

private:
    Context context_;
    MenuCallbacks callbacks_;
    QList<SubItem> subscriptions_;
};
