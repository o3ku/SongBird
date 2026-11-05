#pragma once

#include <functional>

#include <QList>
#include <QPoint>
#include <QString>

#include "common/SubscriptionOrder.h"
#include "domain/models/SubItem.h"
#include "domain/models/VmessItem.h"

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
    // The servers the location items are built from. Held as a copy so rebuildTabs() can count
    // per-location nodes and drop empty items without a second caller racing to push them in; the
    // copy is refreshed on every config sync, which is also when the counts may have changed.
    void setServers(const QList<VmessItem>& servers);
    void setGroupByLocation(bool enabled);
    bool groupByLocation() const;
    void rebuildTabs(const QString& previousTabKey);
    // Sets the tab's hover text from the stored refresh time. Separate from rebuildTabs()
    // because a refresh that only touched the timestamp must not rebuild the bar: the bar's
    // rebuild re-selects a tab and resets its drag state, which is not something the user's
    // background update should do to the window they are looking at.
    void applyTabToolTips(qint64 nowMs, std::function<QString(const QString&)> networkErrorFor);
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
    //
    // handleTabMoved() only records the order: QTabBar emits tabMoved while the drag is still in
    // progress -- once for every neighbour the dragged tab slides past -- and the write-back is a
    // config save plus a full window refresh, which is far too heavy to run inside the bar's mouse
    // handling for every crossed neighbour (that was the drag lag). commitDraggedTabOrder() is the
    // other half: called when the drag ends, it hands the recorded order over exactly once.
    void handleTabMoved(int from, int to);
    void commitDraggedTabOrder();

private:
    void showContextMenu(const QString& tabKey, const QPoint& globalPosition, QWidget* parent);
    void rebuildSubscriptionTabs(const QString& previousTabKey);
    void rebuildLocationTabs(const QString& previousTabKey);
    void applyLocationFilter();
    // Returns the bar to "Ungrouped last" after a drag that dropped a subscription past it.
    // Called from commitDraggedTabOrder(), so the drag is over and the swap is safe.
    void restoreUngroupedTabAsLast();
    // A drag displaced Ungrouped itself: QTabBar expresses the swap from the displaced tab's
    // side (moveTab(ungrouped, draggedSlot)), so the tabMoved that arrives describes
    // Ungrouped's move, not the dragged subscription's. Reads the intended subscription order
    // back off the bar, stages it, and snaps Ungrouped to the last slot immediately -- the pin
    // holds mid-drag, not just after the release.
    void handleUngroupedDisplaced(int from, int to);
    QString subscriptionIdAtTab(int index) const;
    void invokeMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation);
    bool canMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation) const;
    QStringList movableSubscriptionIds() const;
    QTabBar* tabBarWithDragReorder() const;

    Context context_;
    MenuCallbacks callbacks_;
    QList<SubItem> subscriptions_;
    QList<VmessItem> servers_;
    bool groupByLocation_ = false;
    // True while rebuildTabs() is tearing the bar down and putting it back. QTabBar emits
    // tabMoved for removeTab()/insertTab() too, so without this every refresh would report the
    // reconstruction as the user reordering the tabs.
    bool rebuildingTabs_ = false;
    // True between commitDraggedTabOrder() handing an order to reorderSubscriptions and the refresh
    // that follows it. The bar already shows that order -- the drag just put it there -- and rebuilding
    // it synchronously while QTabBar still holds drag state for tabs that get removed trips
    // Q_ASSERT(index >= 0) in QTabBarPrivate::layoutTab (issue #37).
    bool skipTabsRebuildFromDragReorder_ = false;
    // True from the first tabMoved of a drag until commitDraggedTabOrder() runs. While set, the
    // in-memory subscriptions_ already carries the dragged order (the bar shows it too); the
    // persisted copy and the window refresh it causes wait for the end of the drag.
    bool dragCommitPending_ = false;
};
