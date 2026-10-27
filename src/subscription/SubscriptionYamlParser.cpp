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

int keyValueSeparator(const QString& text)
{
    // The first ':' outside a quoted scalar. `name: "a: b"` and
    // `server: "[2001:db8::1]:443"` split at the field, not inside the value, and a
    // split inside the value turns the node's data into a key it never had -- or,
    // when the cut lands before the fields that make the node usable, drops it.
    //
    // Only ':' followed by whitespace (or the end of the line) separates a key from
    // its value in block style; a bare `a:b` is the whole scalar, which is also what
    // keeps an IPv6 literal on the value side. Backslash escaping is honoured inside
    // double quotes for the same reason stripComment() honours it: the flow-style
    // parser this feeds does.
    QChar quote;
    bool escaped = false;
    for (int index = 0; index < text.size(); ++index) {
        const QChar ch = text.at(index);
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

        if (ch == QChar(':') && (index + 1 >= text.size() || text.at(index + 1).isSpace())) {
            return index;
        }
    }

    return -1;
}

bool splitKeyValue(const QString& text, QString* key, QString* value)
{
    const int separator = keyValueSeparator(text);
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
