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

} // namespace SubscriptionImportFailure
