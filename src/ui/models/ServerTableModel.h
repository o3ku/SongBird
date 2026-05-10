#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QString>

#include "domain/models/ServerStatistics.h"
#include "domain/models/VmessItem.h"

class ServerTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ServerTableModel(QObject* parent = nullptr);

    void setItems(QList<VmessItem> items, QList<ServerStatItem> statistics, QString currentIndexId = {});
    bool updateTestResult(const QString& indexId, const QString& result);
    const VmessItem* itemAt(int row) const;
    ServerStatItem statisticAt(int row) const;
    QString currentIndexId() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    Qt::DropActions supportedDragActions() const override;
    Qt::DropActions supportedDropActions() const override;

private:
    enum Column {
        DefaultColumn = 0,
        TypeColumn,
        RemarksColumn,
        AddressColumn,
        SecurityColumn,
        NetworkColumn,
        StreamSecurityColumn,
        TestResultColumn,
        ColumnCount
    };

    QList<VmessItem> items_;
    QHash<QString, ServerStatItem> statistics_;
    QString currentIndexId_;
};
