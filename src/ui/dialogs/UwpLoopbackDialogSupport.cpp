#include "ui/dialogs/UwpLoopbackDialogSupport.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "ui/theme/AppTheme.h"

namespace {

QString displayNameForPackage(const WindowsUwpPackageInfo& package)
{
    return package.name.trimmed().isEmpty() ? package.packageFamilyName : package.name.trimmed();
}

int tableRowHeight(const QWidget* widget)
{
    return widget == nullptr ? 0 : widget->fontMetrics().height() + 8;
}

QTableWidgetItem* createReadOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return item;
}

} // namespace

namespace UwpLoopbackDialogSupport {

void configurePackageTable(QTableWidget* table, const QStringList& headerLabels)
{
    if (table == nullptr) {
        return;
    }

    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(headerLabels);
    AppTheme::applyCompactFont({table, table->horizontalHeader()});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setFocusPolicy(Qt::StrongFocus);
    table->setFrameShape(QFrame::NoFrame);
    table->setContentsMargins(0, 0, 0, 0);
    table->verticalHeader()->setVisible(false);

    const int rowHeight = tableRowHeight(table);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(rowHeight);
    table->verticalHeader()->setMinimumSectionSize(rowHeight);
    table->horizontalHeader()->setSectionResizeMode(EnabledColumn, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(AppColumn, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(PackageColumn, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setHighlightSections(false);

    const int headerHeight = table->horizontalHeader()->fontMetrics().height() + 8;
    table->horizontalHeader()->setDefaultSectionSize(headerHeight);
    table->horizontalHeader()->setMinimumSectionSize(headerHeight);
    AppTheme::applyServerTableStyle(table);
}

void populatePackageTable(QTableWidget* table, const QList<WindowsUwpPackageInfo>& packages)
{
    if (table == nullptr) {
        return;
    }

    const QSignalBlocker blocker(table);
    table->setRowCount(0);

    for (const WindowsUwpPackageInfo& package : packages) {
        const int row = table->rowCount();
        table->insertRow(row);

        auto* enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        enabledItem->setTextAlignment(Qt::AlignCenter);
        enabledItem->setCheckState(package.loopbackEnabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(PackageRole, package.packageFamilyName);
        table->setItem(row, EnabledColumn, enabledItem);

        auto* nameItem = createReadOnlyItem(displayNameForPackage(package));
        nameItem->setToolTip(package.packageFullName);
        table->setItem(row, AppColumn, nameItem);

        auto* packageItem = createReadOnlyItem(package.packageFamilyName);
        packageItem->setToolTip(package.packageFullName);
        table->setItem(row, PackageColumn, packageItem);
    }
}

void applyPackageFilter(QTableWidget* table, const QString& needle)
{
    if (table == nullptr) {
        return;
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        const QString app = table->item(row, AppColumn) == nullptr ? QString() : table->item(row, AppColumn)->text();
        const QString packageFamilyName = table->item(row, PackageColumn) == nullptr
            ? QString()
            : table->item(row, PackageColumn)->text();
        const QString packageFullName = table->item(row, PackageColumn) == nullptr
            ? QString()
            : table->item(row, PackageColumn)->toolTip();

        const bool match = needle.isEmpty()
            || app.contains(needle, Qt::CaseInsensitive)
            || packageFamilyName.contains(needle, Qt::CaseInsensitive)
            || packageFullName.contains(needle, Qt::CaseInsensitive);
        table->setRowHidden(row, !match);
    }
}

int enabledPackageCount(const QList<WindowsUwpPackageInfo>& packages)
{
    int count = 0;
    for (const WindowsUwpPackageInfo& package : packages) {
        if (package.loopbackEnabled) {
            ++count;
        }
    }
    return count;
}

} // namespace UwpLoopbackDialogSupport
