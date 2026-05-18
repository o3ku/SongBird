#pragma once

#include <functional>

#include <QString>

#include "common/SystemProxyMode.h"
#include "domain/models/StatisticsSessionState.h"

class QEvent;
class QObject;
class QPoint;
class QStatusBar;
class QWidget;

class StatusBarController final {
public:
    enum class TransientStatusPriority {
        Important,
        Routine
    };

    struct Snapshot {
        QString currentServerName;
        QString currentServerLocation;
        QString currentServerWarning;
        QString listenSummary;
        QString currentCoreName;
        StatisticsSessionState statisticsState;
        bool systemProxyApplied = false;
        bool autoRunEnabled = false;
        bool coreProcessRunning = false;
        bool coreRunning = false;
        bool coreTransitionPending = false;
        bool backgroundTaskRunning = false;
        QString backgroundTaskDescription;
        SystemProxyMode systemProxyMode = SystemProxyMode::ForcedClear;
    };

    StatusBarController(
        QWidget* owner,
        QStatusBar* statusBar,
        std::function<void()> settingsRequested,
        std::function<void(const QPoint&)> systemProxyMenuRequested);

    void setSnapshot(const Snapshot& snapshot);
    void refresh(
        const QString& currentServerName,
        const QString& currentServerLocation,
        const QString& currentServerWarning,
        const QString& listenSummary,
        const QString& currentCoreName,
        const StatisticsSessionState& statisticsState,
        bool systemProxyApplied,
        bool autoRunEnabled,
        bool coreProcessRunning,
        bool coreRunning,
        bool coreTransitionPending,
        bool backgroundTaskRunning,
        const QString& backgroundTaskDescription,
        SystemProxyMode systemProxyMode);
    const Snapshot& snapshot() const;

    void showTransientStatus(const QString& message, int timeoutMs, TransientStatusPriority priority);
    void clearTransientStatus();
    void refreshTransientStatusLabel(bool ownerVisible);
    bool handleEvent(QObject* watched, QEvent* event);

private:
    void updateStatusIndicators();
    QString currentTransientStatusText() const;

    QWidget* owner_ = nullptr;
    class QLabel* currentServerStatusLabel_ = nullptr;
    class QLabel* routingStatusLabel_ = nullptr;
    class QLabel* coreStatusLabel_ = nullptr;
    class QLabel* proxyStatusLabel_ = nullptr;
    class QLabel* autoRunStatusLabel_ = nullptr;
    class QLabel* statisticsStatusLabel_ = nullptr;
    class QLabel* transientStatusLabel_ = nullptr;
    class QTimer* transientStatusTimer_ = nullptr;
    std::function<void()> settingsRequested_;
    std::function<void(const QPoint&)> systemProxyMenuRequested_;
    Snapshot snapshot_;
    QString transientStatusMessage_;
};
