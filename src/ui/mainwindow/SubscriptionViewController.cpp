#include "ui/mainwindow/SubscriptionViewController.h"

#include <QAction>
#include <QFile>
#include <QHash>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QStyle>
#include <QSvgRenderer>
#include <QTabBar>

#include <algorithm>
#include <iterator>

#include "appcore/LocationCatalog.h"
#include "services/SubscriptionUpdateTime.h"
#include "ui/mainwindow/LocationTabBar.h"
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

void SubscriptionViewController::setServers(const QList<VmessItem>& servers)
{
    servers_ = servers;
}

void SubscriptionViewController::setGroupByLocation(bool enabled)
{
    groupByLocation_ = enabled;
}

bool SubscriptionViewController::groupByLocation() const
{
    return groupByLocation_;
}

void SubscriptionViewController::rebuildTabs(const QString& previousTabKey)
{
    if (context_.subscriptionTabBar == nullptr || context_.serverFilterModel == nullptr) {
        return;
    }
    if (skipTabsRebuildFromDragReorder_) {
        // The order change came from the bar's own drag; rebuilding now would run mid-drag (see
        // the member's comment). Consume the skip exactly once so a later refresh still rebuilds.
        skipTabsRebuildFromDragReorder_ = false;
        return;
    }

    context_.serverFilterModel->setKnownSubscriptionIds(
        SubscriptionViewSupport::enabledSubscriptionIds(subscriptions_));
    if (groupByLocation_) {
        rebuildLocationTabs(previousTabKey);
        return;
    }
    rebuildSubscriptionTabs(previousTabKey);
}

