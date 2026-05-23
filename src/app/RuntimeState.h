#pragma once

#include <QObject>
#include <QString>

#include "common/SystemProxyMode.h"

struct RuntimeStateSnapshot {
    QString currentServerName;
    QString currentServerLocation;
    QString currentServerWarning;
    QString routingSummary;
    QString listenSummary;
    bool coreProcessRunning = false;
    bool coreRunning = false;
    bool coreTransitionPending = false;
    SystemProxyMode systemProxyMode = SystemProxyMode::ForcedClear;
    bool systemProxyApplied = false;
    bool autoRunEnabled = false;
    bool routingAdvancedEnabled = false;
};

class RuntimeState final : public QObject {
    Q_OBJECT

public:
    explicit RuntimeState(QObject* parent = nullptr);

    RuntimeStateSnapshot snapshot() const;
    void applySnapshot(const RuntimeStateSnapshot& snapshot);

signals:
    void currentServerChanged(const QString& name, const QString& location, const QString& warning);
    void coreStateChanged(bool processRunning, bool running, bool transitionPending);
    void systemProxyStateChanged(int mode, bool enabled);
    void autoRunChanged(bool enabled);
    void routingStatusChanged(const QString& routingText, const QString& listenText, bool advancedEnabled);

private:
    RuntimeStateSnapshot snapshot_;
};
