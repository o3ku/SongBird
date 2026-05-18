#include "ui/mainwindow/ServerSelectionController.h"

#include <QItemSelectionModel>
#include <QSet>
#include <QTableView>

#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"

ServerSelectionController::ServerSelectionController(
    QTableView* serverView,
    ServerFilterProxyModel* serverFilterModel,
    ServerTableModel* serverModel,
    std::function<QString()> currentIndexId,
    std::function<void()> refreshSelectionUi)
    : serverView_(serverView)
    , serverFilterModel_(serverFilterModel)
    , serverModel_(serverModel)
    , currentIndexId_(std::move(currentIndexId))
    , refreshSelectionUi_(std::move(refreshSelectionUi))
{
}

void ServerSelectionController::setup()
{
    if (serverView_ == nullptr || serverView_->selectionModel() == nullptr) {
        return;
    }

    QObject::connect(
        serverView_->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        serverView_,
        [this](const QModelIndex&, const QModelIndex&) {
            handleSelectionChanged();
        });
    QObject::connect(
        serverView_->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        serverView_,
        [this](const QItemSelection&, const QItemSelection&) {
            handleSelectionChanged();
        });
}

void ServerSelectionController::setPreferredSelectionId(const QString& indexId)
{
    preferredSelectionId_ = indexId.trimmed();
}

QString ServerSelectionController::preferredSelectionId() const
{
    return preferredSelectionId_;
}

void ServerSelectionController::updateSelectionForVisibleRows()
{
    if (serverView_ == nullptr
        || serverView_->selectionModel() == nullptr
        || serverFilterModel_ == nullptr
        || serverModel_ == nullptr) {
        return;
    }

    QModelIndex targetIndex;
    const QStringList preferredIds{
        preferredSelectionId_.trimmed(),
        currentIndexId_ ? currentIndexId_().trimmed() : QString()};

    for (const QString& preferredId : preferredIds) {
        if (preferredId.isEmpty()) {
            continue;
        }

        for (int proxyRow = 0; proxyRow < serverFilterModel_->rowCount(); ++proxyRow) {
            const QModelIndex proxyIndex = serverFilterModel_->index(proxyRow, 0);
            const QModelIndex sourceIndex = serverFilterModel_->mapToSource(proxyIndex);
            const ServerTableRow* item = serverModel_->itemAt(sourceIndex.row());
            if (item != nullptr && item->indexId == preferredId) {
                targetIndex = proxyIndex;
                break;
            }
        }

        if (targetIndex.isValid()) {
            break;
        }
    }

    if (!targetIndex.isValid() && serverFilterModel_->rowCount() > 0) {
        targetIndex = serverFilterModel_->index(0, 0);
    }

    serverView_->clearSelection();
    if (!targetIndex.isValid()) {
        serverView_->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
        setPreferredSelectionId(QString());
        refreshSelectionUi_();
        return;
    }

    serverView_->setCurrentIndex(targetIndex);
    serverView_->selectRow(targetIndex.row());
}

QList<const ServerTableRow*> ServerSelectionController::selectedServers() const
{
    QList<const ServerTableRow*> items;
    if (serverView_ == nullptr
        || serverView_->selectionModel() == nullptr
        || serverFilterModel_ == nullptr
        || serverModel_ == nullptr) {
        return items;
    }

    QSet<int> seenRows;
    const QModelIndexList rows = serverView_->selectionModel()->selectedRows();
    for (const QModelIndex& proxyRow : rows) {
        const QModelIndex sourceRow = serverFilterModel_->mapToSource(proxyRow);
        if (!sourceRow.isValid() || seenRows.contains(sourceRow.row())) {
            continue;
        }

        seenRows.insert(sourceRow.row());
        const ServerTableRow* item = serverModel_->itemAt(sourceRow.row());
        if (item != nullptr) {
            items.append(item);
        }
    }

    return items;
}

const ServerTableRow* ServerSelectionController::selectedServer() const
{
    const QList<const ServerTableRow*> items = selectedServers();
    return items.isEmpty() ? nullptr : items.constFirst();
}

QString ServerSelectionController::selectedServerId() const
{
    const ServerTableRow* item = selectedServer();
    return item == nullptr ? QString() : item->indexId;
}

QStringList ServerSelectionController::selectedServerIds() const
{
    QStringList ids;
    const QList<const ServerTableRow*> items = selectedServers();
    for (const ServerTableRow* item : items) {
        if (item != nullptr && !item->indexId.isEmpty()) {
            ids.append(item->indexId);
        }
    }

    ids.removeDuplicates();
    return ids;
}

QStringList ServerSelectionController::selectedShareLinks(const QHash<QString, QString>& shareUrlsByIndexId) const
{
    QStringList shareLinks;
    const QList<const ServerTableRow*> items = selectedServers();
    for (const ServerTableRow* item : items) {
        if (item == nullptr) {
            continue;
        }

        const QString shareUrl = shareUrlsByIndexId.value(item->indexId).trimmed();
        if (!shareUrl.isEmpty()) {
            shareLinks.append(shareUrl);
        }
    }

    return shareLinks;
}

void ServerSelectionController::handleSelectionChanged()
{
    setPreferredSelectionId(selectedServerId());
    refreshSelectionUi_();
}
