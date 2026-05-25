#pragma once

#include <initializer_list>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace SubscriptionJsonSupport {

QString scalarText(const QJsonValue& value);
QString firstNonEmpty(const QJsonObject& object, std::initializer_list<const char*> keys);
QStringList stringListValue(const QJsonValue& value);
QString joinStringList(const QStringList& values);
QString compactJson(const QJsonValue& value);
bool parsePortValue(const QJsonValue& value, int* port);
QJsonObject objectField(const QJsonObject& object, std::initializer_list<const char*> keys);

} // namespace SubscriptionJsonSupport
