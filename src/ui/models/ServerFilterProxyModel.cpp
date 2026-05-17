#include "ui/models/ServerFilterProxyModel.h"

#include <QAbstractItemModel>
#include <QRegularExpression>

#include "services/SpeedTestServiceInternal.h"
#include "ui/models/ServerTableModel.h"

namespace {

enum ServerColumn {
    DefaultColumn = 0,
    TypeColumn,
    RemarksColumn,
    AddressColumn,
    SecurityColumn,
    NetworkColumn,
    StreamSecurityColumn,
    TestResultColumn
};

QString normalizeSortText(QString value)
{
    value = value.trimmed().toLower();
    return value;
}

int compareText(const QString& left, const QString& right)
{
    const QString normalizedLeft = normalizeSortText(left);
    const QString normalizedRight = normalizeSortText(right);
    if (normalizedLeft.isEmpty() != normalizedRight.isEmpty()) {
        return normalizedLeft.isEmpty() ? 1 : -1;
    }
    if (normalizedLeft < normalizedRight) {
        return -1;
    }
    if (normalizedRight < normalizedLeft) {
        return 1;
    }

    return 0;
}

template<typename T>
int compareValues(const T& left, const T& right)
{
    if (left < right) {
        return -1;
    }
    if (right < left) {
        return 1;
    }

    return 0;
}

bool tryParseTestResultMetric(const QString& value, QString& family, double& numericValue)
{
    const QString normalized = normalizeSortText(value);
    if (normalized.isEmpty()) {
        return false;
    }

    double parsedLatencyMs = 0.0;
    if (SpeedTestServiceInternal::tryParseUrlProbeLatency(normalized, parsedLatencyMs)) {
        family = QStringLiteral("latency");
        numericValue = parsedLatencyMs;
        return true;
    }

    static const QRegularExpression expression(
        QStringLiteral("^([+-]?\\d+(?:\\.\\d+)?)\\s*([a-zA-Z/]+)$"));
    const QRegularExpressionMatch match = expression.match(normalized);
    if (!match.hasMatch()) {
        return false;
    }

    bool numberOk = false;
    const double rawNumber = match.captured(1).toDouble(&numberOk);
    if (!numberOk) {
        return false;
    }

    const QString unit = match.captured(2);

    const QHash<QString, double> speedUnitMultipliers{
        {QStringLiteral("b/s"), 1.0},
        {QStringLiteral("kb/s"), 1024.0},
        {QStringLiteral("mb/s"), 1024.0 * 1024.0},
        {QStringLiteral("gb/s"), 1024.0 * 1024.0 * 1024.0}
    };
    const auto multiplierIt = speedUnitMultipliers.constFind(unit);
    if (multiplierIt != speedUnitMultipliers.constEnd()) {
        family = QStringLiteral("speed");
        numericValue = rawNumber * multiplierIt.value();
        return true;
    }

    return false;
}

int compareTestResults(const QString& left, const QString& right)
{
    QString leftFamily;
    QString rightFamily;
    double leftValue = 0.0;
    double rightValue = 0.0;
    const bool leftParsed = tryParseTestResultMetric(left, leftFamily, leftValue);
    const bool rightParsed = tryParseTestResultMetric(right, rightFamily, rightValue);

    if (leftParsed && rightParsed && leftFamily == rightFamily) {
        return compareValues(leftValue, rightValue);
    }

    if (leftParsed != rightParsed) {
        return leftParsed ? -1 : 1;
    }

    return compareText(left, right);
}

} // namespace

ServerFilterProxyModel::ServerFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void ServerFilterProxyModel::setKnownSubscriptionIds(const QSet<QString>& subscriptionIds)
{
    if (knownSubscriptionIds_ == subscriptionIds) {
        return;
    }

    knownSubscriptionIds_ = subscriptionIds;
    invalidateFilter();
}

void ServerFilterProxyModel::setSubscriptionFilterMode(SubscriptionFilterMode mode, QString subscriptionId)
{
    subscriptionId = subscriptionId.trimmed();
    if (subscriptionFilterMode_ == mode && subscriptionId_ == subscriptionId) {
        return;
    }

    subscriptionFilterMode_ = mode;
    subscriptionId_ = std::move(subscriptionId);
    invalidateFilter();
}

ServerFilterProxyModel::SubscriptionFilterMode ServerFilterProxyModel::subscriptionFilterMode() const
{
    return subscriptionFilterMode_;
}

