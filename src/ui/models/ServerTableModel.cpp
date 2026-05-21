#include "ui/models/ServerTableModel.h"

#include <QApplication>
#include <QBrush>
#include <QFont>
#include <QIcon>
#include <utility>

namespace {

QString formatAddressDisplay(const ServerTableRow& item)
{
    if (item.port > 0) {
        return QStringLiteral("%1:%2").arg(item.address).arg(item.port);
    }

    return item.address;
}

} // namespace

ServerTableModel::ServerTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void ServerTableModel::setItems(QList<VmessItem> items, QString currentIndexId)
{
    beginResetModel();
    items_.clear();
    items_.reserve(items.size());
    for (const VmessItem& item : items) {
        items_.append(ServerTableRow{
            item.indexId,
            item.configType,
            item.remarks,
            item.address,
            item.port,
            item.security,
            item.network,
            item.streamSecurity,
            item.testResult,
            item.subId});
    }
    rowByIndexId_.clear();
    rowByIndexId_.reserve(items_.size());
    for (int row = 0; row < items_.size(); ++row) {
        const QString trimmedId = items_.at(row).indexId.trimmed();
        if (!trimmedId.isEmpty()) {
            rowByIndexId_.insert(trimmedId, row);
        }
    }
    currentIndexId_ = std::move(currentIndexId);
    endResetModel();
}

bool ServerTableModel::updateTestResult(const QString& indexId, const QString& result)
{
    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty()) {
        return false;
    }

    const auto rowIt = rowByIndexId_.constFind(trimmedId);
    if (rowIt == rowByIndexId_.constEnd()) {
        return false;
    }

    const int row = rowIt.value();
    if (row < 0 || row >= items_.size()) {
        return false;
    }

    ServerTableRow& item = items_[row];
    const QString trimmedResult = result.trimmed();
    if (item.testResult == trimmedResult) {
        return true;
    }

    item.testResult = trimmedResult;
    const QModelIndex changedIndex = index(row, TestResultColumn);
    emit dataChanged(changedIndex, changedIndex, {Qt::DisplayRole});
    return true;
}

const ServerTableRow* ServerTableModel::itemAt(int row) const
{
    if (row < 0 || row >= items_.size()) {
        return nullptr;
    }

    return &items_.at(row);
}

const ServerTableRow* ServerTableModel::itemByIndexId(const QString& indexId) const
{
    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty()) {
        return nullptr;
    }

    const auto rowIt = rowByIndexId_.constFind(trimmedId);
    if (rowIt == rowByIndexId_.constEnd()) {
        return nullptr;
    }

    return itemAt(rowIt.value());
}

const ServerTableRow* ServerTableModel::currentItem() const
{
    return itemByIndexId(currentIndexId_);
}

QString ServerTableModel::currentIndexId() const
{
    return currentIndexId_;
}

int ServerTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return items_.size();
}

int ServerTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return ColumnCount;
}

QVariant ServerTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const ServerTableRow* item = itemAt(index.row());
    if (item == nullptr) {
        return {};
    }
    const bool isDefault = !currentIndexId_.isEmpty() && item->indexId == currentIndexId_;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case DefaultColumn:
            return isDefault ? QVariant() : QVariant(QString::number(index.row() + 1));
        case TypeColumn:
            return configTypeDisplayName(item->configType);
        case RemarksColumn:
            return item->remarks;
        case AddressColumn:
            return formatAddressDisplay(*item);
        case TestResultColumn:
            return item->testResult;
        default:
            return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == DefaultColumn && isDefault) {
        return QIcon(QStringLiteral(":/app/logo.svg"));
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case DefaultColumn:
            return static_cast<int>(Qt::AlignCenter);
        case TestResultColumn:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (role == Qt::ForegroundRole && isDefault) {
        return QBrush(QColor(QStringLiteral("#007A1F")));
    }

    if (role == IsCurrentServerRole) {
        return isDefault;
    }

    return {};
}

QVariant ServerTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    if (role == Qt::TextAlignmentRole) {
        return static_cast<int>(Qt::AlignCenter);
    }

    if (role == Qt::FontRole) {
        QVariant baseFontData = QAbstractTableModel::headerData(section, orientation, role);
        QFont font = baseFontData.canConvert<QFont>()
            ? qvariant_cast<QFont>(baseFontData)
            : QApplication::font();
        font.setBold(true);
        return font;
    }

    if (role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch (section) {
    case DefaultColumn:
        return QStringLiteral("No.");
    case TypeColumn:
        return tr("Type");
    case RemarksColumn:
        return tr("Alias");
    case AddressColumn:
        return tr("Address");
    case TestResultColumn:
        return tr("Result");
    default:
        return {};
    }
}

Qt::ItemFlags ServerTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (index.isValid()) {
        return flags | Qt::ItemIsDragEnabled;
    }

    return flags | Qt::ItemIsDropEnabled;
}

Qt::DropActions ServerTableModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions ServerTableModel::supportedDropActions() const
{
    return Qt::MoveAction;
}
