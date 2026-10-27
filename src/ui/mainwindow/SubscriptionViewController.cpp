#include "ui/mainwindow/SubscriptionViewController.h"

#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QTabBar>

#include <algorithm>
#include <iterator>

#include "ui/mainwindow/SubscriptionViewSupport.h"
#include "ui/models/ServerFilterProxyModel.h"

SubscriptionViewController::SubscriptionViewController(
    const Context& context,
    MenuCallbacks callbacks)
    : context_(context)
    , callbacks_(std::move(callbacks))
{
}

void SubscriptionViewController::setSubscriptions(const QList<SubItem>& subscriptions)
{
    subscriptions_ = subscriptions;
}

void SubscriptionViewController::rebuildTabs(const QString& previousTabKey)
{
    if (context_.subscriptionTabBar == nullptr || context_.serverFilterModel == nullptr) {
        return;
    }

    context_.serverFilterModel->setKnownSubscriptionIds(
        SubscriptionViewSupport::enabledSubscriptionIds(subscriptions_));
    tabBarWithDragReorder();

    {
        const QSignalBlocker blocker(context_.subscriptionTabBar);
        // Rebuilding the bar emits tabMoved as tabs are removed and re-inserted; that is not the
        // user dragging anything, and treating it as a reorder would write the list back on every
        // refresh.
        rebuildingTabs_ = true;
        for (int index = context_.subscriptionTabBar->count() - 1; index >= 0; --index) {
            context_.subscriptionTabBar->removeTab(index);
        }

        QSet<QString> addedSubscriptionIds;
        for (const SubItem& item : subscriptions_) {
            if (!item.enabled) {
                continue;
            }

            const QString subscriptionId = item.id.trimmed();
            if (subscriptionId.isEmpty() || addedSubscriptionIds.contains(subscriptionId)) {
                continue;
            }

            addedSubscriptionIds.insert(subscriptionId);
            // Appended, not inserted at 0: the persisted subscription order is the tab order, and
            // rebuilding it backwards is what a saved order could never survive.
            const int index = context_.subscriptionTabBar->addTab(
                SubscriptionViewSupport::subscriptionDisplayName(item));
            context_.subscriptionTabBar->setTabData(index, QStringLiteral("sub:%1").arg(subscriptionId));
        }

        // Ungrouped is not a subscription and has no place in the order, so it stays pinned last.
        const int ungroupedIndex = context_.subscriptionTabBar->addTab(QObject::tr("Ungrouped"));
        context_.subscriptionTabBar->setTabData(ungroupedIndex, QStringLiteral("ungrouped"));

        int restoreIndex = -1;
        for (int index = 0; index < context_.subscriptionTabBar->count(); ++index) {
            if (context_.subscriptionTabBar->tabData(index).toString() == previousTabKey) {
                restoreIndex = index;
                break;
            }
        }

        if (context_.subscriptionTabBar->count() > 0) {
            context_.subscriptionTabBar->setCurrentIndex(restoreIndex >= 0 ? restoreIndex : 0);
        }
        rebuildingTabs_ = false;
    }
}

void SubscriptionViewController::applyFilter(const QString& filterText)
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    const QString pattern = filterText.trimmed();
    context_.serverFilterModel->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(pattern), QRegularExpression::CaseInsensitiveOption));
}

void SubscriptionViewController::applyCurrentTabFilter()
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    const QString tabKey = currentTabKey();
    if (tabKey == QStringLiteral("ungrouped")) {
        context_.serverFilterModel->setSubscriptionFilterMode(ServerFilterProxyModel::SubscriptionFilterMode::Ungrouped);
    } else {
        context_.serverFilterModel->setSubscriptionFilterMode(
            ServerFilterProxyModel::SubscriptionFilterMode::Subscription,
            tabKey.mid(QStringLiteral("sub:").size()));
    }
}

bool SubscriptionViewController::selectSubscriptionTab(const QString& selectionId)
{
    if (context_.subscriptionTabBar == nullptr) {
        return false;
    }

    const QString targetTabKey = SubscriptionViewSupport::tabKeyFromSelectionId(selectionId);
    for (int index = 0; index < context_.subscriptionTabBar->count(); ++index) {
        if (context_.subscriptionTabBar->tabData(index).toString() == targetTabKey) {
            context_.subscriptionTabBar->setCurrentIndex(index);
            return true;
        }
    }

    return false;
}

QString SubscriptionViewController::currentTabKey() const
{
    if (context_.subscriptionTabBar == nullptr || context_.subscriptionTabBar->currentIndex() < 0) {
        return QStringLiteral("ungrouped");
    }

    return context_.subscriptionTabBar->tabData(context_.subscriptionTabBar->currentIndex()).toString();
}

QString SubscriptionViewController::currentSubscriptionUrl() const
{
    const QString subscriptionId = SubscriptionViewSupport::subscriptionIdFromTabKey(currentTabKey());
    if (subscriptionId.isEmpty()) {
        return {};
    }

    for (const SubItem& item : subscriptions_) {
        if (item.id.trimmed() == subscriptionId) {
            return item.url.trimmed();
        }
    }

    return {};
}

