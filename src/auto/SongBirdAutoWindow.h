#pragma once

#include <QList>
#include <QIcon>
#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "auto/AutoTypes.h"

class QComboBox;
class QCloseEvent;
class QEvent;
class QLabel;
class QMenu;
class QObject;
class QPushButton;
class QAction;
class QSystemTrayIcon;
class QTableWidget;
class QTimer;
class SongBirdAutoCoordinator;

class SongBirdAutoWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit SongBirdAutoWindow(SongBirdAutoCoordinator& coordinator, QWidget* parent = nullptr);
    void startProxy();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void buildTray();
    void connectSignals();
    void applyCompactStyle();
    void showMainWindowFromTray();
    void requestExit();
    void updateTrayMenu();
    void updateTrayToolTip();
    void showSubscriptionEditor();
    void showRoutingSettings();
    void showNodeTable();
    void showLogPanel();
    bool ensureTunAdminReadyForStart();
    void scheduleSubscriptionAutosave();
    void saveSubscriptionUrlsIfChanged();
    void setSubscriptionUrlsText(const QString& text);
    void setAutoSelectionStrategy(const QString& strategy);
    void startRunButtonAnimation();
    void stopRunButtonAnimation();
    void advanceRunButtonAnimation();
    void setCountrySummaries(const QList<AutoCountrySummary>& summaries);
    void setNodeEvaluations(const QList<AutoNodeEvaluation>& evaluations);
    void appendTaskSummary(const QString& message);
    bool isActivationPending() const;
    bool isProxyActive() const;
    void updateAppIcon();
    QList<AutoNodeEvaluation> visibleNodeEvaluations() const;
    void renderNodeEvaluations(QTableWidget* table);
    void updateNodesButtonText();
    void updateLogStatusText();
    void refreshLogStatusLabel();
    void updateRunButtonState();
    void appendLog(const QString& message);
    void setTunEnabled(bool enabled);
    void setBusy(bool busy);
    void setRunning(bool running);
    void setActiveServer(const QString& serverId, const QString& serverName, const QString& countryDisplay, const QString& location, qint64 latencyMs);
    void updateTunButtonState();

    SongBirdAutoCoordinator& coordinator_;
    QComboBox* countryCombo_ = nullptr;
    QComboBox* strategyCombo_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* nodesButton_ = nullptr;
    QPushButton* subscriptionsButton_ = nullptr;
    QPushButton* routingButton_ = nullptr;
    QPushButton* tunButton_ = nullptr;
    QLabel* logsStatusLabel_ = nullptr;
    QTimer* subscriptionSaveTimer_ = nullptr;
    QTimer* runButtonAnimationTimer_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu* trayMenu_ = nullptr;
    QMenu* trayCountriesMenu_ = nullptr;
    QAction* trayShowAction_ = nullptr;
    QAction* trayRunAction_ = nullptr;
    QAction* trayTunAction_ = nullptr;
    QAction* trayExitAction_ = nullptr;
    QIcon idleIcon_;
    QIcon activeIcon_;
    QList<AutoCountrySummary> countries_;
    QList<AutoNodeEvaluation> evaluations_;
    QStringList logLines_;
    QStringList taskSummaryLines_;
    QString logStatusText_;
    QString currentServerId_;
    QString currentServerName_;
    QString currentCountryDisplay_;
    QString currentLocation_;
    qint64 currentLatencyMs_ = -1;
    QString lastSavedSubscriptionText_;
    QString subscriptionDraftText_;
    bool updatingCombo_ = false;
    bool updatingStrategyCombo_ = false;
    bool updatingSubscriptionText_ = false;
    bool pendingSubscriptionSave_ = false;
    bool activationInProgress_ = false;
    bool busy_ = false;
    bool running_ = false;
    bool tunEnabled_ = false;
    bool exiting_ = false;
    bool trayCloseMessageShown_ = false;
    int runButtonAnimationFrame_ = 0;
};
