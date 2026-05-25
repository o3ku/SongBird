#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include "domain/models/VmessItem.h"

namespace ShareUrlParserSupport {

QString normalizeJsonText(const QString& value);
QString decodedUserName(const QUrl& url);
QString decodedPassword(const QUrl& url);
QString firstNonEmptyQueryValue(const QUrlQuery& query, const QStringList& keys);
QString decodeBase64(const QString& value);
QStringList splitCsv(const QString& value);
int parseInt(const QString& value);

bool tryAssignUserInfo(QString credentials, QString& first, QString& second, bool allowEmpty = false);
bool tryParseAddressAndPort(const QString& endpoint, QString& address, int& port);
void resolveStandardTransport(const QUrlQuery& query, VmessItem& item);

} // namespace ShareUrlParserSupport
