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

// A JSON number is a double, and a millisecond timestamp is ~1.7e12 -- inside the 2^53 range
// where every integer is exactly representable, so the round trip through QJsonValue loses
// nothing. An absent or non-numeric value falls back, which is what makes a config written
// before this field existed read as "never updated" rather than as an arbitrary instant.
inline qint64 readInt64(const QJsonObject& object, const QString& key, qint64 fallback = 0)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? static_cast<qint64>(value.toDouble()) : fallback;
}

inline bool readBool(const QJsonObject& object, const QString& key, bool fallback = false)
{
    return object.value(key).toBool(fallback);
}

inline QJsonObject readObject(const QJsonObject& object, const QString& key)
{
    return object.value(key).toObject();
}

inline QJsonArray readArray(const QJsonObject& object, const QString& key)
{
    return object.value(key).toArray();
}

inline QStringList readStringList(const QJsonObject& object, const QString& key)
{
    QStringList values;
    const QJsonArray array = readArray(object, key);
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

inline void writeIfNotDefault(QJsonObject& object, const QString& key, qint64 value, qint64 defaultValue)
{
    // Written as a double because that is what QJsonValue stores; the value is an integer in
    // the exactly-representable range, so it reads back unchanged.
    if (value != defaultValue) {
        object.insert(key, static_cast<double>(value));
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

inline void writeArrayIfNotEmpty(QJsonObject& object, const QString& key, const QJsonArray& value)
{
    if (!value.isEmpty()) {
        object.insert(key, value);
    }
}

inline void writeObjectIfNotEmpty(QJsonObject& object, const QString& key, const QJsonObject& value)
{
    if (!value.isEmpty()) {
        object.insert(key, value);
    }
}

inline void writeStringListIfNotEmpty(QJsonObject& object, const QString& key, const QStringList& values)
{
    const QJsonArray array = toStringArray(values);
    writeArrayIfNotEmpty(object, key, array);
}

} // namespace JsonConfigUtils
