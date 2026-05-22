#include "subscription/SubscriptionImportTextParser.h"

#include <QRegularExpression>
#include <QUrl>

QStringList SubscriptionImportTextParser::nonEmptyLines(const QString& text)
{
    QStringList result;
    const QStringList rawParts = text
        .trimmed()
        .replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
        .replace(QChar('\r'), QChar('\n'))
        .split(QChar('\n'), Qt::SkipEmptyParts);

    for (const QString& rawPart : rawParts) {
        const QString line = rawPart.trimmed();
        if (!line.isEmpty()) {
            result.append(line);
        }
    }

    return result;
}

bool SubscriptionImportTextParser::isSubscriptionUrl(const QString& text)
{
    const QString value = text.trimmed();
    return value.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || value.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

QString SubscriptionImportTextParser::extractHostOrIpFromUrl(const QString& url)
{
    const QString normalizedUrl = url.trimmed();
    if (normalizedUrl.isEmpty()) {
        return {};
    }

    const QUrl parsedUrl = QUrl::fromUserInput(normalizedUrl);
    if (parsedUrl.isValid()
        && (parsedUrl.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
            || parsedUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)) {
        const QString host = parsedUrl.host().trimmed();
        if (!host.isEmpty()) {
            return host;
        }
    }

    static const QRegularExpression hostPattern(
        QStringLiteral(R"(^https?://(?<host>[^/?#]+))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = hostPattern.match(normalizedUrl);
    if (!match.hasMatch()) {
        return {};
    }

    QString host = match.captured(QStringLiteral("host")).trimmed();
    const int atIndex = host.lastIndexOf(QChar('@'));
    if (atIndex >= 0) {
        host = host.mid(atIndex + 1);
    }

    if (host.startsWith(QChar('['))) {
        const int closingIndex = host.indexOf(QChar(']'));
        if (closingIndex > 0) {
            return host.mid(1, closingIndex - 1).trimmed();
        }
    }

    const int colonIndex = host.lastIndexOf(QChar(':'));
    if (colonIndex > 0) {
        host = host.left(colonIndex);
    }

    return host.trimmed();
}