void SubscriptionViewController::rebuildSubscriptionTabs(const QString& previousTabKey)
{
    tabBarWithDragReorder();
    // The subscription view's own tab look: clear the location mode's compact variants. The
    // default icon size (style-provided) and the wider QSS padding are what the subscription
    // view has always rendered with; the property toggle re-runs the sheet's selectors.
    context_.subscriptionTabBar->setProperty("locationMode", QVariant());
    context_.subscriptionTabBar->style()->unpolish(context_.subscriptionTabBar);
    context_.subscriptionTabBar->style()->polish(context_.subscriptionTabBar);
    if (auto* locationBar = qobject_cast<LocationTabBar*>(context_.subscriptionTabBar)) {
        // Without this the subscription pass would keep painting flags over its own tabs.
        locationBar->clearLocationFlags();
    }

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

namespace {

// The flag cell's pixmap: the source is fit by aspect ratio into the 12x12 cell (a 3:2 flag
// renders 12x8 vertically centered; a square Lucide icon like ellipsis/star renders 8x8), so
// official verbatim icons need no per-file viewBox surgery and no source is stretched. Rendered
// through QSvgRenderer at the cell's pixel size so the vector stays crisp.
QPixmap squareFlagPixmap(const QString& locationId)
{
    QFile source(QStringLiteral(":/flags/%1.svg").arg(locationId.toLower()));
    if (!source.open(QIODevice::ReadOnly)) {
        return {};
    }
    QSvgRenderer renderer(source.readAll());
    if (!renderer.isValid()) {
        return {};
    }

    constexpr QSize kCell(12, 12);
    const QSizeF viewBox = renderer.defaultSize();
    if (viewBox.isEmpty()) {
        return {};
    }
    const qreal scale = qMin(kCell.width() / viewBox.width(), kCell.height() / viewBox.height());
    const QSize fitted(qRound(viewBox.width() * scale), qRound(viewBox.height() * scale));

    QPixmap square(kCell);
    square.fill(Qt::transparent);
    QPainter painter(&square);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(
        &painter,
        QRectF((kCell.width() - fitted.width()) / 2.0,
               (kCell.height() - fitted.height()) / 2.0,
               fitted.width(),
               fitted.height()));
    return square;
}

} // namespace

void SubscriptionViewController::rebuildLocationTabs(const QString& previousTabKey)
{
    if (context_.subscriptionTabBar == nullptr || context_.serverFilterModel == nullptr) {
        return;
    }
    if (skipTabsRebuildFromDragReorder_) {
        skipTabsRebuildFromDragReorder_ = false;
        return;
    }

    // The fixed order is the catalog's, so the bar is not movable in this mode: a drag would be
    // a reorder the catalog has no place to store and the next rebuild would silently undo.
    context_.subscriptionTabBar->setMovable(false);
    // The QSS half of the compact look: [locationMode="true"] swaps the tab's padding for the
    // tighter location variant (see AppTheme.qss.in). Property + repolish, because a style-sheet
    // selector only re-evaluates after the widget is told its style changed.
    context_.subscriptionTabBar->setProperty("locationMode", true);
    context_.subscriptionTabBar->style()->unpolish(context_.subscriptionTabBar);
    context_.subscriptionTabBar->style()->polish(context_.subscriptionTabBar);
    // The flag half lives in LocationTabBar (12x12 flag, 2px to the label, drawn as one token
    // with it); no QIcon is set, because the stylesheet's tab layout puts an 11-12px gap between
    // icon and text that nothing can move.
    if (auto* locationBar = qobject_cast<LocationTabBar*>(context_.subscriptionTabBar)) {
        locationBar->clearLocationFlags();
    }

    // Count nodes per location up front: empty items are dropped (the fixed list is a vocabulary,
    // not a promise that every item is shown), and the count is the "(N)" suffix on the tab.
    QHash<QString, int> countByLocationId;
    for (const VmessItem& server : servers_) {
        if (server.favorite) {
            ++countByLocationId[QStringLiteral("FAVORITE")];
        }
        ++countByLocationId[LocationCatalog::resolvedLocationId(server)];
    }

    {
        const QSignalBlocker blocker(context_.subscriptionTabBar);
        rebuildingTabs_ = true;
        for (int index = context_.subscriptionTabBar->count() - 1; index >= 0; --index) {
            context_.subscriptionTabBar->removeTab(index);
        }

        for (const LocationCatalog::Item& item : LocationCatalog::items()) {
            const int count = countByLocationId.value(item.id, 0);
            if (count <= 0) {
                continue; // An empty item shows nothing and cannot be selected; drop it.
            }

            // Compact on purpose: the tab carries a flag icon + the short code + the count, and
            // the full name ("United States", "Hong Kong / Taiwan") is the tooltip. A long label
            // here is what pushed the bar into its scroll arrows with a handful of items.
            const QString shortName = LocationCatalog::shortName(item.id);
            const QString label = shortName.isEmpty()
                ? (item.id == QStringLiteral("FAVORITE")
                    ? QObject::tr("Favorite (%1)").arg(count)
                    : QObject::tr("Other (%1)").arg(count))
                : QStringLiteral("%1 (%2)").arg(shortName).arg(count);
            const int index = context_.subscriptionTabBar->addTab(label);
            context_.subscriptionTabBar->setTabData(index, QStringLiteral("loc:%1").arg(item.id));
            if (auto* locationBar = qobject_cast<LocationTabBar*>(context_.subscriptionTabBar)) {
                locationBar->setLocationFlag(index, squareFlagPixmap(item.id));
            }
            // The full name, node count and merged-item explanation: what the tooltip can say
            // that three glyphs cannot.
            const QString fullName = LocationCatalog::displayName(item.id);
            context_.subscriptionTabBar->setTabToolTip(
                index,
                fullName.isEmpty() || fullName == shortName
                    ? label
                    : QStringLiteral("%1 — %2 (%3)").arg(shortName, fullName).arg(count));
        }

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

void SubscriptionViewController::applyLocationFilter()
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    const QString tabKey = currentTabKey();
    if (!tabKey.startsWith(QStringLiteral("loc:"))) {
        return;
    }

    const QString locationId = tabKey.mid(QStringLiteral("loc:").size());
    if (locationId == QStringLiteral("FAVORITE")) {
        context_.serverFilterModel->setLocationFilterMode(
            ServerFilterProxyModel::LocationFilterMode::Favorite);
    } else {
        context_.serverFilterModel->setLocationFilterMode(
            ServerFilterProxyModel::LocationFilterMode::Location,
            locationId);
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
            // Not a subscription. In subscription view that is Ungrouped (no refresh time of its
            // own, no tooltip); in location view the tab is a location item whose tooltip was
            // set by the rebuild (full name + count) and must survive this pass -- a refresh-time
            // tooltip pass has no business clearing it.
            if (!groupByLocation_) {
                context_.subscriptionTabBar->setTabToolTip(index, QString());
            }
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

    if (groupByLocation_) {
        applyLocationFilter();
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
    // Location tabs are their own selection space and are persisted separately
    // (mainSelectedLocationId, captured by the window). From this method's point of view a
    // location tab is "no subscription selected", the same answer the ungrouped tab gives.
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
    if (rebuildingTabs_ || context_.subscriptionTabBar == nullptr || groupByLocation_) {
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
        // Empty means the move the bar reports is Ungrouped's own displacement: a subscription
        // was dragged across the pinned tab and QTabBar expressed the swap from Ungrouped's
        // side. Ignoring it (the old behaviour) left Ungrouped parked mid-bar with nothing
        // staged -- the "it can still be dragged" bug.
        handleUngroupedDisplaced(from, to);
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

    // Record, do not commit. tabMoved() fires while the drag is still going (every time the
    // dragged tab slides past a neighbour), and the commit is a config write plus the full window
    // and tray refresh that follows it -- running that inside the bar's mouse handling for every
    // crossed neighbour is what made dragging laggy. subscriptions_ is updated here so the next
    // swap of the same drag composes on top of this one; commitDraggedTabOrder() persists the
    // accumulated order once, when the drag ends.
    subscriptions_ = reordered;
    dragCommitPending_ = true;
}

void SubscriptionViewController::commitDraggedTabOrder()
{
    if (!dragCommitPending_) {
        return; // Not every release on the bar ends a reorder; an ordinary click arrives here too.
    }
    dragCommitPending_ = false;

    // Before the write-back: a drop past Ungrouped leaves it sitting mid-bar (QTabBar places
    // the dropped tab after it), and the pinned-tab promise is "last, always". The bar's own
    // drag of Ungrouped cannot happen -- LocationTabBar pins the last tab against drags -- so
    // this only has to undo what another tab's drop did. The moveTab() inside may re-enter
    // handleUngroupedDisplaced() and stage again; the pending flag is re-cleared afterwards so
    // one drag still commits exactly once.
    restoreUngroupedTabAsLast();
    dragCommitPending_ = false;

    if (callbacks_.reorderSubscriptions) {
        // The bar already shows this order, and rebuilding it while QTabBar is still finishing the
        // drag's release -- tearing its tabs down under the drag state -- is what asserts in
        // layoutTab. The one refresh this reorder triggers is skipped; the next unrelated refresh
        // rebuilds.
        skipTabsRebuildFromDragReorder_ = true;
        callbacks_.reorderSubscriptions(subscriptions_);
        skipTabsRebuildFromDragReorder_ = false;
    }
}

void SubscriptionViewController::handleUngroupedDisplaced(int from, int to)
{
    Q_UNUSED(from)
    Q_UNUSED(to)

    if (context_.subscriptionTabBar == nullptr) {
        return;
    }

    // The bar's order minus Ungrouped is the order the user is asking for: every tab but the
    // pinned one is a subscription, in exactly the sequence the drag left them in.
    QStringList orderedIds;
    const int tabCount = context_.subscriptionTabBar->count();
    for (int index = 0; index < tabCount; ++index) {
        const QString id = subscriptionIdAtTab(index);
        if (!id.isEmpty()) {
            orderedIds.append(id);
        }
    }

    // Staged the same way handleTabMoved() stages a plain swap: subscriptions named by the bar
    // in bar order, then the ones no tab shows (a hidden subscription) in their existing
    // relative order.
    QList<SubItem> reordered;
    reordered.reserve(subscriptions_.size());
    QSet<QString> seenIds;
    for (const QString& id : orderedIds) {
        const auto position = std::find_if(
            subscriptions_.cbegin(),
            subscriptions_.cend(),
            [&id](const SubItem& item) { return item.id.trimmed() == id; });
        if (position != subscriptions_.cend() && !seenIds.contains(id)) {
            seenIds.insert(id);
            reordered.append(*position);
        }
    }
    for (const SubItem& item : subscriptions_) {
        if (!seenIds.contains(item.id.trimmed())) {
            reordered.append(item);
        }
    }
    subscriptions_ = reordered;
    dragCommitPending_ = true;

    // Snap the pin back now, inside the same drag: the dragged subscription runs into the last
    // slot and bounces back off it, so Ungrouped never visibly leaves the end. The moveTab()
    // below re-enters handleTabMoved() for this pair; that pass derives the same order (nothing
    // changed but Ungrouped's own slot), stages it again harmlessly, and finds Ungrouped already
    // last, so the recursion always terminates here.
    restoreUngroupedTabAsLast();
}

void SubscriptionViewController::restoreUngroupedTabAsLast()
{
    if (context_.subscriptionTabBar == nullptr) {
        return;
    }

    const int tabCount = context_.subscriptionTabBar->count();
    for (int index = 0; index < tabCount - 1; ++index) {
        if (context_.subscriptionTabBar->tabData(index).toString() == QStringLiteral("ungrouped")) {
            // The re-entrant tabMoved() goes through handleUngroupedDisplaced(), which derives
            // the same staged order (only Ungrouped's own slot changed) and finds nothing left
            // to snap -- see its comment for why that always terminates.
            context_.subscriptionTabBar->moveTab(index, tabCount - 1);
            return;
        }
    }
}

void SubscriptionViewController::showContextMenu(const QString& tabKey, const QPoint& globalPosition, QWidget* parent)
{
    const QString subscriptionId = SubscriptionViewSupport::subscriptionIdFromTabKey(tabKey);
    QMenu menu(parent == nullptr ? context_.subscriptionTabBar : parent);

    // A location item has no subscription commands: no url to copy, nothing to update, no order
    // to change (the tabData prefix is "loc:", which subscriptionIdFromTabKey answers with an
    // empty id, so this whole block is skipped for it). The management entry below stays, because
    // that is where subscriptions are edited from either view.
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
        menu.exec(globalPosition);
        return;
    }

    // Location items carry no menu of their own: the grouping mode's one preference (auto
    // switch) belongs to the toggle button, not to any one group, and subscriptions are edited
    // from the subscription view. An empty group menu would only be a dead click.
    if (groupByLocation_) {
        return;
    }

    auto* manageAction = menu.addAction(QObject::tr("Subscription Management"));
    QObject::connect(manageAction, &QAction::triggered, &menu, [this]() {
        if (callbacks_.openSubscriptionManagement) {
            callbacks_.openSubscriptionManagement();
        }
    });

    menu.exec(globalPosition);
}
