#pragma once

#include <QList>
#include <QString>

#include "appcore/LocationGroupScan.h"
#include "domain/models/VmessItem.h"

// The pure half of "what does clicking a server mean right now" (AppBootstrapMainWindowWiring
// routes the answer to the coordinators). Kept free of QObject and I/O so a test needs neither
// a window nor a core -- the same discipline as AutoSwitchGroupLogic.
namespace ServerActivationGestureLogic {

struct ActivationGesture {
    enum class Kind {
        // Set the clicked node as the single active server -- the pre-group behavior.
        ActivateNode,
        // Start (or switch to) the clicked node's location group as a core auto-select group.
        ActivateGroup,
        // The click changes nothing: the clicked node's group is already what is running.
        NoOp,
    };

    Kind kind = Kind::NoOp;
    QString indexId;     // ActivateNode: the node to activate
    QString locationId;  // ActivateGroup: the group to activate
};

inline ActivationGesture resolveServerActivationGesture(
    bool coreRunning,
    const QString& activeGroupLocationId,
    const QList<VmessItem>& servers,
    const QString& clickedIndexId)
{
    const QString clickedId = clickedIndexId.trimmed();
    if (clickedId.isEmpty()) {
        return {};
    }

    const VmessItem* clicked = nullptr;
    for (const VmessItem& server : servers) {
        if (server.indexId == clickedId) {
            clicked = &server;
            break;
        }
    }
    if (clicked == nullptr) {
        return {};
    }

    // A custom config can never join a group, so clicking one always means single-node -- and
    // so does every click while nothing runs: activation from stopped keeps the user's node.
    if (clicked->configType == ConfigType::Custom || !coreRunning
        || activeGroupLocationId.trimmed().isEmpty()) {
        return {ActivationGesture::Kind::ActivateNode, clickedId, {}};
    }

    // A group session runs: clicking a node of that same group is a no-op (no needless
    // restart), clicking a node of another group switches the auto-select to that group.
    //
    // Membership goes through isGroupMember, the same predicate the group's own member list is
    // built from -- NOT locationIdOfServer. A favorited node is a member of its location group
    // (it is shown in that tab and the group activation embeds it), so clicking one while that
    // group runs is a no-op; locationIdOfServer resolves FAVORITE first and would have wrongly
    // reported "another group" and restarted the session on the FAVORITE pseudo-group.
    if (LocationGroupScan::isGroupMember(*clicked, activeGroupLocationId)) {
        return {};
    }
    return {ActivationGesture::Kind::ActivateGroup, {}, LocationGroupScan::locationIdOfServer(*clicked)};
}

} // namespace ServerActivationGestureLogic
