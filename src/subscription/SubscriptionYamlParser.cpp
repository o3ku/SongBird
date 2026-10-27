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

int commentIndex(const QString& line)
{
    QChar quote;
    for (int position = 0; position < line.size(); ++position) {
        const QChar current = line.at(position);
        if (!quote.isNull()) {
            // Only double-quoted text carries backslash escapes, the same
            // asymmetry parseFlowQuotedString() and the scalar parser keep.
            if (quote == QChar('"') && current == QChar('\\')) {
                ++position;
                continue;
            }
            if (current == quote) {
                // YAML folds a doubled quote into one literal quote, so it does
                // not end the scalar; parseFlowQuotedString() agrees.
                if (position + 1 < line.size() && line.at(position + 1) == quote) {
                    ++position;
                    continue;
                }
                quote = QChar();
            }
            continue;
        }

        if (current == QChar('"') || current == QChar('\'')) {
            quote = current;
            continue;
        }

        if (current == QChar('#')) {
            // A comment starts at "#" only at line start or after whitespace;
            // "a#b" is one scalar and the "#" belongs to it.
            if (position == 0 || line.at(position - 1).isSpace()) {
                return position;
            }
        }
    }

    return -1;
}

QString stripComment(const QString& line)
{
    const int position = commentIndex(line);
    return position >= 0 ? line.left(position) : line;
}

} // namespace SubscriptionYamlParser