QString SubscriptionViewController::persistedSelectionId() const
{
    return SubscriptionViewSupport::persistedSelectionIdFromTabKey(currentTabKey());
}

QString SubscriptionViewController::subscriptionIdAtTab(int index) const
{
    return context_.subscriptionTabBar == nullptr
        ? QString()
        : SubscriptionViewSupport::subscriptionIdFromTabKey(
            context_.subscriptionTabBar->tabData(index).toString());
}

QTabBar* SubscriptionViewController::tabBarWithDragReorder() const
{
    if (context_.subscriptionTabBar != nullptr) {
        // Movable so a drag persists the order; the bar is the only place the order is visible, so
        // it is the only place it can be edited.
        context_.subscriptionTabBar->setMovable(true);
    }
    return context_.subscriptionTabBar;
}

void SubscriptionViewController::invokeMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation)
{
    if (subscriptionId.isEmpty() || !canMove(subscriptionId, operation)) {
        return;
    }

    if (callbacks_.moveSubscription) {
        callbacks_.moveSubscription(subscriptionId, operation);
    }
}

bool SubscriptionViewController::canMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation) const
{
    // Disabled subscriptions keep their place in the persisted list but have no tab, so "can this
    // move" is a question the visible tabs answer: a subscription needs a neighbour after it (or
    // before it) to swap with, and Ungrouped is never one of those neighbours.
    QStringList movableIds;
    for (const SubItem& item : subscriptions_) {
        const QString id = item.id.trimmed();
        if (item.enabled && !id.isEmpty() && !movableIds.contains(id)) {
            movableIds.append(id);
        }
    }

    const int position = movableIds.indexOf(subscriptionId.trimmed());
    if (position < 0 || movableIds.size() < 2) {
        return false;
    }

    switch (operation) {
    case SubscriptionOrder::MoveOperation::Up:
    case SubscriptionOrder::MoveOperation::Top:
        return position > 0;
    case SubscriptionOrder::MoveOperation::Down:
    case SubscriptionOrder::MoveOperation::Bottom:
        return position < movableIds.size() - 1;
    }

    return false;
}

bool SubscriptionViewController::isUngroupedTabSelected() const
{
    return currentTabKey() == QStringLiteral("ungrouped");
}

void SubscriptionViewController::showTabContextMenu(const QPoint& position)
{
    if (context_.subscriptionTabBar == nullptr) {
        return;
    }

    const int tabIndex = context_.subscriptionTabBar->tabAt(position);
    if (tabIndex < 0) {
        return;
    }

    if (context_.subscriptionTabBar->currentIndex() != tabIndex) {
        context_.subscriptionTabBar->setCurrentIndex(tabIndex);
    }

    const QString tabKey = context_.subscriptionTabBar->tabData(tabIndex).toString();
    showContextMenu(tabKey, context_.subscriptionTabBar->mapToGlobal(position), context_.subscriptionTabBar);
}

void SubscriptionViewController::showContextMenuForTabKey(const QString& tabKey, const QPoint& globalPosition)
{
    if (context_.subscriptionTabBar == nullptr) {
        return;
    }

    for (int index = 0; index < context_.subscriptionTabBar->count(); ++index) {
        if (context_.subscriptionTabBar->tabData(index).toString() == tabKey) {
            if (context_.subscriptionTabBar->currentIndex() != index) {
                context_.subscriptionTabBar->setCurrentIndex(index);
            }
            showContextMenu(tabKey, globalPosition, context_.subscriptionTabBar);
            return;
        }
    }
}

QString SubscriptionViewController::draggableTabData(int index) const
{
    // Ungrouped is not a subscription, has no place in the order, and stays pinned last, so it is
    // not a drag source. Returning empty lets QTabBar/StartDragDistance refuse that tab alone
    // instead of disabling dragging for the whole bar.
    if (context_.subscriptionTabBar == nullptr
        || SubscriptionViewSupport::subscriptionIdFromTabKey(
               context_.subscriptionTabBar->tabData(index).toString()).isEmpty()) {
        return {};
    }

    return context_.subscriptionTabBar->tabData(index).toString();
}

