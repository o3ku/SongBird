#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

// The panel shown in the server table's place when there is nothing to list.
//
// Three entry points cover the ways a user can get servers into the app:
// importing a share link or subscription URL, adding one by hand, and pasting
// from the clipboard. It is a plain widget with signals rather than a dialog so
// the workspace can stack it over the table and the window can wire it to the
// same actions the toolbar uses.
class ServerTableEmptyStateWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ServerTableEmptyStateWidget(QWidget* parent = nullptr);

    // The overlay can be shown for two different reasons, and the wording has to
    // differ: an empty list is a first-run state ("import something"), while a
    // filter that matched nothing is a dead end the user can undo. Call this
    // before showing the widget.
    void setFilterActive(bool filterActive);
    bool filterActive() const;

    // An empty table is not always a first run. An import that downloaded fine and
    // produced no node -- every node an unsupported protocol, a payload the parsers
    // do not know -- leaves the same empty table behind, and "import something to
    // get started" then describes a state the user is no longer in: they did import
    // something, and it failed. The reason belongs in this panel, because the panel
    // is what the table is hiding.
    void setImportFailure(const QString& reason);
    QString importFailure() const;

    QPushButton* importButton() const;
    QPushButton* addButton() const;
    QPushButton* clipboardButton() const;

signals:
    void importRequested();
    void addRequested();
    void importFromClipboardRequested();

private:
    void setupUi();
    void refreshTexts();

    QLabel* titleLabel_ = nullptr;
    QLabel* hintLabel_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* clipboardButton_ = nullptr;
    bool filterActive_ = false;
    QString importFailure_;
};
