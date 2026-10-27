#include "subscription/SubscriptionYamlParser.h"

#include "subscription/SubscriptionYamlFlowValueParser.h"
#include "subscription/SubscriptionYamlQuoteStateMachine.h"
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
    // keeps an IPv6 literal on the value side.
    int position = 0;
    while (position < text.size()) {
        const int separator = text.indexOf(QChar(':'), position);
        if (separator < 0) {
            return -1;
        }

        const QChar opening = text.at(position);
        if (opening == QChar('"') || opening == QChar('\'')) {
            // Skip the whole quoted scalar instead of stepping into it: a ':' inside
            // `"a: b"` never separates, and a scalar that never closes leaves no
            // separator outside it to find.
            const SubscriptionYamlQuoteStateMachine::Scan scan =
                SubscriptionYamlQuoteStateMachine::scanQuotedScalar(text, position);
            if (!scan.ok) {
                return -1;
            }
            position = scan.quoteEndedAt + 1;
            continue;
        }

        if (separator + 1 >= text.size() || text.at(separator + 1).isSpace()) {
            return separator;
        }
        position = separator + 1;
    }

    return -1;
}

bool splitKeyValue(const QString& text, QString* key, QString* value, bool* malformedValue)
{
    if (malformedValue != nullptr) {
        *malformedValue = false;
    }

    const int separator = keyValueSeparator(text);
    if (separator <= 0) {
        return false;
    }

    const QString rawValue = text.mid(separator + 1).trimmed();
    if (malformedValue != nullptr && !rawValue.isEmpty()
        && (rawValue.at(0) == QChar('"') || rawValue.at(0) == QChar('\''))) {
        // The quote never closes, so everything after it -- which may be the rest of the
        // node's fields -- is inside the value. The caller keeps the pair (there is no
        // better reading of it) and reports the node as one it could not use.
        *malformedValue = !SubscriptionYamlQuoteStateMachine::scanQuotedScalar(rawValue, 0).ok;
    }

    if (key != nullptr) {
        *key = text.left(separator).trimmed();
    }
    if (value != nullptr) {
        *value = scalar(rawValue);
    }
    return true;
}

QJsonValue parseValue(const QString& text)
{
    return SubscriptionYamlFlowValueParser::parseValue(text);
}

int commentIndex(const QString& line)
{
    int position = 0;
    while (position < line.size()) {
        const QChar current = line.at(position);
        if (current == QChar('"') || current == QChar('\'')) {
            const SubscriptionYamlQuoteStateMachine::Scan scan =
                SubscriptionYamlQuoteStateMachine::scanQuotedScalar(line, position);
            // An unclosed scalar swallows the rest of the line, so there is no comment
            // boundary in it: reporting one would cut a value in half.
            if (!scan.ok) {
                return -1;
            }
            position = scan.quoteEndedAt + 1;
            continue;
        }

        if (current == QChar('#')) {
            // A comment starts at "#" only at line start or after whitespace;
            // "a#b" is one scalar and the "#" belongs to it.
            if (position == 0 || line.at(position - 1).isSpace()) {
                return position;
            }
        }
        ++position;
    }

    return -1;
}

QString stripComment(const QString& line)
{
    const int position = commentIndex(line);
    return position >= 0 ? line.left(position) : line;
}

} // namespace SubscriptionYamlParser
