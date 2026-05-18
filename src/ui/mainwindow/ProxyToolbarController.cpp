#include "ui/mainwindow/ProxyToolbarController.h"

#include <QAction>

#include "domain/models/VmessItem.h"
#include "runtime/ProtocolCoreCompat.h"
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
        proxyToggleAction_->setText(QObject::tr("PROXY"));
        proxyToggleAction_->setCheckable(true);
        proxyToggleAction_->setChecked(isProxyCheckedState(snapshot));

        QString toolTip = QObject::tr("Enable or disable proxy.");
        if (snapshot.coreTransitionPending) {
            toolTip = QObject::tr("Proxy state transition is in progress.");
        } else if (isProxyCheckedState(snapshot)) {
            toolTip = QObject::tr("Disable system proxy and stop the running core.");
        } else if (!isProxyUncheckedState(snapshot)) {
            toolTip = QObject::tr("Proxy state is synchronizing. Wait until the core and system proxy reach the same target state.");
        } else if (activeServer == nullptr) {
            toolTip = QObject::tr("No available server. Add or import a server first.");
        } else {
            Config configForCoreSelection;
            configForCoreSelection.coreTypeItems = snapshot.coreTypeItems;

            VmessItem activeServerConfig;
            activeServerConfig.indexId = activeServer->indexId;
            activeServerConfig.configType = activeServer->configType;
            activeServerConfig.address = activeServer->address;
            activeServerConfig.port = activeServer->port;
            activeServerConfig.remarks = activeServer->remarks;
            activeServerConfig.security = activeServer->security;
            activeServerConfig.network = activeServer->network;
            activeServerConfig.streamSecurity = activeServer->streamSecurity;
            activeServerConfig.testResult = activeServer->testResult;
            activeServerConfig.subId = activeServer->subId;
            activeServerConfig.sort = activeServer->sort;

            const CoreType requiredCore =
                resolveSelectedCoreType(configForCoreSelection, activeServerConfig, snapshot.existingCoreTypes);
            if (!snapshot.existingCoreTypes.contains(requiredCore)) {
                toolTip = QObject::tr("No compatible %1 core is installed for the active server \"%2\".")
                              .arg(coreTypeDisplayName(requiredCore))
                              .arg(activeServer->remarks.trimmed().isEmpty()
                                       ? QObject::tr("Unnamed server")
                                       : activeServer->remarks.trimmed());
            } else {
                toolTip = QObject::tr("Start the core with the active server and enable system proxy.");
            }
        }

        proxyToggleAction_->setToolTip(toolTip);
        proxyToggleAction_->setEnabled(
            snapshot.hasServers
            && (isProxyCheckedState(snapshot) || isProxyUncheckedState(snapshot)));
    }

    if (tunToggleAction_ != nullptr) {
        tunToggleAction_->setText(QObject::tr("TUN"));
        tunToggleAction_->setCheckable(true);
        tunToggleAction_->setChecked(snapshot.tunEnabled);
        tunToggleAction_->setToolTip(QObject::tr("Enable or disable TUN mode."));
        tunToggleAction_->setEnabled(!snapshot.coreTransitionPending);
    }
}

void ProxyToolbarController::refresh(
    bool coreProcessRunning,
    bool coreRunning,
    bool coreTransitionPending,
    bool systemProxyApplied,
    bool tunEnabled,
    bool hasServers,
    const QList<CoreType>& existingCoreTypes,
    const QList<CoreTypeItem>& coreTypeItems,
    const ServerTableRow* activeServer)
{
    Snapshot snapshot;
    snapshot.coreProcessRunning = coreProcessRunning;
    snapshot.coreRunning = coreRunning;
    snapshot.coreTransitionPending = coreTransitionPending;
    snapshot.systemProxyApplied = systemProxyApplied;
    snapshot.tunEnabled = tunEnabled;
    snapshot.hasServers = hasServers;
    snapshot.existingCoreTypes = existingCoreTypes;
    snapshot.coreTypeItems = coreTypeItems;
    sync(snapshot, activeServer);
}

bool ProxyToolbarController::isProxyCheckedState(const Snapshot& snapshot)
{
    return snapshot.coreRunning && snapshot.systemProxyApplied && !snapshot.coreTransitionPending;
}

bool ProxyToolbarController::isProxyUncheckedState(const Snapshot& snapshot)
{
    return !snapshot.coreProcessRunning
        && !snapshot.coreRunning
        && !snapshot.systemProxyApplied
        && !snapshot.coreTransitionPending;
}
