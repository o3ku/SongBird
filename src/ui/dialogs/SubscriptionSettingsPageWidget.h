#pragma once

#include <QList>
#include <QWidget>

#include "common/SubscriptionOrder.h"
#include "domain/models/SubItem.h"

class QAction;
class QPoint;
class QPushButton;
class QTableWidget;

class SubscriptionSettingsPageWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SubscriptionSettingsPageWidget(QWidget* parent = nullptr);

    void setSubscriptions(const QList<SubItem>& items);
    QList<SubItem> subscriptions() const;
    QList<int> selectedRows() const;
    // What "Update Selected" means, captured when the button is pressed. A row number is not an
    // identity: the table is reorderable, so by the time the dialog is accepted the same number can
    // address a different subscription. Ids survive the move, and a row added here has none yet --
    // which is the correct answer, since it has nothing to update.
    QStringList selectedSubscriptionIds() const;

    static QString resolveUserAgent(const QString& storedValue);

signals:
    void updateRequested();

private:
    void reloadTable();
    void appendRow(const SubItem& item);
    // Copies the fields the table does not show (HTTP cache validators and the failure streak) from
    // the subscription this row was loaded from.
    void carryServerState(const QString& id, SubItem& item) const;
    QString userAgentAtRow(int row) const;
    QString subscriptionIdAtRow(int row) const;
    // Rearranges the row widgets in place: `newOrder` lists the current rows in the order they
    // should end up in.
    void moveRowsToOrder(const QList<int>& newOrder);
    void updateActionState();
    // Reorders the rows the table owns. The table is the live edit buffer for the dialog, so
    // moving a row here rearranges the persisted order only when the dialog is accepted; a cancel
    // leaves the saved list untouched. Hidden rows are ordinary rows here -- this page is the only
    // place a subscription without a tab can be placed, so nothing about the move is conditional on
    // the Enabled checkbox.
    void moveSelectedRows(SubscriptionOrder::MoveOperation operation);
    void showTableContextMenu(const QPoint& position);

    QList<SubItem> items_;
    QTableWidget* table_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* updateButton_ = nullptr;
    QAction* moveTopAction_ = nullptr;
    QAction* moveUpAction_ = nullptr;
    QAction* moveDownAction_ = nullptr;
    QAction* moveBottomAction_ = nullptr;
};
