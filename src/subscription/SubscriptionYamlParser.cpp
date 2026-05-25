#include "subscription/SubscriptionYamlParser.h"

#include "subscription/SubscriptionYamlFlowValueParser.h"
#include "subscription/SubscriptionYamlScalarValueParser.h"

namespace SubscriptionYamlParser {

QString scalar(const QString& text)
{
    return SubscriptionYamlScalarValueParser::scalar(text);
}

bool isListItem(const QString& line)
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith(QStringLiteral("- "));
}

int indent(const QString& line)
{
    int count = 0;
    while (count < line.size() && line.at(count).isSpace()) {
        ++count;
    }
    return count;
}

bool splitKeyValue(const QString& text, QString* key, QString* value)
{
    const int separator = text.indexOf(QChar(':'));
    if (separator <= 0) {
        return false;
    }

    if (key != nullptr) {
        *key = text.left(separator).trimmed();
    }
    if (value != nullptr) {
        *value = scalar(text.mid(separator + 1));
    }
    return true;
}

QJsonValue parseValue(const QString& text)
{
    return SubscriptionYamlFlowValueParser::parseValue(text);
}

} // namespace SubscriptionYamlParser
