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

// The embeddable members of a location group, in config list order (the first is the group's
// anchor). FAVORITE is the favorite-flag pseudo-group; every other id is the resolved
// location (manual override first, name inference second -- LocationCatalog). Custom configs
// are excluded because a whole-file config cannot become one outbound of a group.
inline QList<VmessItem> embeddableMembers(const QList<VmessItem>& servers, const QString& locationId)
{
    const QString trimmed = locationId.trimmed();
    const bool favoriteGroup = trimmed == QLatin1String("FAVORITE");
    QList<VmessItem> members;
    for (const VmessItem& server : servers) {
        if (server.indexId.trimmed().isEmpty() || server.configType == ConfigType::Custom) {
            continue;
        }
        const bool inGroup = favoriteGroup
            ? server.favorite
            : LocationCatalog::resolvedLocationId(server) == trimmed;
        if (inGroup) {
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
