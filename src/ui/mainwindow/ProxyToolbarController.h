#pragma once

#include <QList>
#include <QString>

#include "app/RuntimeState.h"
#include "domain/models/Config.h"

class QAction;
struct ServerTableRow;

class ProxyToolbarController final {
public:
    struct Snapshot {
        ProxyUiState uiState = ProxyUiState::Idle;
        bool outboundLocationAvailable = false;
        bool tunEnabled = false;
        QList<CoreType> existingCoreTypes;
        QList<CoreTypeItem> coreTypeItems;
    };

    ProxyToolbarController(QAction* proxyToggleAction, QAction* tunToggleAction);

    void sync(
        const Snapshot& snapshot,
        const ServerTableRow* activeServer);

private:
    QAction* proxyToggleAction_ = nullptr;
    QAction* tunToggleAction_ = nullptr;
};
