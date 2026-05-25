#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

namespace DnsHosts {

const QMap<QString, QStringList>& predefined();
bool isIpAddress(const QString& value);
bool isDomainName(const QString& value);
QMap<QString, QStringList> parseConfigured(const QString& hosts);
QMap<QString, QString> loadSystem();

} // namespace DnsHosts
