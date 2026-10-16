#include "ui/dialogs/ShortcutsCheatSheetDialog.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "common/DialogUtils.h"
#include "ui/theme/AppTheme.h"

namespace {

// One row of the sheet. "keys" is the platform-neutral display spelling; the
// sequences are the same chords as real QKeySequences, so the drift test can
// compare them against the actions the window installs.
struct ShortcutRow {
    const char* keys;
    const char* description;
    QList<QKeySequence> sequences;
};

// Kept in step with the two places that actually install shortcuts:
// MainWindowShortcutController (window scope) and ServerActionsController
// (server-table scope). Grouped in the order a user is likely to look for them.
//
// The drift test in MainWindowTests walks every installed QAction and fails if a
// chord is documented here but not installed, or installed but not documented
// (the Alt+1..9 range below is expanded there).
//
// Built at runtime because QKeySequence is not a literal type.
const QList<ShortcutRow>& shortcutRows()
{
    static const QList<ShortcutRow> rows = {
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+F"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Focus the server filter"),
         {QKeySequence(QKeySequence::Find)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Alt+T"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Speed test the selected servers"),
         {QKeySequence(Qt::ALT | Qt::Key_T)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+R"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Update the current subscription"),
         {QKeySequence(Qt::CTRL | Qt::Key_R)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+Shift+R"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Update all subscriptions"),
         {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Alt+1 .. Alt+9"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Switch to subscription tab 1-9"),
         {}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Alt+X"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Exit SongBird"),
         {QKeySequence(Qt::ALT | Qt::Key_X)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Shift+?"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Open this keyboard shortcuts sheet"),
         {QKeySequence(Qt::SHIFT | Qt::Key_Question), QKeySequence(Qt::SHIFT | Qt::Key_Slash)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Enter"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Set the selected server as current"),
         {QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+Enter"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Set current and enable TUN"),
         {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+C"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Copy the selected share link(s)"),
         {QKeySequence(QKeySequence::Copy)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+V"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Import servers from the clipboard"),
         {QKeySequence(QKeySequence::Paste)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Ctrl+A"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Select all servers"),
         {QKeySequence(QKeySequence::SelectAll)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Delete"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Remove the selected servers"),
         {QKeySequence(QKeySequence::Delete)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "T / B"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Move the selection to the top / bottom"),
         {QKeySequence(Qt::Key_T), QKeySequence(Qt::Key_B)}},
        {QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "U / D"),
         QT_TRANSLATE_NOOP("ShortcutsCheatSheetDialog", "Move the selection up / down"),
         {QKeySequence(Qt::Key_U), QKeySequence(Qt::Key_D)}},
    };
    return rows;
}

constexpr int kShortcutColumn = 0;
constexpr int kDescriptionColumn = 1;

} // namespace

const QList<ShortcutsCheatSheetDialog::ShortcutEntry>& ShortcutsCheatSheetDialog::shortcutEntries()
{
    static const QList<ShortcutEntry> entries = []() {
        QList<ShortcutEntry> result;
        const QList<ShortcutRow>& rows = shortcutRows();
        result.reserve(rows.size());
        for (const ShortcutRow& row : rows) {
            result.append(ShortcutEntry{
                QCoreApplication::translate("ShortcutsCheatSheetDialog", row.keys),
                QCoreApplication::translate("ShortcutsCheatSheetDialog", row.description),
                row.sequences});
        }
        return result;
    }();
    return entries;
}


ShortcutsCheatSheetDialog::ShortcutsCheatSheetDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

int ShortcutsCheatSheetDialog::shortcutCount() const
{
    return table_ == nullptr ? 0 : table_->rowCount();
}

void ShortcutsCheatSheetDialog::setupUi()
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(460, 480);
    AppTheme::applyCompactFont(this);

    auto* hintLabel = new QLabel(tr("Shortcuts available in the main window."), this);
    hintLabel->setWordWrap(true);

    table_ = new QTableWidget(this);
    table_->setObjectName(QStringLiteral("shortcutsCheatSheetTable"));
    table_->setColumnCount(2);
    table_->setHorizontalHeaderLabels({tr("Keys"), tr("Action")});
    table_->setRowCount(shortcutEntries().size());
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setSectionResizeMode(kShortcutColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(kDescriptionColumn, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setShowGrid(false);

    const QList<ShortcutEntry>& entries = shortcutEntries();
    for (int row = 0; row < entries.size(); ++row) {
        const ShortcutEntry& entry = entries.at(row);
        auto* keysItem = new QTableWidgetItem(entry.keys);
        auto* descriptionItem = new QTableWidgetItem(entry.description);
        keysItem->setFlags(Qt::ItemIsEnabled);
        descriptionItem->setFlags(Qt::ItemIsEnabled);
        table_->setItem(row, kShortcutColumn, keysItem);
        table_->setItem(row, kDescriptionColumn, descriptionItem);
    }

    buttonBox_ = new QDialogButtonBox(QDialogButtonBox::Close, this);
    DialogUtils::localizeStandardDialogButtonBox(buttonBox_);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 14, 16, 12);
    rootLayout->setSpacing(10);
    rootLayout->addWidget(hintLabel);
    rootLayout->addWidget(table_, 1);
    rootLayout->addWidget(buttonBox_);

    connect(buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
