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
    bool shouldDisableProxy(const Snapshot& snapshot) const;
    bool shouldEnableProxy(const Snapshot& snapshot, const ServerTableRow* activeServer) const;
    void refresh(
        ProxyUiState uiState,
        bool outboundLocationAvailable,
        bool tunEnabled,
        const QList<CoreType>& existingCoreTypes,
        const QList<CoreTypeItem>& coreTypeItems,
        const ServerTableRow* activeServer);

private:
    QAction* proxyToggleAction_ = nullptr;
    QAction* tunToggleAction_ = nullptr;
};
