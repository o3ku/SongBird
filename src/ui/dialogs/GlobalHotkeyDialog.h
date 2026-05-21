#pragma once

#include <QDialog>
#include <QList>

#include "domain/models/GlobalHotkeyItem.h"

class QLabel;
class QLineEdit;
class QPushButton;

class GlobalHotkeyDialog final : public QDialog {
    Q_OBJECT

public:
    explicit GlobalHotkeyDialog(QWidget* parent = nullptr);

    void setHotkeys(const QList<GlobalHotkeyItem>& hotkeys);
    QList<GlobalHotkeyItem> hotkeys() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void updateUi();
    void captureHotkey(GlobalHotkeyAction action, QKeyEvent* event);
    int findHotkeyIndex(GlobalHotkeyAction action) const;
    static QString actionLabel(GlobalHotkeyAction action);
    static bool isModifierOnlyKey(int key);
    static QString hotkeyText(const GlobalHotkeyItem& hotkey);

    QList<GlobalHotkeyItem> hotkeys_;
    QList<QLineEdit*> hotkeyEdits_;
    QPushButton* resetButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
};
