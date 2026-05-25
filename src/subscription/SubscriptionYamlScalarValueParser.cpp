#include "subscription/SubscriptionYamlScalarValueParser.h"

QString SubscriptionYamlScalarValueParser::scalar(const QString& text)
{
    QString value = text.trimmed();
    if ((value.startsWith(QChar('"')) && value.endsWith(QChar('"')))
        || (value.startsWith(QChar('\'')) && value.endsWith(QChar('\'')))) {
        value = value.mid(1, value.size() - 2);
    }
    return value.trimmed();
}

QJsonValue SubscriptionYamlScalarValueParser::parse(const QString& text)
{
    const QString normalized = scalar(text);
    if (normalized.isEmpty()) {
        return QString();
    }

    if (normalized.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (normalized.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    if (normalized.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0
        || normalized == QStringLiteral("~")) {
        return QJsonValue(QJsonValue::Null);
    }

    bool integerOk = false;
    const int integerValue = normalized.toInt(&integerOk);
    if (integerOk) {
        return integerValue;
    }

    return normalized;
}
