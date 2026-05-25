#pragma once

#include <QPair>
#include <QString>
#include <QStringList>

namespace DnsAddressParser {

QStringList splitDnsAddresses(const QString& value);
QPair<QString, int> parseAuthority(QString authority);
QString extractHost(const QString& address);

} // namespace DnsAddressParser
