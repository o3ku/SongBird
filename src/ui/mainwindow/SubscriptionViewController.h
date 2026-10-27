#pragma once

#include <functional>

#include <QList>
#include <QPoint>
#include <QString>

#include "common/SubscriptionOrder.h"
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
        std::function<void(const QString&, SubscriptionOrder::MoveOperation)> moveSubscription;
        std::function<void(const QList<SubItem>&)> reorderSubscriptions;
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
    void showContextMenuForTabKey(const QString& tabKey, const QPoint& globalPosition);

    // Drag-to-reorder. The dragged tab's data is handed back as "sub:<id>", exactly the key the
    // context menu uses, so the two paths share one notion of what a tab addresses.
    // The tab data for a tab that may be dragged: empty for Ungrouped, which is not a subscription.
    QString draggableTabData(int index) const;
    void handleTabMoved(int from, int to);

private:
    void showContextMenu(const QString& tabKey, const QPoint& globalPosition, QWidget* parent);
    QString subscriptionIdAtTab(int index) const;
    void invokeMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation);
    bool canMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation) const;
    QTabBar* tabBarWithDragReorder() const;

    Context context_;
    MenuCallbacks callbacks_;
    QList<SubItem> subscriptions_;
    // True while rebuildTabs() is tearing the bar down and putting it back. QTabBar emits
    // tabMoved for removeTab()/insertTab() too, so without this every refresh would report the
    // reconstruction as the user reordering the tabs.
    bool rebuildingTabs_ = false;
};
