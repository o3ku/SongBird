#include "common/SystemProxySnapshot.h"

// The registry holds either a bare "host:port" or the per-scheme form
// "http=host:port;https=host:port;ftp=host:port". Both are matched by a plain substring test,
// which keeps this from re-implementing the parser in
// WindowsSystemProxyService::normalizeProxyServer for a comparison that only needs to answer
// "does this mention our endpoint".
bool systemProxySnapshotMentionsServer(const SystemProxySnapshot& snapshot, const QString& server)
{
    if (!snapshot.enabled || !snapshot.server.has_value()) {
        return false;
    }
    const QString needle = server.trimmed();
    if (needle.isEmpty()) {
        return false;
    }
    return snapshot.server->contains(needle, Qt::CaseInsensitive);
}

SystemProxySnapshot sanitizeSelfReferentialSnapshot(
    const SystemProxySnapshot& snapshot,
    const QString& ourServer)
{
    if (!systemProxySnapshotMentionsServer(snapshot, ourServer)) {
        return snapshot;
    }

    // Keep `valid` so the caller still restores something; only the values change. Restoring a
    // snapshot that says "proxy was off and none of these keys existed" turns the proxy off and
    // deletes our own leftovers, which is precisely the wanted outcome.
    SystemProxySnapshot cleared;
    cleared.valid = snapshot.valid;
    cleared.enabled = false;
    return cleared;
}
