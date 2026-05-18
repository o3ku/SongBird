#include "ui/dialogs/SubscriptionSettingsPageWidget.h"

#include <algorithm>

#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "ui/theme/AppTheme.h"

SubscriptionSettingsPageWidget::SubscriptionSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("Enabled"),
        QStringLiteral("Remarks"),
        QStringLiteral("URL"),
        QStringLiteral("User Agent")
    });
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    AppTheme::applyServerTableStyle(table_);
    AppTheme::applyCompactFont(table_);
    AppTheme::applyCompactFont(table_->horizontalHeader());

    addButton_ = new QPushButton(QStringLiteral("Add"), this);
    removeButton_ = new QPushButton(QStringLiteral("Remove"), this);
    updateButton_ = new QPushButton(QStringLiteral("Update Selected"), this);
    AppTheme::applyCompactFont({addButton_, removeButton_, updateButton_});

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(addButton_);
    buttonLayout->addWidget(removeButton_);
    buttonLayout->addWidget(updateButton_);
    buttonLayout->addStretch(1);

    layout->addLayout(buttonLayout);
    layout->addWidget(table_);

    connect(addButton_, &QPushButton::clicked, this, [this]() {
        appendRow(SubItem{QString(), QStringLiteral("Subscription"), QStringLiteral("https://"), true, QString()});
        updateActionState();
    });
    connect(removeButton_, &QPushButton::clicked, this, [this]() {
        QModelIndexList rows = table_->selectionModel() == nullptr
            ? QModelIndexList()
            : table_->selectionModel()->selectedRows();
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
            return lhs.row() > rhs.row();
        });
        for (const QModelIndex& row : rows) {
            table_->removeRow(row.row());
        }
        updateActionState();
    });
    connect(updateButton_, &QPushButton::clicked, this, &SubscriptionSettingsPageWidget::updateRequested);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateActionState();
    });
}

void SubscriptionSettingsPageWidget::setSubscriptions(const QList<SubItem>& items)
{
    items_ = items;
    reloadTable();
}

QList<SubItem> SubscriptionSettingsPageWidget::subscriptions() const
{
    QList<SubItem> items;
    const int rowCount = table_ == nullptr ? 0 : table_->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        auto* enabledItem = table_->item(row, 0);
        auto* remarksItem = table_->item(row, 1);
        auto* urlItem = table_->item(row, 2);
        auto* userAgentItem = table_->item(row, 3);

        SubItem item;
        if (remarksItem != nullptr) {
            item.id = remarksItem->data(Qt::UserRole).toString();
            item.remarks = remarksItem->text().trimmed();
        }
        item.enabled = enabledItem == nullptr || enabledItem->checkState() == Qt::Checked;
        item.url = urlItem == nullptr ? QString() : urlItem->text().trimmed();
        item.userAgent = userAgentItem == nullptr ? QString() : userAgentItem->text().trimmed();

        if (!item.remarks.isEmpty() || !item.url.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

QList<int> SubscriptionSettingsPageWidget::selectedRows() const
{
    QList<int> rows;
    if (table_ == nullptr || table_->selectionModel() == nullptr) {
        return rows;
    }

    const QModelIndexList indexes = table_->selectionModel()->selectedRows();
    for (const QModelIndex& index : indexes) {
        rows.append(index.row());
    }

    std::sort(rows.begin(), rows.end());
    return rows;
}

void SubscriptionSettingsPageWidget::reloadTable()
{
    const QSignalBlocker blocker(table_);
    table_->setRowCount(0);

    for (const SubItem& item : items_) {
        appendRow(item);
    }

    updateActionState();
}

void SubscriptionSettingsPageWidget::appendRow(const SubItem& item)
{
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto* enabledItem = new QTableWidgetItem();
    enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
    enabledItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);

    auto* remarksItem = new QTableWidgetItem(item.remarks);
    remarksItem->setData(Qt::UserRole, item.id);

    auto* urlItem = new QTableWidgetItem(item.url);
    auto* userAgentItem = new QTableWidgetItem(item.userAgent);

    table_->setItem(row, 0, enabledItem);
    table_->setItem(row, 1, remarksItem);
    table_->setItem(row, 2, urlItem);
    table_->setItem(row, 3, userAgentItem);
}

void SubscriptionSettingsPageWidget::updateActionState()
{
    const bool hasSelection = !selectedRows().isEmpty();
    if (removeButton_ != nullptr) {
        removeButton_->setEnabled(hasSelection);
    }
    if (updateButton_ != nullptr) {
        updateButton_->setEnabled(hasSelection);
    }
}
