#pragma once

#include <QString>

#include "common/PortValidator.h"

namespace EndpointParser {

// Parses "host:port", "[ipv6]:port" endpoints into address + TCP port.
// Returns false for empty input, a missing port, or a port outside the valid
// TCP range. Bracketed IPv6 without a port is rejected; callers that accept a
// bare host or optional ports need their own parsing.
inline bool tryParseAddressAndPort(const QString& endpoint, QString& address, int& port)
{
    const QString trimmed = endpoint.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith(QLatin1Char('['))) {
        const int closingIndex = trimmed.indexOf(QStringLiteral("]:"));
        if (closingIndex <= 0) {
            return false;
        }

        bool ok = false;
        address = trimmed.mid(1, closingIndex - 1);
        port = trimmed.mid(closingIndex + 2).toInt(&ok);
        return ok && !address.isEmpty() && isValidTcpPort(port);
    }

    const int separatorIndex = trimmed.lastIndexOf(QLatin1Char(':'));
    if (separatorIndex <= 0) {
        return false;
    }

    bool ok = false;
    address = trimmed.left(separatorIndex);
    port = trimmed.mid(separatorIndex + 1).toInt(&ok);
    return ok && !address.isEmpty() && isValidTcpPort(port);
}

} // namespace EndpointParser
