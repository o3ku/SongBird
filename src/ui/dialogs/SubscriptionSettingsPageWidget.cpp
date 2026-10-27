#include "ui/dialogs/SubscriptionSettingsPageWidget.h"

#include <algorithm>

#include <QAction>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QComboBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QKeySequence>
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

    // Order actions are shared by the right-click menu and, through their shortcuts, the table
    // itself: the server table binds T/B/U/D for the same four moves, and a subscription row is
    // still a row in a list.
    const auto createMoveAction = [this](
                                      const QString& text,
                                      const QString& objectName,
                                      SubscriptionOrder::MoveOperation operation,
                                      const QList<QKeySequence>& shortcuts) {
        auto* action = new QAction(text, this);
        action->setObjectName(objectName);
        action->setShortcuts(shortcuts);
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(action, &QAction::triggered, this, [this, operation]() {
            moveSelectedRows(operation);
        });
        table_->addAction(action);
        return action;
    };
    moveTopAction_ = createMoveAction(
        tr("Move To Top"), QStringLiteral("moveSubscriptionTopAction"),
        SubscriptionOrder::MoveOperation::Top, {QKeySequence(Qt::Key_T)});
    moveUpAction_ = createMoveAction(
        tr("Move Up"), QStringLiteral("moveSubscriptionUpAction"),
        SubscriptionOrder::MoveOperation::Up, {QKeySequence(Qt::Key_U)});
    moveDownAction_ = createMoveAction(
        tr("Move Down"), QStringLiteral("moveSubscriptionDownAction"),
        SubscriptionOrder::MoveOperation::Down, {QKeySequence(Qt::Key_D)});
    moveBottomAction_ = createMoveAction(
        tr("Move To Bottom"), QStringLiteral("moveSubscriptionBottomAction"),
        SubscriptionOrder::MoveOperation::Bottom, {QKeySequence(Qt::Key_B)});

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        showTableContextMenu(position);
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
        // This table edits five columns; the rest of a SubItem is server state the page never
        // showed. Rebuilding the struct from the table alone reset `etag`/`lastModified` (forcing a
        // full download on the next refresh) and `failStreak` (losing the back-off) every time the
        // dialog was opened and confirmed, even with nothing changed.
        carryServerState(item.id, item);

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

