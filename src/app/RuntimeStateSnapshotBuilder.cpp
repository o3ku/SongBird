#include "app/RuntimeStateSnapshotBuilder.h"

#include <utility>

#include <QCoreApplication>

#include "common/LocalInboundPorts.h"
#include "domain/models/RoutingProfiles.h"
#include "appcore/ProxyUiStateDecision.h"

RuntimeStateSnapshotBuilder::RuntimeStateSnapshotBuilder(Callbacks callbacks)
    : callbacks_(std::move(callbacks))
{
}

RuntimeStateSnapshot RuntimeStateSnapshotBuilder::buildSnapshot(const Inputs& inputs)
{
    RuntimeStateSnapshot snapshot;
    snapshot.currentServerName = inputs.currentServerName;
    snapshot.currentServerLocation = inputs.currentServerLocation;
    snapshot.currentServerWarning = inputs.currentServerWarning;
    if (inputs.config != nullptr) {
        snapshot.routingSummary = buildRoutingSummaryText(*inputs.config);
        snapshot.listenSummary = buildListenSummaryText(*inputs.config);
    }
    snapshot.proxyUiState = computeProxyUiState(inputs);
    snapshot.systemProxyMode = inputs.systemProxyMode;
    snapshot.systemProxyApplied = inputs.systemProxyApplied;
    snapshot.autoRunEnabled = inputs.autoRunEnabled;
    logProxySyncDiagnostic(inputs, snapshot);
    return snapshot;
}

QString RuntimeStateSnapshotBuilder::buildRoutingSummaryText(const Config& config) const
{
    const QList<RoutingItem> items = RoutingProfiles::routingItems(config.collection());
    if (items.isEmpty()) {
        return QCoreApplication::translate("AppBootstrap", "Basic");
    }

    const int index = RoutingProfiles::selectedRoutingIndex(config.collection());
    if (index >= 0 && index < items.size()) {
        const QString remarks = items.at(index).remarks.trimmed();
        return remarks.isEmpty()
            ? QCoreApplication::translate("AppBootstrap", "Routing %1").arg(index + 1)
            : remarks;
    }

    return QCoreApplication::translate("AppBootstrap", "Basic");
}

QString RuntimeStateSnapshotBuilder::buildListenSummaryText(const Config& config) const
{
    if (config.localPort <= 0) {
        return QCoreApplication::translate("AppBootstrap", "Unavailable");
    }

    return QCoreApplication::translate("AppBootstrap", "SOCKS %1 | HTTP %2")
        .arg(config.localPort)
        .arg(LocalInboundPorts::httpPort(config));
}

ProxyUiState RuntimeStateSnapshotBuilder::computeProxyUiState(const Inputs& inputs) const
{
    return decideProxyUiState(
        inputs.proxyTransitioning,
        inputs.coreReady,
        inputs.coreRunning,
        inputs.systemProxyApplied,
        inputs.outboundReachable);
}

void RuntimeStateSnapshotBuilder::logProxySyncDiagnostic(const Inputs& inputs, const RuntimeStateSnapshot& snapshot)
{
    const auto stateName = [](ProxyUiState state) -> QString {
        switch (state) {
        case ProxyUiState::Idle:
            return QStringLiteral("Idle");
        case ProxyUiState::Transitioning:
            return QStringLiteral("Transitioning");
        case ProxyUiState::Active:
            return QStringLiteral("Active");
        case ProxyUiState::Inconsistent:
            return QStringLiteral("Inconsistent");
        }
        return QStringLiteral("Unknown");
    };

    if (snapshot.proxyUiState != ProxyUiState::Inconsistent) {
        if (lastProxyUiStateLogged_.has_value()
            && *lastProxyUiStateLogged_ == ProxyUiState::Inconsistent
            && callbacks_.appendLog) {
            callbacks_.appendLog(
                QStringLiteral("Proxy UI state recovered to %1").arg(stateName(snapshot.proxyUiState)));
        }
        lastProxyUiStateLogged_ = snapshot.proxyUiState;
        return;
    }

    if (lastProxyUiStateLogged_.has_value() && *lastProxyUiStateLogged_ == ProxyUiState::Inconsistent) {
        return;
    }

    const QString diagnostic = QStringLiteral(
        "Proxy UI state inconsistent: phase=%1 coreProcessRunning=%2 coreReady=%3 "
        "systemProxyApplied=%4 outboundReachable=%5 currentServerLocation=\"%6\" systemProxyMode=%7")
        .arg(inputs.proxyPhaseName)
        .arg(inputs.coreRunning ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(inputs.coreReady ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(snapshot.systemProxyApplied ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(inputs.outboundReachable ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(snapshot.currentServerLocation.trimmed())
        .arg(systemProxyModeDisplayName(snapshot.systemProxyMode));

    lastProxyUiStateLogged_ = ProxyUiState::Inconsistent;
    if (callbacks_.appendLog) {
        callbacks_.appendLog(diagnostic);
    }
}
