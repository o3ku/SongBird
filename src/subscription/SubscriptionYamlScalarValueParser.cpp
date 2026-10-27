#include "subscription/SubscriptionYamlScalarValueParser.h"

#include "subscription/SubscriptionYamlEscape.h"
#include "subscription/SubscriptionYamlQuoteStateMachine.h"

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
    const QString value = text.trimmed();
    if (value.isEmpty() || (value.at(0) != QChar('"') && value.at(0) != QChar('\''))) {
        return value;
    }

    // Where the scalar ends is the shared question, not this function's: the same rule
    // decides where a comment starts and where a flow entry closes, and a value and a
    // comment cut by two different rules is how a name loses its tail. The trim left the
    // closing quote at the end, so the scan has to land on the last character -- a value
    // like `"abc` is unterminated and stays exactly as written. Asked of the state
    // machine, not of the endsWith() pair this used to be: `"a""` ends with a quote and
    // is not a completed scalar.
    const bool doubleQuoted = value.at(0) == QChar('"');
    SubscriptionYamlQuoteStateMachine::Options options;
    options.requireEnd = true;
    const SubscriptionYamlQuoteStateMachine::Scan scan =
        SubscriptionYamlQuoteStateMachine::scanQuotedScalar(value, 0, options);
    if (!scan.ok) {
        return value;
    }

    const QString body = value.mid(1, scan.quoteEndedAt - 1);
    // Per YAML, only double-quoted scalars process backslash escapes; applying
    // the decoder to single-quoted text would corrupt names such as 'C:\folder'.
    return doubleQuoted
        ? SubscriptionYamlEscape::unescape(body).trimmed()
        : unescapeSingleQuotedScalar(body).trimmed();
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
