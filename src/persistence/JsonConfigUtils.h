#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace JsonConfigUtils {

inline QString readString(const QJsonObject& object, const QString& key, const QString& fallback = QString())
{
    return object.value(key).toString(fallback);
}

inline int readInt(const QJsonObject& object, const QString& key, int fallback = 0)
{
    return object.value(key).toInt(fallback);
}

inline bool readBool(const QJsonObject& object, const QString& key, bool fallback = false)
{
    return object.value(key).toBool(fallback);
}

inline QStringList readStringList(const QJsonObject& object, const QString& key)
{
    QStringList values;
    const QJsonArray array = object.value(key).toArray();
    values.reserve(array.size());
    for (const QJsonValue& value : array) {
        values.append(value.toString());
    }
    return values;
}

inline QJsonArray toStringArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

inline void writeIfNotEmpty(QJsonObject& object, const QString& key, const QString& value)
{
    if (!value.trimmed().isEmpty()) {
        object.insert(key, value);
    }
}

inline void writeIfTrue(QJsonObject& object, const QString& key, bool value)
{
    if (value) {
        object.insert(key, true);
    }
}

inline void writeIfFalse(QJsonObject& object, const QString& key, bool value)
{
    if (!value) {
        object.insert(key, false);
    }
}

inline void writeIfNotDefault(QJsonObject& object, const QString& key, bool value, bool defaultValue)
{
    if (value != defaultValue) {
        object.insert(key, value);
    }
}

inline void writeIfNotDefault(QJsonObject& object, const QString& key, int value, int defaultValue)
{
    if (value != defaultValue) {
        object.insert(key, value);
    }
}

inline void writeIfNotDefault(
    QJsonObject& object,
    const QString& key,
    const QString& value,
    const QString& defaultValue)
{
    if (value != defaultValue) {
        object.insert(key, value);
    }
}

inline void writeStringListIfNotEmpty(QJsonObject& object, const QString& key, const QStringList& values)
{
    const QJsonArray array = toStringArray(values);
    if (!array.isEmpty()) {
        object.insert(key, array);
    }
}

} // namespace JsonConfigUtils
