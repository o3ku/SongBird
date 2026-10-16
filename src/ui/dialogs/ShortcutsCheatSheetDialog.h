#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QKeySequence;
class QTableWidget;
class QDialogButtonBox;

// A read-only cheat sheet listing the shortcuts the main window actually
// installs.
//
// The sheet is data-driven and the data is exposed through shortcutEntries() so a
// test can compare it against the QActions the window really installs: a shortcut
// added to MainWindowShortcutController or ServerActionsController without a row
// here (or a row whose chord no longer matches) fails that test instead of
// silently going stale.
class ShortcutsCheatSheetDialog final : public QDialog {
    Q_OBJECT

public:
    struct ShortcutEntry {
        // Human-readable key chord, written with the platform-neutral "Ctrl"/"Alt"
        // spelling rather than QKeySequence::toString() so the sheet reads the
        // same on every platform this app ships. This is the string the user sees.
        QString keys;
        QString description;
        // The chords this row documents, as real QKeySequences. Empty for a row
        // that stands for a range of shortcuts (for example "Alt+1 .. Alt+9"),
        // which the drift test expands separately.
        QList<QKeySequence> sequences;
    };

    explicit ShortcutsCheatSheetDialog(QWidget* parent = nullptr);

    // The rows the sheet lists, in display order. The single source of truth for
    // both the table and the drift test.
    static const QList<ShortcutEntry>& shortcutEntries();

    // Exposed for tests: the number of rows the sheet lists.
    int shortcutCount() const;

private:
    void setupUi();

    QTableWidget* table_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};
