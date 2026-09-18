#include "runtime/DnsAddressParser.h"

#include <QUrl>

namespace DnsAddressParser {

QStringList splitDnsAddresses(const QString& value)
{
    QString normalized = value;
    normalized.replace(QChar(';'), QChar(','));

    QStringList result;
    const QStringList parts = normalized.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

// Parses a DNS authority ("host", "host:port", "[ipv6]", "[ipv6]:port").
//
// Deliberately not EndpointParser::tryParseAddressAndPort: that helper requires
// a port and returns a bare host, whereas DNS authorities may omit the port
// (port 0 means "absent") and a bracketed IPv6 host is returned *with* its
// brackets. The brackets matter because the result is written into generated
// core config, where "[::1]:53" has to stay bracketed to be valid. Merging the
// two parsers would need a flag for each of those differences, so they are kept
// separate on purpose.
QPair<QString, int> parseAuthority(QString authority)
{
    authority = authority.trimmed();
    if (authority.isEmpty()) {
        return {};
    }

    if (authority.startsWith(QChar('['))) {
        const int bracketIndex = authority.indexOf(QChar(']'));
        if (bracketIndex > 0) {
            const QString host = authority.left(bracketIndex + 1);
            const QString remainder = authority.mid(bracketIndex + 1);
            if (remainder.startsWith(QChar(':'))) {
                bool ok = false;
                const int port = remainder.mid(1).toInt(&ok);
                return {host, ok ? port : 0};
            }
            return {host, 0};
        }
    }

    const int lastColon = authority.lastIndexOf(QChar(':'));
    if (lastColon > 0 && authority.indexOf(QChar(':')) == lastColon) {
        bool ok = false;
        const int port = authority.mid(lastColon + 1).toInt(&ok);
        if (ok) {
            return {authority.left(lastColon), port};
        }
    }

    return {authority, 0};
}

QString extractHost(const QString& address)
{
    const QStringList addresses = splitDnsAddresses(address);
    if (addresses.isEmpty()) {
        return {};
    }

    const QString first = addresses.constFirst();
    const QUrl url(first);
    if (!url.scheme().trimmed().isEmpty()) {
        return url.host(QUrl::FullyDecoded);
    }

    return parseAuthority(first).first;
}

} // namespace DnsAddressParser
