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
    // Location-grouping state, resolved from the same VmessItem fields the catalog reads. The
    // override is carried raw (the catalog normalizes at read time) so a row is a faithful copy
    // of the item, and favorite drives both the filter and the star decoration.
    QString locationOverride;
    bool favorite = false;
};

class ServerTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Role {
        IsCurrentServerRole = Qt::UserRole + 1
    };

    // Column layout of the server table. Public so callers (including tests) can
    // name a column instead of hard-coding its index when the order changes.
    enum Column {
        DefaultColumn = 0,
        TypeColumn,
        RemarksColumn,
        AddressColumn,
        TestResultColumn,
        ColumnCount
    };

    explicit ServerTableModel(QObject* parent = nullptr);

    void setItems(const QList<VmessItem>& items, QString currentIndexId = {});
    // Moves the active-row mark without a model reset: a group session's in-core member
    // changes while the server list itself does not, and re-running setItems would cost the
    // view its selection and scroll position for a one-row highlight change.
    void setActiveIndexId(const QString& indexId);
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
    QHash<QString, int> rowByIndexId_;
    QList<ServerTableRow> items_;
    QString currentIndexId_;
    mutable QIcon cachedDefaultRowIcon_;
};
