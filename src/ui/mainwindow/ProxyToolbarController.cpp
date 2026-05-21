#include "ui/mainwindow/ProxyToolbarController.h"

#include <QAction>

#include "ui/models/ServerTableModel.h"

ProxyToolbarController::ProxyToolbarController(
    QAction* proxyToggleAction,
    QAction* tunToggleAction)
    : proxyToggleAction_(proxyToggleAction)
    , tunToggleAction_(tunToggleAction)
{
}

void ProxyToolbarController::sync(
    const Snapshot& snapshot,
    const ServerTableRow* activeServer)
{
    if (proxyToggleAction_ != nullptr) {
        proxyToggleAction_->setText(isProxyCheckedState(snapshot) ? QObject::tr("STOP") : QObject::tr("START"));
        proxyToggleAction_->setCheckable(true);
        proxyToggleAction_->setChecked(isProxyCheckedState(snapshot));

        QString toolTip = QObject::tr("Start proxy.");
        if (snapshot.coreTransitionPending) {
            toolTip = QObject::tr("Proxy state transition is in progress.");
        } else if (isProxyCheckedState(snapshot)) {
            toolTip = QObject::tr("Stop the running core and clear system proxy.");
        } else if (!isProxyUncheckedState(snapshot)) {
            toolTip = QObject::tr("Proxy state is synchronizing. Wait until the core and system proxy reach the target state.");
        } else if (activeServer == nullptr) {
            toolTip = QObject::tr("Select a server, then start proxy.");
        } else {
            toolTip = QObject::tr("Start the core with the active server and enable system proxy.");
        }

        proxyToggleAction_->setToolTip(toolTip);
        proxyToggleAction_->setEnabled(
            isProxyCheckedState(snapshot)
            || isProxyUncheckedState(snapshot));
    }

    if (tunToggleAction_ != nullptr) {
        tunToggleAction_->setText(QObject::tr("TUN"));
        tunToggleAction_->setCheckable(true);
        tunToggleAction_->setChecked(snapshot.tunEnabled);
        bool canToggleTun = true;
        QString toolTip = QObject::tr("Enable or disable TUN mode.");
        if (snapshot.coreTransitionPending) {
            canToggleTun = false;
            toolTip = QObject::tr("Proxy state transition is in progress.");
        }

        tunToggleAction_->setToolTip(toolTip);
        tunToggleAction_->setEnabled(canToggleTun);
    }
}

void ProxyToolbarController::refresh(
    bool coreProcessRunning,
    bool coreRunning,
    bool coreTransitionPending,
    bool systemProxyApplied,
    bool outboundLocationAvailable,
    bool tunEnabled,
    const QList<CoreType>& existingCoreTypes,
    const QList<CoreTypeItem>& coreTypeItems,
    const ServerTableRow* activeServer)
{
    Snapshot snapshot;
    snapshot.coreProcessRunning = coreProcessRunning;
    snapshot.coreRunning = coreRunning;
    snapshot.coreTransitionPending = coreTransitionPending;
    snapshot.systemProxyApplied = systemProxyApplied;
    snapshot.outboundLocationAvailable = outboundLocationAvailable;
    snapshot.tunEnabled = tunEnabled;
    snapshot.existingCoreTypes = existingCoreTypes;
    snapshot.coreTypeItems = coreTypeItems;
    sync(snapshot, activeServer);
}

bool ProxyToolbarController::shouldDisableProxy(const Snapshot& snapshot) const
{
    return isProxyCheckedState(snapshot);
}

bool ProxyToolbarController::shouldEnableProxy(
    const Snapshot& snapshot,
    const ServerTableRow* activeServer) const
{
    if (!isProxyUncheckedState(snapshot) || activeServer == nullptr) {
        return false;
    }

    return true;
}

bool ProxyToolbarController::isProxyCheckedState(const Snapshot& snapshot)
{
    return snapshot.coreRunning
        && snapshot.systemProxyApplied
        && snapshot.outboundLocationAvailable
        && !snapshot.coreTransitionPending;
}

bool ProxyToolbarController::isProxyUncheckedState(const Snapshot& snapshot)
{
    return !snapshot.coreProcessRunning
        && !snapshot.coreRunning
        && !snapshot.coreTransitionPending;
}
