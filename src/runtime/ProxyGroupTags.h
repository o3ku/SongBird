#pragma once

#include <QString>

// Deterministic member tag for a group activation config.
//
// sing-box outbound tags and mihomo proxy names accept the same "member-N" scheme, and the
// front end maps a Clash-API group answer back to a server id by index, so one function
// serves every core that has a native auto-select group. The tags are unique within one
// generated config and stable for its lifetime (the config is regenerated whenever the
// member set changes, which re-bases the indices together).
inline QString proxyGroupMemberTag(int memberIndex)
{
    return QStringLiteral("member-%1").arg(memberIndex);
}