QString ServerFilterProxyModel::subscriptionId() const
{
    return subscriptionId_;
}

QVariant ServerFilterProxyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    if (role == Qt::DisplayRole && index.column() == DefaultColumn) {
        const QModelIndex sourceIndex = mapToSource(index);
        if (!sourceIndex.isValid()) {
            return {};
        }

        const auto* tableModel = qobject_cast<const ServerTableModel*>(sourceModel());
        const ServerTableRow* item = tableModel == nullptr ? nullptr : tableModel->itemAt(sourceIndex.row());
        if (tableModel != nullptr && item != nullptr
            && !tableModel->currentIndexId().isEmpty()
            && item->indexId == tableModel->currentIndexId()) {
            return QStringLiteral(">");
        }

        return QVariant(QString::number(index.row() + 1));
    }

    return QSortFilterProxyModel::data(index, role);
}

bool ServerFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto* tableModel = qobject_cast<const ServerTableModel*>(sourceModel());
    const ServerTableRow* item = tableModel == nullptr ? nullptr : tableModel->itemAt(sourceRow);
    if (item == nullptr) {
        return false;
    }

    if (!matchesSubscriptionFilter(item->subId)) {
        return false;
    }

    return matchesTextFilter(sourceRow, sourceParent);
}

bool ServerFilterProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
    const auto* tableModel = qobject_cast<const ServerTableModel*>(sourceModel());
    const ServerTableRow* leftItem = tableModel == nullptr ? nullptr : tableModel->itemAt(sourceLeft.row());
    const ServerTableRow* rightItem = tableModel == nullptr ? nullptr : tableModel->itemAt(sourceRight.row());
    if (tableModel == nullptr || leftItem == nullptr || rightItem == nullptr) {
        return QSortFilterProxyModel::lessThan(sourceLeft, sourceRight);
    }

    int comparison = 0;
    switch (sourceLeft.column()) {
    case DefaultColumn:
        comparison = compareValues(sourceLeft.row(), sourceRight.row());
        break;
    case TypeColumn:
        comparison = compareText(configTypeDisplayName(leftItem->configType), configTypeDisplayName(rightItem->configType));
        break;
    case RemarksColumn:
        comparison = compareText(leftItem->remarks, rightItem->remarks);
        break;
    case AddressColumn:
        comparison = compareText(leftItem->address, rightItem->address);
        if (comparison == 0) {
            comparison = compareValues(leftItem->port, rightItem->port);
        }
        break;
    case SecurityColumn:
        comparison = compareText(leftItem->security, rightItem->security);
        break;
    case NetworkColumn:
        comparison = compareText(leftItem->network, rightItem->network);
        break;
    case StreamSecurityColumn:
        comparison = compareText(leftItem->streamSecurity, rightItem->streamSecurity);
        break;
    case TestResultColumn:
        comparison = compareTestResults(leftItem->testResult, rightItem->testResult);
        break;
    default:
        comparison = compareText(sourceLeft.data(Qt::DisplayRole).toString(), sourceRight.data(Qt::DisplayRole).toString());
        break;
    }

    if (comparison == 0) {
        comparison = compareValues(leftItem->sort, rightItem->sort);
    }
    if (comparison == 0) {
        comparison = compareText(leftItem->indexId, rightItem->indexId);
    }

    return comparison < 0;
}

bool ServerFilterProxyModel::matchesSubscriptionFilter(const QString& subscriptionId) const
{
    const QString trimmedSubscriptionId = subscriptionId.trimmed();

    switch (subscriptionFilterMode_) {
    case SubscriptionFilterMode::Ungrouped:
        return trimmedSubscriptionId.isEmpty() || !knownSubscriptionIds_.contains(trimmedSubscriptionId);
    case SubscriptionFilterMode::Subscription:
        return !subscriptionId_.isEmpty() && trimmedSubscriptionId == subscriptionId_;
    default:
        return true;
    }
}

bool ServerFilterProxyModel::matchesTextFilter(int sourceRow, const QModelIndex& sourceParent) const
{
    const QRegularExpression expression = filterRegularExpression();
    if (expression.pattern().trimmed().isEmpty()) {
        return true;
    }

    if (sourceModel() == nullptr) {
        return false;
    }

    for (int column = 0; column < sourceModel()->columnCount(sourceParent); ++column) {
        const QString text = sourceModel()->index(sourceRow, column, sourceParent).data(Qt::DisplayRole).toString();
        if (expression.match(text).hasMatch()) {
            return true;
        }
    }

    return false;
}
