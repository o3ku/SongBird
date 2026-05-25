#pragma once

#include <QJsonValue>
#include <QString>

namespace SubscriptionYamlScalarValueParser {

QString scalar(const QString& text);
QJsonValue parse(const QString& text);

} // namespace SubscriptionYamlScalarValueParser
