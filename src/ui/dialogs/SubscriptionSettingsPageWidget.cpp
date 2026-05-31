#include "ui/dialogs/SubscriptionSettingsPageWidget.h"

#include <algorithm>

#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

#include "common/UserAgent.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int kEnabledColumn = 0;
constexpr int kUrlColumn = 1;
constexpr int kRemarksColumn = 2;
constexpr int kUserAgentColumn = 3;
const QString kLegacyUaNekobox = QStringLiteral("Nekobox");
const QString kLegacyUaClashVerge = QStringLiteral("ClashVerge");

int tableRowHeight(const QWidget* widget)
{
    return widget == nullptr ? 0 : widget->fontMetrics().height() + 8;
}

void enableCustomUserAgentInput(QComboBox* combo, const QString& text = QString())
{
    if (combo == nullptr) {
        return;
    }

    auto* lineEdit = new QLineEdit(combo);
    lineEdit->setObjectName(QStringLiteral("uaComboLineEdit"));
    lineEdit->setText(text);
    lineEdit->setFrame(false);
    combo->setLineEdit(lineEdit);
}

QComboBox* createUserAgentCombo(QWidget* parent, const QString& storedValue)
{
    auto* combo = new QComboBox(parent);
    combo->setObjectName(QStringLiteral("uaCombo"));
    combo->setFrame(false);
    combo->addItem(QString());
    for (const QString& label : subscriptionUserAgentPresetLabels()) {
        combo->addItem(label);
    }

    const QString trimmed = storedValue.trimmed();
    if (trimmed.isEmpty()) {
        combo->setCurrentIndex(0);
        enableCustomUserAgentInput(combo);
    } else if (trimmed == kLegacyUaNekobox) {
        combo->setCurrentIndex(combo->findText(QStringLiteral("nekobox")));
    } else if (trimmed == kLegacyUaClashVerge) {
        combo->setCurrentIndex(combo->findText(QStringLiteral("clash verge rev")));
    } else if (combo->findText(trimmed) >= 0) {
        combo->setCurrentIndex(combo->findText(trimmed));
    } else {
        combo->setCurrentIndex(0);
        enableCustomUserAgentInput(combo, trimmed);
    }

    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), combo, [combo](int index) {
        if (index == 0) {
            enableCustomUserAgentInput(combo);
        } else {
            combo->setEditable(false);
        }
    });

    return combo;
}

QString subscriptionDomainFromUrl(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QUrl url(trimmed);
    if (url.host().trimmed().isEmpty() && !trimmed.contains(QStringLiteral("://"))) {
        url = QUrl(QStringLiteral("https://") + trimmed);
    }

    return url.host().trimmed();
}

} // namespace

SubscriptionSettingsPageWidget::SubscriptionSettingsPageWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({
        tr("Enabled"),
        QStringLiteral("URL"),
        tr("Remarks"),
        tr("User Agent")
    });
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->verticalHeader()->setVisible(false);
    AppTheme::applyCompactFont(table_);
    AppTheme::applyCompactFont(table_->horizontalHeader());
    const int rowHeight = tableRowHeight(table_);
    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table_->verticalHeader()->setDefaultSectionSize(rowHeight);
    table_->verticalHeader()->setMinimumSectionSize(rowHeight);
    table_->horizontalHeader()->setSectionResizeMode(kEnabledColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kUrlColumn, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(kRemarksColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kUserAgentColumn, QHeaderView::ResizeToContents);
    const int headerHeight = table_->horizontalHeader()->fontMetrics().height() + 8;
    table_->horizontalHeader()->setDefaultSectionSize(headerHeight);
    table_->horizontalHeader()->setMinimumSectionSize(headerHeight);
    AppTheme::applyServerTableStyle(table_);

    addButton_ = new QPushButton(tr("Add"), this);
    removeButton_ = new QPushButton(tr("Remove"), this);
    updateButton_ = new QPushButton(tr("Update Selected"), this);
    AppTheme::applyCompactFont({addButton_, removeButton_, updateButton_});

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(addButton_);
    buttonLayout->addWidget(removeButton_);
    buttonLayout->addWidget(updateButton_);
    buttonLayout->addStretch(1);

    layout->addLayout(buttonLayout);
    layout->addWidget(table_);

    connect(addButton_, &QPushButton::clicked, this, [this]() {
        appendRow(SubItem{
            QString(),
            QString(),
            QStringLiteral("https://"),
            true,
            QString()});
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

QString SubscriptionSettingsPageWidget::resolveUserAgent(const QString& storedValue)
{
    return resolveSubscriptionUserAgent(storedValue);
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
        auto* enabledItem = table_->item(row, kEnabledColumn);
        auto* urlItem = table_->item(row, kUrlColumn);
        auto* remarksItem = table_->item(row, kRemarksColumn);

        SubItem item;
        if (remarksItem != nullptr) {
            item.id = remarksItem->data(Qt::UserRole).toString();
            item.remarks = remarksItem->text().trimmed();
        }
        item.enabled = enabledItem == nullptr || enabledItem->checkState() == Qt::Checked;
        item.url = urlItem == nullptr ? QString() : urlItem->text().trimmed();
        const QString subscriptionDomain = subscriptionDomainFromUrl(item.url);
        if (subscriptionDomain.isEmpty()) {
            continue;
        }
        if (item.remarks.isEmpty()) {
            item.remarks = subscriptionDomain;
        }
        item.userAgent = userAgentAtRow(row);

        items.append(item);
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
    auto* userAgentItem = new QTableWidgetItem();

    table_->setItem(row, kEnabledColumn, enabledItem);
    table_->setItem(row, kUrlColumn, urlItem);
    table_->setItem(row, kRemarksColumn, remarksItem);
    table_->setItem(row, kUserAgentColumn, userAgentItem);
    table_->setCellWidget(row, kUserAgentColumn, createUserAgentCombo(table_, item.userAgent));
}

QString SubscriptionSettingsPageWidget::userAgentAtRow(int row) const
{
    if (table_ == nullptr || row < 0 || row >= table_->rowCount()) {
        return {};
    }

    if (auto* combo = qobject_cast<QComboBox*>(table_->cellWidget(row, kUserAgentColumn))) {
        return combo->currentText().trimmed();
    }
    if (auto* wrapper = table_->cellWidget(row, kUserAgentColumn)) {
        if (auto* combo = wrapper->findChild<QComboBox*>(QStringLiteral("uaCombo"))) {
            return combo->currentText().trimmed();
        }
    }

    auto* userAgentItem = table_->item(row, kUserAgentColumn);
    return userAgentItem == nullptr ? QString() : userAgentItem->text().trimmed();
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