void SubscriptionSettingsPageWidget::carryServerState(const QString& id, SubItem& item) const
{
    const QString normalizedId = id.trimmed();
    if (normalizedId.isEmpty()) {
        return;
    }

    const auto stored = std::find_if(
        items_.cbegin(),
        items_.cend(),
        [&normalizedId](const SubItem& candidate) {
            return candidate.id.trimmed() == normalizedId;
        });
    if (stored == items_.cend()) {
        return;
    }

    item.etag = stored->etag;
    item.lastModified = stored->lastModified;
    item.failStreak = stored->failStreak;
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

void SubscriptionSettingsPageWidget::moveSelectedRows(SubscriptionOrder::MoveOperation operation)
{
    if (table_ == nullptr) {
        return;
    }

    // The persisted order lives in the table, and a move is a reordering of its rows -- including
    // rows the user added a moment ago and whose URL is still being typed. Rebuilding the
    // SubItems first would drop those, so the move is expressed as a row permutation.
    const QStringList selectedIds = selectedSubscriptionIds();
    if (selectedIds.isEmpty()) {
        return;
    }

    QList<SubItem> rowIdentities;
    const int rowCount = table_->rowCount();
    rowIdentities.reserve(rowCount);
    for (int row = 0; row < rowCount; ++row) {
        SubItem identity;
        identity.id = subscriptionIdAtRow(row);
        rowIdentities.append(identity);
    }

    if (!SubscriptionOrder::moveSubscriptions(rowIdentities, selectedIds, operation)) {
        return;
    }

    // The identity list now holds the ids in their new order; map that back onto the rows.
    QList<int> newOrder;
    newOrder.reserve(rowIdentities.size());
    for (const SubItem& identity : rowIdentities) {
        for (int row = 0; row < rowCount; ++row) {
            if (subscriptionIdAtRow(row) == identity.id && !newOrder.contains(row)) {
                newOrder.append(row);
                break;
            }
        }
    }

    if (newOrder.size() != rowCount) {
        return;
    }

    const QSignalBlocker blocker(table_);
    moveRowsToOrder(newOrder);
    updateActionState();
}

QStringList SubscriptionSettingsPageWidget::selectedSubscriptionIds() const
{
    QStringList ids;
    for (int row : selectedRows()) {
        const QString id = subscriptionIdAtRow(row);
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}

void SubscriptionSettingsPageWidget::showTableContextMenu(const QPoint& position)
{
    if (table_ == nullptr || table_->selectionModel() == nullptr) {
        return;
    }

    const QModelIndex clickedIndex = table_->indexAt(position);
    if (clickedIndex.isValid()
        && !table_->selectionModel()->isRowSelected(clickedIndex.row(), QModelIndex())) {
        table_->clearSelection();
        table_->selectRow(clickedIndex.row());
        table_->setCurrentIndex(clickedIndex);
    }

    updateActionState();
    if (selectedSubscriptionIds().isEmpty()) {
        return;
    }

    auto* menu = new QMenu(table_);
    menu->setObjectName(QStringLiteral("subscriptionRowContextMenu"));
    menu->addAction(moveUpAction_);
    menu->addAction(moveDownAction_);
    menu->addAction(moveTopAction_);
    menu->addAction(moveBottomAction_);
    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(table_->viewport()->mapToGlobal(position));
}

QString SubscriptionSettingsPageWidget::subscriptionIdAtRow(int row) const
{
    if (table_ == nullptr || row < 0 || row >= table_->rowCount()) {
        return {};
    }

    auto* remarksItem = table_->item(row, kRemarksColumn);
    return remarksItem == nullptr ? QString() : remarksItem->data(Qt::UserRole).toString().trimmed();
}

void SubscriptionSettingsPageWidget::moveRowsToOrder(const QList<int>& newOrder)
{
    // QTableWidget cannot permute rows, and removeRow() deletes the row's cell widget -- the user
    // agent combo. The rows therefore travel through this container: each is taken out as a bundle
    // of items plus its cell widget, and the bundles go back in the new order.
    struct RowBundle {
        QList<QTableWidgetItem*> items;
        QWidget* cellWidget = nullptr;
    };

    const int columnCount = table_->columnCount();
    QList<RowBundle> bundles;
    bundles.reserve(newOrder.size());
    for (int row : newOrder) {
        RowBundle bundle;
        bundle.items.reserve(columnCount);
        for (int column = 0; column < columnCount; ++column) {
            // takeItem() hands ownership over, so the items survive the row's removal.
            bundle.items.append(table_->takeItem(row, column));
        }
        bundle.cellWidget = table_->cellWidget(row, kUserAgentColumn);
        if (bundle.cellWidget != nullptr) {
            // Detached so the bundle owns it: removeRow() destroys whatever the row still points
            // at, and rows shift up as others are removed, so a later row can share the index of a
            // row whose widget is already in a bundle.
            bundle.cellWidget->setParent(nullptr);
        }
        bundles.append(bundle);
    }

    QList<int> rowsToRemove = newOrder;
    std::sort(rowsToRemove.begin(), rowsToRemove.end(), [](int left, int right) { return left > right; });
    for (int row : rowsToRemove) {
        table_->removeRow(row);
    }

    for (int index = 0; index < bundles.size(); ++index) {
        RowBundle& bundle = bundles[index];
        table_->insertRow(index);
        for (int column = 0; column < columnCount; ++column) {
            if (QTableWidgetItem* item = bundle.items.at(column)) {
                table_->setItem(index, column, item);
            }
        }
        if (bundle.cellWidget != nullptr) {
            table_->setCellWidget(index, kUserAgentColumn, bundle.cellWidget);
        }
    }
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

    // The order actions are reachable as shortcuts as well as through the menu, so the edge cases
    // ("nothing to swap with") are disabled here rather than silently doing nothing.
    const bool canMove = hasSelection && table_ != nullptr && table_->rowCount() > 1;
    for (QAction* action : {moveTopAction_, moveUpAction_, moveDownAction_, moveBottomAction_}) {
        if (action != nullptr) {
            action->setEnabled(canMove);
        }
    }
}
