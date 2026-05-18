#pragma once

#include <QList>
#include <QString>

#include "domain/models/Config.h"

class QAction;
struct ServerTableRow;

class ProxyToolbarController final {
public:
    struct Snapshot {
        bool coreProcessRunning = false;
        bool coreRunning = false;
        bool coreTransitionPending = false;
        bool systemProxyApplied = false;
        bool tunEnabled = false;
        bool hasServers = false;
        QList<CoreType> existingCoreTypes;
        QList<CoreTypeItem> coreTypeItems;
    };

    ProxyToolbarController(QAction* proxyToggleAction, QAction* tunToggleAction);

    void sync(
        const Snapshot& snapshot,
        const ServerTableRow* activeServer);
    void refresh(
        bool coreProcessRunning,
        bool coreRunning,
        bool coreTransitionPending,
        bool systemProxyApplied,
        bool tunEnabled,
        bool hasServers,
        const QList<CoreType>& existingCoreTypes,
        const QList<CoreTypeItem>& coreTypeItems,
        const ServerTableRow* activeServer);

private:
    static bool isProxyCheckedState(const Snapshot& snapshot);
    static bool isProxyUncheckedState(const Snapshot& snapshot);

    QAction* proxyToggleAction_ = nullptr;
    QAction* tunToggleAction_ = nullptr;
};
