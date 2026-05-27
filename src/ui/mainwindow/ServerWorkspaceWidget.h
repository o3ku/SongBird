#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class QLineEdit;
class QSplitter;
class QTabBar;
class QVBoxLayout;
class ServerFilterProxyModel;
class ServerTableView;
class SharePanelWidget;
class LogPanelWidget;

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
    QLineEdit* serverFilterEdit() const;
    SharePanelWidget* sharePanel() const;
    LogPanelWidget* logPanel() const;
    QSplitter* topSplitter() const;
    QSplitter* rootSplitter() const;

    void setCompactMode(bool compact);
    bool compactMode() const;
    void syncCompactSections();
    void setCompactSelectionHandler(std::function<void(const QString&)> handler);
    void setCompactContextMenuHandler(std::function<void(const QString&, const QPoint&)> handler);

private:
    void rebuildCompactSections();
    void moveServerFilterToDesktop();
    void moveServerFilterToCompact();
    void moveServerViewToDesktop();
    void moveServerViewToCompact();

    ServerTableView* serverView_ = nullptr;
    QSplitter* topSplitter_ = nullptr;
    QSplitter* rootSplitter_ = nullptr;
    QTabBar* subscriptionTabBar_ = nullptr;
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
    class QHBoxLayout* subscriptionTabBarLayout_ = nullptr;
    bool compactMode_ = false;
    std::function<void(const QString&)> compactSelectionHandler_;
    std::function<void(const QString&, const QPoint&)> compactContextMenuHandler_;
};
