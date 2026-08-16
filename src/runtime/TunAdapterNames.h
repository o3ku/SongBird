#pragma once

#include <QString>
#include <QStringList>

inline QString songbirdTunAdapterName()
{
    return QStringLiteral("songbird_tun");
}

inline QString legacySingBoxTunAdapterName()
{
    return QStringLiteral("singbox_tun");
}

inline QStringList tunAdapterCleanupNames()
{
    return {
        songbirdTunAdapterName(),
        legacySingBoxTunAdapterName()};
}
