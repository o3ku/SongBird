#pragma once

#include <QJsonValue>
#include <QString>

namespace SubscriptionYamlParser {

QString scalar(const QString& text);
bool isListItem(const QString& line);
int indent(const QString& line);
bool splitKeyValue(const QString& text, QString* key, QString* value);
QJsonValue parseValue(const QString& text);

} // namespace SubscriptionYamlParser
