#include "services/CoreUpdateVersion.h"

#include <QList>
#include <QtGlobal>

namespace {

QList<int> parseVersionParts(const QString& version)
{
    QString stripped = CoreUpdateVersion::normalizeTag(version);
    if (stripped.startsWith(QChar('v'))) {
        stripped = stripped.mid(1);
    }
    const int dashIndex = stripped.indexOf(QChar('-'));
    if (dashIndex >= 0) {
        stripped = stripped.left(dashIndex);
    }

    QList<int> parts;
    for (const QString& segment : stripped.split(QChar('.'))) {
        bool ok = false;
        const int value = segment.toInt(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}

} // namespace

namespace CoreUpdateVersion {

QString normalizeTag(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("version "))) {
        value = value.mid(QStringLiteral("version ").size()).trimmed();
    }

    if (!value.isEmpty() && value.front().isDigit()) {
        value.prepend(QChar('v'));
    }

    return value;
}

bool isNewerThan(const QString& candidate, const QString& current)
{
    const QList<int> candidateParts = parseVersionParts(candidate);
    const QList<int> currentParts = parseVersionParts(current);
    const int maxLen = qMax(candidateParts.size(), currentParts.size());

    for (int i = 0; i < maxLen; ++i) {
        const int c = i < candidateParts.size() ? candidateParts[i] : 0;
        const int r = i < currentParts.size() ? currentParts[i] : 0;
        if (c > r) {
            return true;
        }
        if (c < r) {
            return false;
        }
    }
    return false;
}

} // namespace CoreUpdateVersion
