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
    struct Snapshot {
        QString currentServerName;
        QString currentServerLocation;
        QString currentServerWarning;
        QString listenSummary;
        bool backgroundTaskRunning = false;
        QString backgroundTaskDescription;
        QString idleStatusText;
        QString updateAvailableText;
        bool updateAvailable = false;
    };

    StatusBarController(
        QWidget* owner,
        QStatusBar* statusBar,
        std::function<void()> settingsRequested,
        std::function<void()> currentServerRequested,
        std::function<void()> updateDownloadRequested);

    void refresh(
        const QString& currentServerName,
        const QString& currentServerLocation,
        const QString& currentServerWarning,
        const QString& listenSummary,
        bool backgroundTaskRunning,
        const QString& backgroundTaskDescription,
        const QString& idleStatusText,
        const QString& updateAvailableText,
        bool updateAvailable);
    const Snapshot& snapshot() const;

    void refreshBackgroundTaskStatusLabel(bool ownerVisible);
    void setCompactMode(bool compact);
    bool handleEvent(QObject* watched, QEvent* event);

private:
    void updateStatusIndicators();
    QString currentBackgroundTaskStatusText() const;
    void updateBackgroundTaskStatusInteraction();

    QWidget* owner_ = nullptr;
    class QLabel* currentServerStatusLabel_ = nullptr;
    class QLabel* routingStatusLabel_ = nullptr;
    class QLabel* backgroundTaskStatusLabel_ = nullptr;
    std::function<void()> settingsRequested_;
    std::function<void()> currentServerRequested_;
    std::function<void()> updateDownloadRequested_;
    Snapshot snapshot_;
    bool compactMode_ = false;
};
