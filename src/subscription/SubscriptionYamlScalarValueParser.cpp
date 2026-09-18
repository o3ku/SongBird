#include "subscription/SubscriptionYamlScalarValueParser.h"

#include "subscription/SubscriptionYamlEscape.h"

namespace {

// YAML single-quoted scalars have no backslash escapes; the only special
// sequence is a doubled quote, which folds into a single literal quote.
QString unescapeSingleQuotedScalar(const QString& value)
{
    QString result;
    result.reserve(value.size());
    for (int position = 0; position < value.size(); ++position) {
        result.append(value.at(position));
        if (value.at(position) == QChar('\'') && position + 1 < value.size()
            && value.at(position + 1) == QChar('\'')) {
            ++position;
        }
    }
    return result;
}

} // namespace

QString SubscriptionYamlScalarValueParser::scalar(const QString& text)
{
    QString value = text.trimmed();
    const bool doubleQuoted = value.startsWith(QChar('"')) && value.endsWith(QChar('"')) && value.size() >= 2;
    const bool singleQuoted = value.startsWith(QChar('\'')) && value.endsWith(QChar('\'')) && value.size() >= 2;
    if (!doubleQuoted && !singleQuoted) {
        return value;
    }

    value = value.mid(1, value.size() - 2);
    // Per YAML, only double-quoted scalars process backslash escapes; applying
    // the decoder to single-quoted text would corrupt names such as 'C:\folder'.
    return doubleQuoted
        ? SubscriptionYamlEscape::unescape(value).trimmed()
        : unescapeSingleQuotedScalar(value).trimmed();
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
