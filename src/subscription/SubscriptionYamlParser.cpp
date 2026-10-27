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

QString stripComment(const QString& line)
{
    // A comment starts at " #" and only outside a quoted scalar: `name: "Gemini #1"`
    // is a value, and cutting it at the hash turns it into a different value -- or,
    // when the cut lands before the fields that decide the node is usable, into a
    // silently dropped node. Backslash escaping is honoured inside double quotes
    // because the flow-style parser this feeds also honours it.
    QChar quote;
    bool escaped = false;
    for (int index = 0; index < line.size(); ++index) {
        const QChar ch = line.at(index);
        if (!quote.isNull()) {
            if (escaped) {
                escaped = false;
            } else if (ch == QChar('\\') && quote == QChar('"')) {
                escaped = true;
            } else if (ch == quote) {
                quote = QChar();
            }
            continue;
        }

        if (ch == QChar('"') || ch == QChar('\'')) {
            quote = ch;
            continue;
        }

        if (ch == QChar('#') && index > 0 && line.at(index - 1).isSpace()) {
            return line.left(index);
        }
    }

    return line;
}

} // namespace SubscriptionYamlParser
