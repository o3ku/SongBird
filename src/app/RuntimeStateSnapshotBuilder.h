#pragma once

#include <functional>
#include <optional>

#include <QString>

#include "domain/models/RuntimeState.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"

class RuntimeStateSnapshotBuilder final {
public:
    struct Inputs {
        const Config* config = nullptr;
        QString currentServerName;
        QString currentServerLocation;
        QString currentServerWarning;
        QString systemProxyUndoWarning;
        QString proxyPhaseName;
        SystemProxyMode systemProxyMode = SystemProxyMode::ForcedClear;
        bool systemProxyApplied = false;
        bool autoRunEnabled = false;
        bool proxyTransitioning = false;
        bool coreRunning = false;
        bool coreReady = false;
        // Whether the outbound was confirmed reachable. This is what decides Active, and it is
        // deliberately *not* "the location string is non-empty" any more: a geo lookup that failed
        // while the tunnel works is a missing flag, not a missing connection. The rule itself
        // lives in appcore/ProxyUiStateDecision.h so it can be tested.
        bool outboundReachable = false;
    };

    struct Callbacks {
        std::function<void(const QString&)> appendLog;
    };

    explicit RuntimeStateSnapshotBuilder(Callbacks callbacks = {});

    RuntimeStateSnapshot buildSnapshot(const Inputs& inputs);

private:
    QString buildRoutingSummaryText(const Config& config) const;
    QString buildListenSummaryText(const Config& config) const;
    ProxyUiState computeProxyUiState(const Inputs& inputs) const;
    void logProxySyncDiagnostic(const Inputs& inputs, const RuntimeStateSnapshot& snapshot);

    Callbacks callbacks_;
    std::optional<ProxyUiState> lastProxyUiStateLogged_;
};
