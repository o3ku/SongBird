#include "app/SubscriptionImportFailure.h"

#include <QRegularExpression>

namespace SubscriptionImportFailure {

bool shouldExplainInEmptyServerPanel(const OperationResult& result)
{
    if (result.success || result.cancelled) {
        return false;
    }

    const QString message = result.message.trimmed();
    if (message.isEmpty()) {
        return false;
    }

    // "Updated 1 of 2 subscription(s), imported 12 server(s)." is how a batch
    // reports that one subscription failed while the others came back -- those
    // servers are on the table, so the panel is not showing at all.
    static const QRegularExpression importedCount(QStringLiteral("imported (\\d+) server"));
    const QRegularExpressionMatch match = importedCount.match(message);
    if (!match.hasMatch()) {
        return true;
    }

    return match.captured(1).toInt() == 0;
}

QString reasonForEmptyServerPanel(const QString& message)
{
    const QStringList lines = message.split(QChar('\n'), Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return message.trimmed();
    }

    // The summary line counts subscriptions; the failure lines say what happened.
    // Showing the first of those is what turns the panel from "import failed" into
    // "your nodes are protocols this build cannot read".
    static const QRegularExpression updatesSummary(QStringLiteral("^Updated \\d+ of \\d+ subscription"));
    if (!updatesSummary.match(lines.constFirst()).hasMatch()) {
        return message.trimmed();
    }

    for (const QString& line : lines) {
        const QString reason = line.trimmed();
        if (reason.startsWith(QStringLiteral("[FAIL] "))) {
            return reason.mid(7).trimmed();
        }
    }

    return message.trimmed();
}

} // namespace SubscriptionImportFailure
