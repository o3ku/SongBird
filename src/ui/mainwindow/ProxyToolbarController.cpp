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
        const bool isActive = snapshot.uiState == ProxyUiState::Active;
        proxyToggleAction_->setText(isActive ? QObject::tr("STOP") : QObject::tr("START"));
        proxyToggleAction_->setCheckable(true);
        proxyToggleAction_->setChecked(isActive);

        QString toolTip;
        switch (snapshot.uiState) {
        case ProxyUiState::Transitioning:
            toolTip = QObject::tr("Proxy state transition is in progress.");
            break;
        case ProxyUiState::Inconsistent:
            toolTip = QObject::tr("Proxy state is inconsistent. Check logs.");
            break;
        case ProxyUiState::Active:
            toolTip = QObject::tr("Stop the running core and clear system proxy.");
            break;
        case ProxyUiState::Idle:
            toolTip = activeServer == nullptr
                ? QObject::tr("Select a server, then start proxy.")
                : QObject::tr("Start the core with the active server and enable system proxy.");
            break;
        }

        proxyToggleAction_->setToolTip(toolTip);
        proxyToggleAction_->setEnabled(
            snapshot.uiState == ProxyUiState::Idle
            || snapshot.uiState == ProxyUiState::Active);
    }

    if (tunToggleAction_ != nullptr) {
        tunToggleAction_->setText(QObject::tr("TUN"));
        tunToggleAction_->setCheckable(true);
        tunToggleAction_->setChecked(snapshot.tunEnabled);
        const bool canToggleTun = snapshot.uiState != ProxyUiState::Transitioning
            && snapshot.uiState != ProxyUiState::Inconsistent;
        QString toolTip = QObject::tr("Enable or disable TUN mode.");
        if (snapshot.uiState == ProxyUiState::Transitioning) {
            toolTip = QObject::tr("Proxy state transition is in progress.");
        } else if (snapshot.uiState == ProxyUiState::Inconsistent) {
            toolTip = QObject::tr("Proxy state is inconsistent. Check logs.");
        }

        tunToggleAction_->setToolTip(toolTip);
        tunToggleAction_->setEnabled(canToggleTun);
    }
}
