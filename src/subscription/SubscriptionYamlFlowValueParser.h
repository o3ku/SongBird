#pragma once

#include <QJsonValue>
#include <QString>

namespace SubscriptionYamlFlowValueParser {

QJsonValue parseValue(const QString& text);

} // namespace SubscriptionYamlFlowValueParser
