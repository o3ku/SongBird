#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

struct ServerTableRow {
    QString indexId;
    ConfigType configType = ConfigType::Unknown;
    QString displayName;
    QString address;
    int port = 0;
    QString testResult;
    QString subId;
};

class ServerTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Role {
        IsCurrentServerRole = Qt::UserRole + 1
    };

    explicit ServerTableModel(QObject* parent = nullptr);

    void setItems(const QList<VmessItem>& items, QString currentIndexId = {});
    bool updateTestResult(const QString& indexId, const QString& result);
    const ServerTableRow* itemAt(int row) const;
    const ServerTableRow* itemByIndexId(const QString& indexId) const;
    const ServerTableRow* currentItem() const;
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
        TestResultColumn,
        ColumnCount
    };

    QHash<QString, int> rowByIndexId_;
    QList<ServerTableRow> items_;
    QString currentIndexId_;
    mutable QIcon cachedDefaultRowIcon_;
};
