#pragma once

#include <QObject>
#include <QString>

#include "common/SystemProxyMode.h"

enum class ProxyUiState {
    Idle,
    Transitioning,
    Active,
    Inconsistent
};

struct RuntimeStateSnapshot {
    QString currentServerName;
    QString currentServerLocation;
    QString currentServerWarning;
    // Set when the session could not undo its registry write (display only, like the
    // OutboundLocationProbe warning): non-empty means the user must act or wait for the
    // next launch's recovery. Cleared when a new activation succeeds.
    QString systemProxyUndoWarning;
    QString routingSummary;
    QString listenSummary;
    ProxyUiState proxyUiState = ProxyUiState::Idle;
    SystemProxyMode systemProxyMode = SystemProxyMode::ForcedClear;
    bool systemProxyApplied = false;
    bool autoRunEnabled = false;
};

class RuntimeState final : public QObject {
    Q_OBJECT

public:
    explicit RuntimeState(QObject* parent = nullptr);

    RuntimeStateSnapshot snapshot() const;
    void applySnapshot(const RuntimeStateSnapshot& snapshot);

signals:
    void snapshotApplied(const RuntimeStateSnapshot& snapshot);
    void currentServerSegmentChanged(const QString& name, const QString& location, const QString& warning);
    void proxyUiStateChanged(ProxyUiState state);
    void systemProxyStateChanged(int mode, bool enabled);
    void autoRunChanged(bool enabled);
    void routingStatusChanged(const QString& routingText, const QString& listenText);

private:
    RuntimeStateSnapshot snapshot_;
};