void SubscriptionViewController::handleTabMoved(int from, int to)
{
    if (rebuildingTabs_ || context_.subscriptionTabBar == nullptr) {
        return;
    }

    const QString movedId = subscriptionIdAtTab(to);
    if (movedId.isEmpty() || from == to) {
        return;
    }

    // The drop position in the bar is the position in the subscription list, one for one: the bar
    // holds exactly the enabled subscriptions, in list order, with Ungrouped appended. So the new
    // rank is read off the bar rather than reconstructed by replaying step operations -- dragging
    // tab 3 to slot 0 is three "move up"s, and a step operation cannot express a drop.
    int targetRank = 0;
    for (int index = 0; index < to; ++index) {
        if (!SubscriptionViewSupport::subscriptionIdFromTabKey(
                 context_.subscriptionTabBar->tabData(index).toString()).isEmpty()) {
            ++targetRank;
        }
    }

    QList<SubItem> reordered = subscriptions_;
    const auto draggedPosition = std::find_if(
        reordered.begin(),
        reordered.end(),
        [&movedId](const SubItem& item) { return item.id.trimmed() == movedId; });
    if (draggedPosition == reordered.end()) {
        return;
    }

    // Remove-then-insert is exact here: with the dragged item out of the list, `targetRank` is the
    // slot it belongs in, whether it travelled up or down.
    const int currentRank = static_cast<int>(std::distance(reordered.begin(), draggedPosition));
    const SubItem draggedItem = *draggedPosition;
    reordered.removeAt(currentRank);
    reordered.insert(qBound(0, targetRank, reordered.size()), draggedItem);

    if (callbacks_.reorderSubscriptions) {
        callbacks_.reorderSubscriptions(reordered);
    }
}

void SubscriptionViewController::showContextMenu(const QString& tabKey, const QPoint& globalPosition, QWidget* parent)
{
    const QString subscriptionId = SubscriptionViewSupport::subscriptionIdFromTabKey(tabKey);
    QMenu menu(parent == nullptr ? context_.subscriptionTabBar : parent);

    if (!subscriptionId.isEmpty()) {
        auto* copySubscriptionUrlAction = menu.addAction(QObject::tr("Copy Subscription Url"));
        copySubscriptionUrlAction->setEnabled(!currentSubscriptionUrl().isEmpty());
        QObject::connect(copySubscriptionUrlAction, &QAction::triggered, &menu, [this]() {
            if (callbacks_.copyCurrentSubscriptionUrl) {
                callbacks_.copyCurrentSubscriptionUrl();
            }
        });

        auto* testServersAction = menu.addAction(QObject::tr("Test servers"));
        testServersAction->setEnabled(callbacks_.canStartBackgroundTask && callbacks_.canStartBackgroundTask());
        QObject::connect(testServersAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.testSubscriptionServers) {
                callbacks_.testSubscriptionServers(subscriptionId);
            }
        });

        menu.addSeparator();

        auto* updateAction = menu.addAction(QObject::tr("Update Subscription"));
        updateAction->setEnabled(callbacks_.canStartBackgroundTask && callbacks_.canStartBackgroundTask());
        QObject::connect(updateAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.updateCurrentSubscription) {
                callbacks_.updateCurrentSubscription(subscriptionId);
            }
        });

        auto* updateViaProxyAction = menu.addAction(QObject::tr("Update Subscription via Proxy"));
        updateViaProxyAction->setEnabled(callbacks_.canStartBackgroundTask && callbacks_.canStartBackgroundTask());
        QObject::connect(updateViaProxyAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.updateCurrentSubscriptionViaProxy) {
                callbacks_.updateCurrentSubscriptionViaProxy(subscriptionId);
            }
        });

        menu.addSeparator();

        // Order entries live next to the other per-subscription commands and are greyed out at the
        // edges, so pressing "Move Up" on the first tab says why it did nothing.
        auto addMoveAction = [this, &menu, subscriptionId](
                                 const QString& text,
                                 SubscriptionOrder::MoveOperation operation) {
            auto* action = menu.addAction(text);
            action->setEnabled(canMove(subscriptionId, operation));
            QObject::connect(action, &QAction::triggered, &menu, [this, subscriptionId, operation]() {
                invokeMove(subscriptionId, operation);
            });
        };
        addMoveAction(QObject::tr("Move Up"), SubscriptionOrder::MoveOperation::Up);
        addMoveAction(QObject::tr("Move Down"), SubscriptionOrder::MoveOperation::Down);
        addMoveAction(QObject::tr("Move To Top"), SubscriptionOrder::MoveOperation::Top);
        addMoveAction(QObject::tr("Move To Bottom"), SubscriptionOrder::MoveOperation::Bottom);

        menu.addSeparator();

        auto* hideAction = menu.addAction(QObject::tr("Hide"));
        QObject::connect(hideAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.hideSubscription) {
                callbacks_.hideSubscription(subscriptionId);
            }
        });

        auto* deleteAction = menu.addAction(QObject::tr("Delete"));
        QObject::connect(deleteAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (callbacks_.confirmAndDeleteSubscription) {
                callbacks_.confirmAndDeleteSubscription(subscriptionId);
            }
        });

        menu.addSeparator();
    }

    auto* manageAction = menu.addAction(QObject::tr("Subscription Management"));
    QObject::connect(manageAction, &QAction::triggered, &menu, [this]() {
        if (callbacks_.openSubscriptionManagement) {
            callbacks_.openSubscriptionManagement();
        }
    });

    menu.exec(globalPosition);
}
