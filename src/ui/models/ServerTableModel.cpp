#include "ui/models/ServerTableModel.h"

#include <QApplication>
#include <QBrush>
#include <QFont>
#include <utility>

namespace {

QString formatAddressDisplay(const VmessItem& item)
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

void ServerTableModel::setItems(QList<VmessItem> items, QList<ServerStatItem> statistics, QString currentIndexId)
{
    beginResetModel();
    items_ = std::move(items);
    statistics_.clear();
    for (const ServerStatItem& item : statistics) {
        if (!item.itemId.trimmed().isEmpty()) {
            statistics_.insert(item.itemId, item);
        }
    }
    currentIndexId_ = std::move(currentIndexId);
    endResetModel();
}

const VmessItem* ServerTableModel::itemAt(int row) const
{
    if (row < 0 || row >= items_.size()) {
        return nullptr;
    }

    return &items_.at(row);
}

ServerStatItem ServerTableModel::statisticAt(int row) const
{
    const VmessItem* item = itemAt(row);
    if (item == nullptr) {
        return {};
    }

    return statistics_.value(item->indexId);
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

    const VmessItem* item = itemAt(index.row());
    if (item == nullptr) {
        return {};
    }
    const bool isDefault = !currentIndexId_.isEmpty() && item->indexId == currentIndexId_;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case DefaultColumn:
            return QString::number(index.row() + 1);
        case TypeColumn:
            return configTypeDisplayName(item->configType);
        case RemarksColumn:
            return item->remarks;
        case AddressColumn:
            return formatAddressDisplay(*item);
        case SecurityColumn:
            return item->security;
        case NetworkColumn:
            return item->network;
        case StreamSecurityColumn:
            return item->streamSecurity;
        case TestResultColumn:
            return item->testResult;
        default:
            return {};
        }
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
        return QBrush(QColor(QStringLiteral("#2f6fce")));
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
    case SecurityColumn:
        return tr("Security");
    case NetworkColumn:
        return tr("Network");
    case StreamSecurityColumn:
        return tr("TLS");
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
