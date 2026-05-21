#pragma once

#include <functional>

#include <QString>

#include "common/SystemProxyMode.h"

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
        bool backgroundTaskRunning = false;
        QString backgroundTaskDescription;
    };

    StatusBarController(
        QWidget* owner,
        QStatusBar* statusBar,
        std::function<void()> settingsRequested,
        std::function<void()> currentServerRequested);

    void setSnapshot(const Snapshot& snapshot);
    void refresh(
        const QString& currentServerName,
        const QString& currentServerLocation,
        const QString& currentServerWarning,
        const QString& listenSummary,
        bool backgroundTaskRunning,
        const QString& backgroundTaskDescription);
    const Snapshot& snapshot() const;

    void showTransientStatus(const QString& message, int timeoutMs, TransientStatusPriority priority);
    void clearTransientStatus();
    void refreshTransientStatusLabel(bool ownerVisible);
    bool handleEvent(QObject* watched, QEvent* event);

private:
    void updateStatusIndicators();
    QString currentTransientStatusText() const;
    bool shouldSuppressRoutineStatus() const;

    QWidget* owner_ = nullptr;
    class QLabel* currentServerStatusLabel_ = nullptr;
    class QLabel* routingStatusLabel_ = nullptr;
    class QLabel* transientStatusLabel_ = nullptr;
    class QTimer* transientStatusTimer_ = nullptr;
    std::function<void()> settingsRequested_;
    std::function<void()> currentServerRequested_;
    Snapshot snapshot_;
    QString transientStatusMessage_;
};
