#include "app/RuntimeState.h"

RuntimeState::RuntimeState(QObject* parent)
    : QObject(parent)
{
}

RuntimeStateSnapshot RuntimeState::snapshot() const
{
    return snapshot_;
}

void RuntimeState::applySnapshot(const RuntimeStateSnapshot& snapshot)
{
    const bool currentServerChanged = snapshot_.currentServerName != snapshot.currentServerName
        || snapshot_.currentServerLocation != snapshot.currentServerLocation
        || snapshot_.currentServerWarning != snapshot.currentServerWarning;
    const bool proxyUiStateChanged = snapshot_.proxyUiState != snapshot.proxyUiState;
    const bool systemProxyStateChanged = snapshot_.systemProxyMode != snapshot.systemProxyMode
        || snapshot_.systemProxyApplied != snapshot.systemProxyApplied;
    const bool autoRunChanged = snapshot_.autoRunEnabled != snapshot.autoRunEnabled;
    const bool routingStatusChanged = snapshot_.routingSummary != snapshot.routingSummary
        || snapshot_.listenSummary != snapshot.listenSummary;

    snapshot_ = snapshot;

    emit this->snapshotApplied(snapshot_);

    if (currentServerChanged) {
        emit this->currentServerChanged(
            snapshot_.currentServerName,
            snapshot_.currentServerLocation,
            snapshot_.currentServerWarning);
    }
    if (proxyUiStateChanged) {
        emit this->proxyUiStateChanged(snapshot_.proxyUiState);
    }
    if (systemProxyStateChanged) {
        emit this->systemProxyStateChanged(
            toLegacySystemProxyModeValue(snapshot_.systemProxyMode),
            snapshot_.systemProxyApplied);
    }
    if (autoRunChanged) {
        emit this->autoRunChanged(snapshot_.autoRunEnabled);
    }
    if (routingStatusChanged) {
        emit this->routingStatusChanged(
            snapshot_.routingSummary,
            snapshot_.listenSummary);
    }
}
