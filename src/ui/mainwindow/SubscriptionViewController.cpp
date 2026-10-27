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

#include "services/SubscriptionUpdateTime.h"
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

void SubscriptionViewController::applyTabToolTips(
    qint64 nowMs,
    std::function<QString(const QString&)> networkErrorFor)
{
    if (context_.subscriptionTabBar == nullptr) {
        return;
    }

    for (int index = 0; index < context_.subscriptionTabBar->count(); ++index) {
        const QString subscriptionId = subscriptionIdAtTab(index);
        if (subscriptionId.isEmpty()) {
            // Ungrouped is not a subscription and has no refresh time of its own.
            context_.subscriptionTabBar->setTabToolTip(index, QString());
            continue;
        }

        const auto stored = std::find_if(
            subscriptions_.cbegin(),
            subscriptions_.cend(),
            [&subscriptionId](const SubItem& item) { return item.id.trimmed() == subscriptionId; });
        if (stored == subscriptions_.cend()) {
            context_.subscriptionTabBar->setTabToolTip(index, QString());
            continue;
        }

        const QString networkError = networkErrorFor ? networkErrorFor(subscriptionId) : QString();
        context_.subscriptionTabBar->setTabToolTip(
            index,
            SubscriptionUpdateTime::tooltipText(
                SubscriptionViewSupport::subscriptionDisplayName(*stored),
                stored->lastUpdatedAtMs,
                nowMs,
                networkError));
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

QStringList SubscriptionViewController::movableSubscriptionIds() const
{
    // The persisted order is this list, hidden subscriptions included. A move is a position in that
    // list, so every id in it is movable -- including one with no tab, which is exactly the case the
    // tab bar and its context menu cannot reach on their own. Ungrouped is not in the list and so is
    // never a neighbour. Duplicates are skipped for the reason the rest of the reorder code skips
    // them: the second occurrence of an id is not a second position.
    QStringList movableIds;
    for (const SubItem& item : subscriptions_) {
        const QString id = item.id.trimmed();
        if (!id.isEmpty() && !movableIds.contains(id)) {
            movableIds.append(id);
        }
    }
    return movableIds;
}

bool SubscriptionViewController::canMove(const QString& subscriptionId, SubscriptionOrder::MoveOperation operation) const
{
    const QStringList movableIds = movableSubscriptionIds();
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

void SubscriptionViewController::showSubscriptionMoveMenu(const QPoint& globalPosition, QWidget* parent)
{
    QMenu menu(parent);
    menu.setObjectName(QStringLiteral("subscriptionMoveMenu"));

    // The current tab first, so the common case ("move what I am looking at") is one click away,
    // then the hidden ones by name. A hidden subscription's remarks is what the settings page calls
    // it, so the entry reads the way the user last saw it.
    const QString currentId = SubscriptionViewSupport::subscriptionIdFromTabKey(currentTabKey());
    if (!currentId.isEmpty()) {
        populateSubscriptionMoveMenu(&menu, currentId);
    }
    for (const SubItem& item : subscriptions_) {
        const QString id = item.id.trimmed();
        if (item.enabled || id.isEmpty() || id == currentId) {
            continue;
        }
        populateSubscriptionMoveMenu(&menu, id);
    }

    if (menu.isEmpty()) {
        menu.addAction(QObject::tr("Nothing to reorder"))->setEnabled(false);
    }

    menu.exec(globalPosition);
}

void SubscriptionViewController::populateSubscriptionMoveMenu(QMenu* menu, const QString& subscriptionId)
{
    if (menu == nullptr || subscriptionId.trimmed().isEmpty()) {
        return;
    }

    const auto removed = std::find_if(
        subscriptions_.begin(),
        subscriptions_.end(),
        [&subscriptionId](const SubItem& item) { return item.id.trimmed() == subscriptionId.trimmed(); });
    if (removed == subscriptions_.end()) {
        return;
    }

    const SubItem& item = *removed;
    QString label = item.remarks.trimmed();
    if (label.isEmpty()) {
        label = subscriptionId.trimmed();
    }
    if (!item.enabled) {
        label = QObject::tr("%1 (hidden)").arg(label);
    }

    if (!menu->isEmpty()) {
        menu->addSeparator();
    }
    // A submenu, not four more entries: this menu lists every subscription the tab bar cannot show,
    // and four top-level actions per subscription would bury the list.
    QMenu* subMenu = menu->addMenu(label);
    subMenu->setObjectName(QStringLiteral("moveSubscriptionEntry_%1").arg(subscriptionId.trimmed()));

    const struct {
        QString text;
        SubscriptionOrder::MoveOperation operation;
    } entries[] = {
        {QObject::tr("Move Up"), SubscriptionOrder::MoveOperation::Up},
        {QObject::tr("Move Down"), SubscriptionOrder::MoveOperation::Down},
        {QObject::tr("Move To Top"), SubscriptionOrder::MoveOperation::Top},
        {QObject::tr("Move To Bottom"), SubscriptionOrder::MoveOperation::Bottom},
    };

    for (const auto& entry : entries) {
        // Greyed out at the edges like the tab context menu, so pressing one on a subscription that
        // is already last says why it did nothing instead of doing nothing silently.
        QAction* action = subMenu->addAction(entry.text);
        action->setEnabled(canMove(subscriptionId, entry.operation));
        const QString id = subscriptionId;
        QObject::connect(action, &QAction::triggered, subMenu, [this, id, operation = entry.operation]() {
            invokeMove(id, operation);
        });
    }
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

void SubscriptionViewController::handleTabMoved(int from, int to)
{
    if (rebuildingTabs_ || context_.subscriptionTabBar == nullptr) {
        return;
    }

    if (from == to || to < 0) {
        return;
    }

    // tabMoved() arrives after the bar has already performed the move, so `to` addresses the
    // dragged tab and `from` addresses whatever now sits where it left from. The dragged tab is
    // therefore read at `to`.
    const QString movedId = subscriptionIdAtTab(to);
    if (movedId.isEmpty()) {
        return;
    }

    QList<SubItem> reordered = subscriptions_;
    const auto draggedPosition = std::find_if(
        reordered.begin(),
        reordered.end(),
        [&movedId](const SubItem& item) { return item.id.trimmed() == movedId; });
    if (draggedPosition == reordered.end()) {
        return;
    }

    const int currentRank = static_cast<int>(std::distance(reordered.begin(), draggedPosition));
    const SubItem draggedItem = *draggedPosition;
    reordered.removeAt(currentRank);

    // The new rank is read off the bar: `to` counts the tabs the moved one now sits after, and
    // those tabs are the list's own order minus the hidden subscriptions -- one for one. Counting
    // the bar instead of replaying step operations is what makes a drop an endpoint rather than
    // "up three times".
    //
    // Ungrouped is pinned last and is not part of the order, so a drop onto the final tab means
    // "after the last subscription". It is not read as a rank, because Ungrouped has none.
    int targetRank = 0;
    const int lastTab = context_.subscriptionTabBar->count() - 1;
    for (int index = 0; index < to && index < lastTab; ++index) {
        const QString anchorId = subscriptionIdAtTab(index);
        if (anchorId.isEmpty()) {
            continue;
        }
        const auto anchorPosition = std::find_if(
            reordered.begin(),
            reordered.end(),
            [&anchorId](const SubItem& item) { return item.id.trimmed() == anchorId; });
        if (anchorPosition != reordered.end()) {
            targetRank = static_cast<int>(std::distance(reordered.begin(), anchorPosition)) + 1;
        }
    }

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
        // edges, so pressing "Move Up" on the first tab says why it did nothing. The state is read
        // here, while the menu is being built, rather than kept from an earlier selection: the
        // current tab can change without anything telling this controller to recompute.
        auto addMoveAction = [this, &menu, subscriptionId](
                                 const QString& text,
                                 const QString& objectName,
                                 SubscriptionOrder::MoveOperation operation) {
            auto* action = menu.addAction(text);
            action->setObjectName(objectName);
            action->setEnabled(canMove(subscriptionId, operation));
            QObject::connect(action, &QAction::triggered, &menu, [this, subscriptionId, operation]() {
                invokeMove(subscriptionId, operation);
            });
        };
        addMoveAction(QObject::tr("Move Up"), QStringLiteral("moveSubscriptionTabUpAction"),
                      SubscriptionOrder::MoveOperation::Up);
        addMoveAction(QObject::tr("Move Down"), QStringLiteral("moveSubscriptionTabDownAction"),
                      SubscriptionOrder::MoveOperation::Down);
        addMoveAction(QObject::tr("Move To Top"), QStringLiteral("moveSubscriptionTabTopAction"),
                      SubscriptionOrder::MoveOperation::Top);
        addMoveAction(QObject::tr("Move To Bottom"), QStringLiteral("moveSubscriptionTabBottomAction"),
                      SubscriptionOrder::MoveOperation::Bottom);

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
