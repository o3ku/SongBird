#pragma once

#include <QList>
#include <QString>

#include "appcore/LocationCatalog.h"
#include "domain/models/VmessItem.h"

// The pure half of location-group membership. Kept free of QObject and I/O so a test needs
// neither a config file nor a core -- the same discipline as AutoSwitchGroupLogic, which
// keeps its own scan for the failure path (src/app may not be included from appcore, and
// this direction is the one both layers can share).
namespace LocationGroupScan {

// Whether a server belongs to a location group. FAVORITE is the favorite-flag pseudo-group;
// every other id is the resolved location (manual override first, name inference second --
// LocationCatalog). Custom configs are excluded because a whole-file config cannot become one
// outbound of a group, and a server with no id cannot be addressed.
//
// Membership is deliberately favorite-*inclusive* for a location group: a favorited US node is
// shown in the US tab and is one of the US group's members, so it also belongs to FAVORITE.
// Callers that only need "does this click land inside the running group" must use this predicate
// (the group's own member list is built from it), never locationIdOfServer -- which resolves the
// favorite pseudo-group first and would read the same node as "a different group".
inline bool isGroupMember(const VmessItem& server, const QString& locationId)
{
    if (server.indexId.trimmed().isEmpty() || server.configType == ConfigType::Custom) {
        return false;
    }
    const QString trimmed = locationId.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    return trimmed == QLatin1String("FAVORITE")
        ? server.favorite
        : LocationCatalog::resolvedLocationId(server) == trimmed;
}

// The embeddable members of a location group, in config list order (the first is the group's
// anchor).
inline QList<VmessItem> embeddableMembers(const QList<VmessItem>& servers, const QString& locationId)
{
    QList<VmessItem> members;
    for (const VmessItem& server : servers) {
        if (isGroupMember(server, locationId)) {
            members.append(server);
        }
    }
    return members;
}

// The location id a server belongs to, with the favorite pseudo-group resolved first.
inline QString locationIdOfServer(const VmessItem& server)
{
    return server.favorite
        ? QStringLiteral("FAVORITE")
        : LocationCatalog::resolvedLocationId(server);
}

} // namespace LocationGroupScan
