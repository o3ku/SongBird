#pragma once

#include <QList>
#include <QPoint>
#include <QString>
#include <QWidget>

#include <functional>

#include "domain/models/SubItem.h"

class QAbstractItemModel;
class QHBoxLayout;
class QLineEdit;
class QSplitter;
class QTabBar;
class QToolButton;
class QVBoxLayout;
class ServerFilterProxyModel;
class ServerTableView;
class SharePanelWidget;
class LogPanelWidget;
class ServerTableEmptyStateWidget;

class ServerWorkspaceWidget final : public QWidget {
    Q_OBJECT

public:
    ServerWorkspaceWidget(
        ServerFilterProxyModel* serverFilterModel,
        bool qrPreviewVisible,
        const QList<int>& defaultColumnWidths,
        QWidget* parent = nullptr);

    ServerTableView* serverView() const;
    QTabBar* subscriptionTabBar() const;
    // The checkable toggle to the left of the subscription tab bar. Checked = the tab bar shows
    // location items instead of subscriptions. Exposed for signal wiring in MainWindow.
    QToolButton* groupByLocationButton() const;
    QLineEdit* serverFilterEdit() const;
    SharePanelWidget* sharePanel() const;
    LogPanelWidget* logPanel() const;
    QSplitter* topSplitter() const;
    QSplitter* rootSplitter() const;

    // The overlay shown in place of the table when the visible server list is
    // empty. Exposed so the empty-state actions can be wired to the window.
    ServerTableEmptyStateWidget* emptyState() const;

    // What the empty-state panel should say, and which tab's subscription it is
    // talking about. Pushed in from the window because the panel cannot work either
    // out from the proxy model: the whole point of the withheld state is that the
    // nodes are in the source model but not in the visible row set.
    void setEmptyStateContext(
        const QList<SubItem>& subscriptions,
        const QString& listedSubscriptionId);

    // The window's answer to "may a background task start right now". The refresh the
    // empty-state panel offers is one, so the button has to follow it -- and the answer
    // changes on paths that touch no model at all (a proxy activation, the startup
    // checklist), so it is pushed in rather than only read when the panel is rebuilt.
    // The location mode's chrome: the server table's top edge turns accent green (the one
    // persistent highlight -- the bar keeps the subscription ground), the tab bar gets the
    // compact padding, and the toggle's icon follows the checked state (white glyph on the
    // accent ground when checked, accent glyph on the transparent ground when not). Applied
    // here rather than in a toggled handler because the restore path sets checked with signals
    // blocked (a signal round-trip would rebuild the tabs mid-restore) -- the icon would then
    // stay at its pre-restore face, an accent glyph on an accent ground, invisible. Called from
    // the window on the toggle and on state restore, the two places that change the mode.
    void setGroupByLocationMode(bool enabled);

    // The window's answer to "may a background task start right now". The refresh the
    // empty-state panel offers is one, so the button has to follow it -- and the answer
    // changes on paths that touch no model at all (a proxy activation, the startup
    // checklist), so it is pushed in rather than only read when the panel is rebuilt.
    void setEmptyStateCanStartBackgroundTask(std::function<bool()> predicate);

    // Why the last import produced nothing, or an empty string when none failed.
    // The panel is what the table is replaced by, so the reason has to live there
    // rather than only in the log; see ServerTableEmptyStateWidget::setImportFailure().
    void setImportFailure(const QString& reason);
    QString importFailure() const;

    void setCompactMode(bool compact);
    bool compactMode() const;
    void syncCompactSections();
    void setCompactSelectionHandler(std::function<void(const QString&)> handler);
    void setCompactContextMenuHandler(std::function<void(const QString&, const QPoint&)> handler);
    // The grouping toggle's own context menu (the auto-switch action lives here, not on the
    // location items: the preference is about the mode, not about any one group). The window
    // owns the preference and its persistence, so both halves are injected.
    void setGroupByLocationContextMenu(std::function<bool()> autoSwitchChecked,
                                        std::function<void(bool)> autoSwitchToggled);

private:
    void rebuildCompactSections();
    void moveServerFilterToDesktop();
    void moveServerFilterToCompact();
    void moveServerViewToDesktop();
    void moveServerViewToCompact();
    // Shows the empty-state overlay when the proxy model has no visible rows,
    // hides it otherwise, and picks the wording from ServerEmptyStatePolicy.
    void syncEmptyStateVisibility();
    // Nodes in the source model that belong to the tab the panel is describing.
    int countSourceNodesForSubscription(const QAbstractItemModel* sourceModel) const;

    ServerTableView* serverView_ = nullptr;
    ServerTableEmptyStateWidget* emptyState_ = nullptr;
    QSplitter* topSplitter_ = nullptr;
    QSplitter* rootSplitter_ = nullptr;
    QTabBar* subscriptionTabBar_ = nullptr;
    QToolButton* groupByLocationButton_ = nullptr;
    QLineEdit* serverFilterEdit_ = nullptr;
    SharePanelWidget* sharePanel_ = nullptr;
    LogPanelWidget* logPanel_ = nullptr;
    QWidget* subscriptionTabBarContainer_ = nullptr;
    QVBoxLayout* compactPanelLayout_ = nullptr;
    QWidget* compactPanel_ = nullptr;
    QWidget* compactSectionsWidget_ = nullptr;
    QVBoxLayout* compactSectionsLayout_ = nullptr;
    QWidget* serverPanel_ = nullptr;
    QVBoxLayout* serverPanelLayout_ = nullptr;
    QWidget* serverHeaderRow_ = nullptr;
    QHBoxLayout* subscriptionTabBarLayout_ = nullptr;
    QList<SubItem> emptyStateSubscriptions_;
    QString emptyStateListedSubscriptionId_;
    std::function<bool()> emptyStateCanStartBackgroundTask_;
    bool compactMode_ = false;
    std::function<void(const QString&)> compactSelectionHandler_;
    std::function<void(const QString&, const QPoint&)> compactContextMenuHandler_;
    std::function<bool()> groupMenuAutoSwitchChecked_;
    std::function<void(bool)> groupMenuAutoSwitchToggled_;
};
