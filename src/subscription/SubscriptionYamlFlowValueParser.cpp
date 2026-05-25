#include "subscription/SubscriptionYamlFlowValueParser.h"

#include <QJsonArray>
#include <QJsonObject>

#include "subscription/SubscriptionYamlScalarValueParser.h"

namespace {

void skipFlowWhitespace(const QString& text, int* position)
{
    if (position == nullptr) {
        return;
    }
    while (*position < text.size() && text.at(*position).isSpace()) {
        ++(*position);
    }
}

QString parseFlowQuotedString(const QString& text, int* position, bool* ok)
{
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr || *position >= text.size()) {
        return {};
    }

    const QChar quote = text.at(*position);
    if (quote != QChar('"') && quote != QChar('\'')) {
        return {};
    }

    ++(*position);
    QString result;
    while (*position < text.size()) {
        const QChar current = text.at(*position);
        if (current == quote) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return result;
        }

        if (current == QChar('\\') && *position + 1 < text.size()) {
            ++(*position);
            result.append(text.at(*position));
            ++(*position);
            continue;
        }

        result.append(current);
        ++(*position);
    }

    return {};
}

QJsonValue parseFlowValue(const QString& text, int* position, bool* ok);

QJsonArray parseFlowArray(const QString& text, int* position, bool* ok)
{
    QJsonArray array;
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr || *position >= text.size() || text.at(*position) != QChar('[')) {
        return array;
    }

    ++(*position);
    while (*position < text.size()) {
        skipFlowWhitespace(text, position);
        if (*position < text.size() && text.at(*position) == QChar(']')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return array;
        }

        bool valueOk = false;
        const QJsonValue value = parseFlowValue(text, position, &valueOk);
        if (!valueOk) {
            return {};
        }
        array.append(value);

        skipFlowWhitespace(text, position);
        if (*position >= text.size()) {
            return {};
        }
        if (text.at(*position) == QChar(',')) {
            ++(*position);
            continue;
        }
        if (text.at(*position) == QChar(']')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return array;
        }
        return {};
    }

    return {};
}

QJsonObject parseFlowObject(const QString& text, int* position, bool* ok)
{
    QJsonObject object;
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr || *position >= text.size() || text.at(*position) != QChar('{')) {
        return object;
    }

    ++(*position);
    while (*position < text.size()) {
        skipFlowWhitespace(text, position);
        if (*position < text.size() && text.at(*position) == QChar('}')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return object;
        }

        QString key;
        if (*position < text.size() && (text.at(*position) == QChar('"') || text.at(*position) == QChar('\''))) {
            bool keyOk = false;
            key = parseFlowQuotedString(text, position, &keyOk);
            if (!keyOk) {
                return {};
            }
        } else {
            const int keyStart = *position;
            while (*position < text.size() && text.at(*position) != QChar(':')) {
                ++(*position);
            }
            if (*position >= text.size()) {
                return {};
            }
            key = text.mid(keyStart, *position - keyStart).trimmed();
        }

        if (key.isEmpty() || *position >= text.size() || text.at(*position) != QChar(':')) {
            return {};
        }

        ++(*position);
        skipFlowWhitespace(text, position);

        bool valueOk = false;
        const QJsonValue value = parseFlowValue(text, position, &valueOk);
        if (!valueOk) {
            return {};
        }
        object.insert(key, value);

        skipFlowWhitespace(text, position);
        if (*position >= text.size()) {
            return {};
        }
        if (text.at(*position) == QChar(',')) {
            ++(*position);
            continue;
        }
        if (text.at(*position) == QChar('}')) {
            ++(*position);
            if (ok != nullptr) {
                *ok = true;
            }
            return object;
        }
        return {};
    }

    return {};
}

QJsonValue parseFlowValue(const QString& text, int* position, bool* ok)
{
    if (ok != nullptr) {
        *ok = false;
    }
    if (position == nullptr) {
        return {};
    }

    skipFlowWhitespace(text, position);
    if (*position >= text.size()) {
        return {};
    }

    const QChar current = text.at(*position);
    if (current == QChar('{')) {
        bool objectOk = false;
        const QJsonObject object = parseFlowObject(text, position, &objectOk);
        if (ok != nullptr) {
            *ok = objectOk;
        }
        return object;
    }
    if (current == QChar('[')) {
        bool arrayOk = false;
        const QJsonArray array = parseFlowArray(text, position, &arrayOk);
        if (ok != nullptr) {
            *ok = arrayOk;
        }
        return array;
    }
    if (current == QChar('"') || current == QChar('\'')) {
        bool stringOk = false;
        const QString value = parseFlowQuotedString(text, position, &stringOk);
        if (ok != nullptr) {
            *ok = stringOk;
        }
        return value;
    }

    const int start = *position;
    while (*position < text.size()) {
        const QChar ch = text.at(*position);
        if (ch == QChar(',') || ch == QChar('}') || ch == QChar(']')) {
            break;
        }
        ++(*position);
    }

    if (ok != nullptr) {
        *ok = true;
    }
    return SubscriptionYamlScalarValueParser::parse(text.mid(start, *position - start));
}

} // namespace

QJsonValue SubscriptionYamlFlowValueParser::parseValue(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    if (trimmed.startsWith(QChar('{'))) {
        int position = 0;
        bool ok = false;
        const QJsonObject object = parseFlowObject(trimmed, &position, &ok);
        skipFlowWhitespace(trimmed, &position);
        if (ok && position == trimmed.size()) {
            return object;
        }
    }

    if (trimmed.startsWith(QChar('['))) {
        int position = 0;
        bool ok = false;
        const QJsonArray array = parseFlowArray(trimmed, &position, &ok);
        skipFlowWhitespace(trimmed, &position);
        if (ok && position == trimmed.size()) {
            return array;
        }
    }

    return SubscriptionYamlScalarValueParser::parse(trimmed);
}
