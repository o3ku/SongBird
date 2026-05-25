#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ShareUrlBuilderSupport {

using QueryEntries = QList<QPair<QString, QString>>;

QString encodeUserInfoPart(const QString& value);
QueryEntries buildStandardTransportEntries(const VmessItem& item, const QString& defaultSecurity);
QString buildQuery(const QueryEntries& entries);
QString buildRemark(const QString& remarks);
QString joinList(const QStringList& values);
QString encodeBase64(const QString& value);
QString wrapIpv6(const QString& address);

} // namespace ShareUrlBuilderSupport
