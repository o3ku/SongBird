#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "common/SystemProxyMode.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/VmessItem.h"

class QAction;
class MainWindow;
class QMenu;
class QSystemTrayIcon;

class TrayController final : public QObject {
    Q_OBJECT

public:
    explicit TrayController(MainWindow* mainWindow, QObject* parent = nullptr);

    bool initialize();
    bool isAvailable() const;
    void setCurrentServerName(const QString& name);
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

    MainWindow* mainWindow_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QAction* currentServerAction_ = nullptr;
    QMenu* serversMenu_ = nullptr;
    QMenu* routingsMenu_ = nullptr;
    QAction* startCoreAction_ = nullptr;
    QAction* stopCoreAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    QString currentServerName_;
    QString routingSummary_;
    QString statisticsSummary_;
    QString trafficSummary_;
    QList<VmessItem> servers_;
    QList<RoutingItem> routings_;
    QString currentServerId_;
    int currentRoutingIndex_ = -1;
    int serverMenuLimit_ = 0;
    bool coreRunning_ = false;
    bool coreTransitionPending_ = false;
    bool backgroundTaskRunning_ = false;
    QString backgroundTaskDescription_;
    bool systemProxyApplied_ = false;
    bool autoRunEnabled_ = false;
    bool advancedRoutingEnabled_ = false;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
};
