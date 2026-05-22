#include "ui/models/LogFilterProxyModel.h"

LogFilterProxyModel::LogFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void LogFilterProxyModel::setPattern(const QString& pattern, bool regexEnabled)
{
    beginResetModel();
    pattern_ = pattern.trimmed();
    regexEnabled_ = regexEnabled;
    validPattern_ = true;
    expression_ = QRegularExpression();

    if (!pattern_.isEmpty() && regexEnabled_) {
        expression_ = QRegularExpression(
            pattern_,
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        validPattern_ = expression_.isValid();
    }
    endResetModel();
}

bool LogFilterProxyModel::hasValidPattern() const
{
    return validPattern_;
}

bool LogFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (pattern_.isEmpty()) {
        return true;
    }

    if (!validPattern_) {
        return false;
    }

    const QModelIndex sourceIndex = sourceModel() == nullptr
        ? QModelIndex()
        : sourceModel()->index(sourceRow, 0, sourceParent);
    const QString value = sourceIndex.data(Qt::DisplayRole).toString();
    if (regexEnabled_) {
        return expression_.match(value).hasMatch();
    }

    return value.contains(pattern_, Qt::CaseInsensitive);
}
