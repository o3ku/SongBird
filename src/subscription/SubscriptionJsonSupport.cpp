#include "subscription/SubscriptionJsonSupport.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "common/PortValidator.h"

namespace SubscriptionJsonSupport {

QString scalarText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isDouble()) {
        return QString::number(value.toInt());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return {};
}

QString firstNonEmpty(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* rawKey : keys) {
        const QString key = QString::fromLatin1(rawKey);
        const QString value = scalarText(object.value(key));
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QStringList stringListValue(const QJsonValue& value)
{
    QStringList values;
    if (value.isString()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
        return values;
    }

    if (!value.isArray()) {
        return values;
    }

    const QJsonArray array = value.toArray();
    values.reserve(array.size());
    for (const QJsonValue& entry : array) {
        const QString text = scalarText(entry);
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values;
}

QString joinStringList(const QStringList& values)
{
    QStringList trimmed;
    trimmed.reserve(values.size());
    for (const QString& value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            trimmed.append(normalized);
        }
    }
    return trimmed.join(QStringLiteral(","));
}

QString compactJson(const QJsonValue& value)
{
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return {};
}

bool parsePortValue(const QJsonValue& value, int* port)
{
    if (port == nullptr) {
        return false;
    }

    if (value.isDouble()) {
        *port = value.toInt();
        return isValidTcpPort(*port);
    }

    if (!value.isString()) {
        return false;
    }

    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok);
    if (!ok || !isValidTcpPort(parsed)) {
        return false;
    }

    *port = parsed;
    return true;
}

QJsonObject objectField(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* rawKey : keys) {
        const QString key = QString::fromLatin1(rawKey);
        const QJsonValue value = object.value(key);
        if (value.isObject()) {
            return value.toObject();
        }
    }
    return {};
}

} // namespace SubscriptionJsonSupport
