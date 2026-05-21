#pragma once

#include <QList>
#include <QWidget>

#include "domain/models/SubItem.h"

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

    static QString resolveUserAgent(const QString& storedValue);

signals:
    void updateRequested();

private:
    void reloadTable();
    void appendRow(const SubItem& item);
    QString userAgentAtRow(int row) const;
    void updateActionState();

    QList<SubItem> items_;
    QTableWidget* table_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;
    QPushButton* updateButton_ = nullptr;
};
