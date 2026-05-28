#pragma once

#include <QIcon>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "app/RuntimeState.h"
#include "common/SystemProxyMode.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/VmessItem.h"
#include "ui/tray/TrayMenuSupport.h"

class QAction;
class MainWindow;
class QMenu;
class QSystemTrayIcon;

class TrayController final : public QObject {
    Q_OBJECT

public:
    explicit TrayController(MainWindow* mainWindow, QObject* parent = nullptr);
    ~TrayController() override;

    bool initialize();
    bool isAvailable() const;
    void setCurrentServerName(const QString& name);
    void setProxyUiState(ProxyUiState state);
    void setSystemProxyState(int mode, bool enabled);
    void setAutoRunEnabled(bool enabled);
    void setTunEnabled(bool enabled);
    void setRoutingSummary(const QString& value);
    void setServers(
        const QList<VmessItem>& servers,
        const QList<SubItem>& subscriptions,
        const QString& currentIndexId);
    void setRoutings(const QList<RoutingItem>& routings, const QString& currentRoutingId);
    void setBackgroundTaskRunning(bool running);
    void setBackgroundTaskDescription(const QString& description);
    void showMessage(const QString& title, const QString& message, bool error = false, int timeoutMs = 5000);

signals:
    void defaultServerRequested(const QString& indexId);
    void routingRequested(const QString& routingModeId);
    void autoRunToggled(bool enabled);
    void quitRequested();

public slots:
    void showMainWindow();
    void hideMainWindow();
    void toggleMainWindow();

private:
    void updateMenuText();
    void updateToolTip();
    void updateTrayIcon();

    QPointer<MainWindow> mainWindow_;
    QPointer<QSystemTrayIcon> trayIcon_;
    QPointer<QMenu> trayMenu_;
    QPointer<QAction> currentServerAction_;
    QPointer<QMenu> serversMenu_;
    QPointer<QMenu> routingsMenu_;
    QPointer<QAction> autoRunAction_;
    QPointer<QAction> quitAction_;
    QString currentServerName_;
    QString routingSummary_;
    QList<TrayServerEntry> servers_;
    QList<TrayRoutingEntry> routings_;
    QString currentServerId_;
    QString currentRoutingId_;
    ProxyUiState proxyUiState_ = ProxyUiState::Idle;
    bool backgroundTaskRunning_ = false;
    QString backgroundTaskDescription_;
    bool systemProxyApplied_ = false;
    bool autoRunEnabled_ = false;
    bool tunEnabled_ = false;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
    QIcon cachedDefaultIcon_;
    QIcon cachedActiveIcon_;
};
