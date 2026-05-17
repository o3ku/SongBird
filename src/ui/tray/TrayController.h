#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "common/SystemProxyMode.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/VmessItem.h"

class QAction;
class MainWindow;
class QMenu;
class QSystemTrayIcon;

struct TrayServerEntry {
    QString indexId;
    QString displayName;
};

struct TrayRoutingEntry {
    QString displayName;
    QString customIconPath;
};

class TrayController final : public QObject {
    Q_OBJECT

public:
    explicit TrayController(MainWindow* mainWindow, QObject* parent = nullptr);
    ~TrayController() override;

    bool initialize();
    bool isAvailable() const;
    void setCurrentServerName(const QString& name);
    void setCoreProcessRunning(bool running);
    void setCoreRunning(bool enabled, bool pending = false);
    void setSystemProxyState(int mode, bool enabled);
    void setAutoRunEnabled(bool enabled);
    void setRoutingSummary(const QString& value, bool advancedEnabled);
    void setStatisticsSummary(const QString& value);
    void setTrafficSummary(const QString& value);
    void setServers(const QList<VmessItem>& servers, const QString& currentIndexId, int limit);
    void setRoutings(const QList<RoutingItem>& routings, int currentRoutingIndex, bool advancedEnabled);
    void setBackgroundTaskRunning(bool running);
    void setBackgroundTaskDescription(const QString& description);
    void showMessage(const QString& title, const QString& message, bool error = false, int timeoutMs = 5000);

signals:
    void startCoreRequested();
    void stopCoreRequested();
    void defaultServerRequested(const QString& indexId);
    void routingRequested(int index);
    void quitRequested();

public slots:
    void showMainWindow();
    void hideMainWindow();
    void toggleMainWindow();

private:
    void updateMenuText();
    void updateToolTip();
    void updateTrayIcon();
    void rebuildServerMenu();
    void rebuildRoutingMenu();
    static QString describeServer(const VmessItem& item);
    static QString describeRouting(const RoutingItem& item, int index);

    QPointer<MainWindow> mainWindow_;
    QPointer<QSystemTrayIcon> trayIcon_;
    QPointer<QMenu> trayMenu_;
    QPointer<QAction> currentServerAction_;
    QPointer<QMenu> serversMenu_;
    QPointer<QMenu> routingsMenu_;
    QPointer<QAction> startCoreAction_;
    QPointer<QAction> stopCoreAction_;
    QPointer<QAction> quitAction_;
    QString currentServerName_;
    QString routingSummary_;
    QString statisticsSummary_;
    QString trafficSummary_;
    QList<TrayServerEntry> servers_;
    QList<TrayRoutingEntry> routings_;
    QString currentServerId_;
    int currentRoutingIndex_ = -1;
    int serverMenuLimit_ = 0;
    bool coreProcessRunning_ = false;
    bool coreRunning_ = false;
    bool coreTransitionPending_ = false;
    bool backgroundTaskRunning_ = false;
    QString backgroundTaskDescription_;
    bool systemProxyApplied_ = false;
    bool autoRunEnabled_ = false;
    bool advancedRoutingEnabled_ = false;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
};
