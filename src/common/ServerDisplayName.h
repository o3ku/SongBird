#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

inline QString serverDisplayName(const VmessItem& server, const QString& fallback = QStringLiteral("Unnamed Server"))
{
    const QString remarks = server.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    const QString address = server.address.trimmed();
    if (!address.isEmpty() && server.port > 0) {
        return QStringLiteral("%1:%2").arg(address).arg(server.port);
    }

    if (!address.isEmpty()) {
        return address;
    }

    return fallback;
}

inline QString elidedServerDisplayName(
    const VmessItem& server,
    int maximumCharacters,
    const QString& fallback = QStringLiteral("Unnamed Server"))
{
    const QString name = serverDisplayName(server, fallback);
    if (maximumCharacters <= 0 || name.size() <= maximumCharacters) {
        return name;
    }

    return name.left(maximumCharacters) + QStringLiteral("...");
}
