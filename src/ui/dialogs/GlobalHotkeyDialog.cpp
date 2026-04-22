#include "ui/dialogs/GlobalHotkeyDialog.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

#include "platform/windows/WindowsGlobalHotkeyKeyInterop.h"

namespace {

QList<GlobalHotkeyAction> actionOrder()
{
    return {
        GlobalHotkeyAction::ShowForm,
        GlobalHotkeyAction::SystemProxyClear,
        GlobalHotkeyAction::SystemProxySet,
        GlobalHotkeyAction::SystemProxyUnchanged,
        GlobalHotkeyAction::SystemProxyPac
    };
}

Qt::KeyboardModifiers relevantModifiers(Qt::KeyboardModifiers modifiers)
{
    return modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier);
}

} // namespace

GlobalHotkeyDialog::GlobalHotkeyDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    updateUi();
}

void GlobalHotkeyDialog::setHotkeys(const QList<GlobalHotkeyItem>& hotkeys)
{
    hotkeys_.clear();
    for (const GlobalHotkeyItem& hotkey : hotkeys) {
        if (!hotkey.keyCode.has_value() || hotkey.keyCode.value() == 0) {
            continue;
        }
        hotkeys_.append(hotkey);
    }

    std::sort(hotkeys_.begin(), hotkeys_.end(), [](const GlobalHotkeyItem& lhs, const GlobalHotkeyItem& rhs) {
        return static_cast<int>(lhs.action) < static_cast<int>(rhs.action);
    });
    updateUi();
}

QList<GlobalHotkeyItem> GlobalHotkeyDialog::hotkeys() const
{
    return hotkeys_;
}

bool GlobalHotkeyDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event == nullptr || event->type() != QEvent::KeyPress) {
        return QDialog::eventFilter(watched, event);
    }

    auto* edit = qobject_cast<QLineEdit*>(watched);
    if (edit == nullptr) {
        return QDialog::eventFilter(watched, event);
    }

    captureHotkey(
        static_cast<GlobalHotkeyAction>(edit->property("globalHotkeyAction").toInt()),
        static_cast<QKeyEvent*>(event));
    return true;
}

void GlobalHotkeyDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (cancelButton_ != nullptr) {
        cancelButton_->setFocus(Qt::OtherFocusReason);
    }
}

void GlobalHotkeyDialog::setupUi()
{
    setWindowTitle(tr("Global Hotkey Settings"));
    resize(520, 280);

    auto* layout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    const QList<GlobalHotkeyAction> actions = actionOrder();
    for (int index = 0; index < actions.size(); ++index) {
        auto* edit = new QLineEdit(this);
        edit->setObjectName(QStringLiteral("globalHotkeyEdit_%1").arg(index));
        edit->setReadOnly(true);
        edit->setPlaceholderText(tr("Press a shortcut"));
        edit->setProperty("globalHotkeyAction", static_cast<int>(actions.at(index)));
        edit->installEventFilter(this);
        hotkeyEdits_.append(edit);
        formLayout->addRow(actionLabel(actions.at(index)), edit);
    }

    tipLabel_ = new QLabel(
        tr("PAC proxy mode is not available in the current Qt rewrite. Its shortcut is kept for config compatibility."),
        this);
    tipLabel_->setWordWrap(true);

    auto* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    resetButton_ = buttonBox->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
    saveButton_ = buttonBox->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    cancelButton_ = buttonBox->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    resetButton_->setObjectName(QStringLiteral("globalHotkeyResetButton"));
    saveButton_->setObjectName(QStringLiteral("globalHotkeySaveButton"));
    cancelButton_->setObjectName(QStringLiteral("globalHotkeyCancelButton"));

    connect(resetButton_, &QPushButton::clicked, this, [this]() {
        hotkeys_.clear();
        updateUi();
    });
    connect(saveButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);

    layout->addLayout(formLayout);
    layout->addWidget(tipLabel_);
    layout->addWidget(buttonBox);
}

void GlobalHotkeyDialog::updateUi()
{
    const QList<GlobalHotkeyAction> actions = actionOrder();
    for (int index = 0; index < hotkeyEdits_.size() && index < actions.size(); ++index) {
        QLineEdit* edit = hotkeyEdits_.at(index);
        if (edit == nullptr) {
            continue;
        }

        const int hotkeyIndex = findHotkeyIndex(actions.at(index));
        edit->setText(hotkeyIndex >= 0 ? hotkeyText(hotkeys_.at(hotkeyIndex)) : QString());
    }
}

void GlobalHotkeyDialog::captureHotkey(GlobalHotkeyAction action, QKeyEvent* event)
{
    if (event == nullptr) {
        return;
    }

    const int key = event->key();
    GlobalHotkeyItem hotkey;
    hotkey.action = action;
    const Qt::KeyboardModifiers modifiers = relevantModifiers(event->modifiers());
    hotkey.control = modifiers.testFlag(Qt::ControlModifier);
    hotkey.alt = modifiers.testFlag(Qt::AltModifier);
    hotkey.shift = modifiers.testFlag(Qt::ShiftModifier);
    if (!isModifierOnlyKey(key) && key != Qt::Key_unknown) {
        hotkey.keyCode = qtKeyToGlobalHotkeyKeyCode(key, event->modifiers());
    }

    const int existingIndex = findHotkeyIndex(action);
    if (!hotkey.keyCode.has_value() || hotkey.keyCode.value() == 0) {
        if (existingIndex >= 0) {
            hotkeys_.removeAt(existingIndex);
            updateUi();
        }
        return;
    }

    if (existingIndex >= 0) {
        hotkeys_[existingIndex] = hotkey;
    } else {
        hotkeys_.append(hotkey);
        std::sort(hotkeys_.begin(), hotkeys_.end(), [](const GlobalHotkeyItem& lhs, const GlobalHotkeyItem& rhs) {
            return static_cast<int>(lhs.action) < static_cast<int>(rhs.action);
        });
    }
    updateUi();
}

int GlobalHotkeyDialog::findHotkeyIndex(GlobalHotkeyAction action) const
{
    for (int index = 0; index < hotkeys_.size(); ++index) {
        if (hotkeys_.at(index).action == action) {
            return index;
        }
    }

    return -1;
}

QString GlobalHotkeyDialog::actionLabel(GlobalHotkeyAction action)
{
    switch (action) {
    case GlobalHotkeyAction::ShowForm:
        return tr("Show Main Window");
    case GlobalHotkeyAction::SystemProxyClear:
        return tr("Clear System Proxy");
    case GlobalHotkeyAction::SystemProxySet:
        return tr("Set System Proxy");
    case GlobalHotkeyAction::SystemProxyUnchanged:
        return tr("Keep System Proxy Unchanged");
    case GlobalHotkeyAction::SystemProxyPac:
        return tr("System Proxy PAC");
    default:
        return QString();
    }
}

bool GlobalHotkeyDialog::isModifierOnlyKey(int key)
{
    return key == Qt::Key_Control
        || key == Qt::Key_Shift
        || key == Qt::Key_Alt
        || key == Qt::Key_Meta;
}

QString GlobalHotkeyDialog::hotkeyText(const GlobalHotkeyItem& hotkey)
{
    return globalHotkeyShortcutText(hotkey);
}
