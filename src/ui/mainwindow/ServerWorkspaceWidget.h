#pragma once

#include <QList>
#include <QWidget>

class QLineEdit;
class QSplitter;
class QTabBar;
class ServerFilterProxyModel;
class ServerTableView;
class SharePanelWidget;
class LogPanelWidget;

class ServerWorkspaceWidget final : public QWidget {
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

    void setSubscriptionUpdateRunning(bool running);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ServerTableView* serverView_ = nullptr;
    QWidget* loadingOverlay_ = nullptr;
    QSplitter* topSplitter_ = nullptr;
    QSplitter* rootSplitter_ = nullptr;
    QTabBar* subscriptionTabBar_ = nullptr;
    QLineEdit* serverFilterEdit_ = nullptr;
    SharePanelWidget* sharePanel_ = nullptr;
    LogPanelWidget* logPanel_ = nullptr;
};
