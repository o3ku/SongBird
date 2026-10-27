#pragma once

#include <functional>

#include <QList>
#include <QPoint>
#include <QString>

#include "common/SubscriptionOrder.h"
#include "domain/models/SubItem.h"

class QLineEdit;
class QMenu;
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
    // The tab bar only shows enabled subscriptions, so a hidden one has no tab and no context menu
    // -- and moving it was the one reason to hide it in the first place. The window's tools menu
    // offers the same four moves for the current tab's subscription plus the hidden ones here.
    void showSubscriptionMoveMenu(const QPoint& globalPosition, QWidget* parent);
    void populateSubscriptionMoveMenu(QMenu* menu, const QString& subscriptionId);

    // Drag-to-reorder. A tab's data is "sub:<id>", exactly the key the context menu uses, so the
    // two paths share one notion of what a tab addresses. Ungrouped has no id, and handleTabMoved()
    // refuses a drop that lands on it as a rank -- it is pinned last and is not part of the order.
    void handleTabMoved(int from, int to);

private:
    void showContextMenu(const QString& tabKey, const QPoint& globalPosition, QWidget* parent);
    QString subscriptionIdAtTab(int index) const;
    void invokeMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation);
    bool canMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation) const;
    QStringList movableSubscriptionIds() const;
    QTabBar* tabBarWithDragReorder() const;

    Context context_;
    MenuCallbacks callbacks_;
    QList<SubItem> subscriptions_;
    // True while rebuildTabs() is tearing the bar down and putting it back. QTabBar emits
    // tabMoved for removeTab()/insertTab() too, so without this every refresh would report the
    // reconstruction as the user reordering the tabs.
    bool rebuildingTabs_ = false;
};
