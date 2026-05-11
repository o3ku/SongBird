#include "platform/windows/WindowsGlobalHotkeyService.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHash>
#include <QKeyEvent>
#include <QList>
#include <QStringList>
#include <QWidget>

#include "platform/windows/WindowsGlobalHotkeyBinding.h"
#include "platform/windows/WindowsGlobalHotkeyKeyInterop.h"

#include <windows.h>

namespace {

Qt::KeyboardModifiers nativeModifiersToQt(unsigned int nativeModifiers)
{
    Qt::KeyboardModifiers modifiers;
    if ((nativeModifiers & MOD_CONTROL) != 0u) {
        modifiers |= Qt::ControlModifier;
    }
    if ((nativeModifiers & MOD_ALT) != 0u) {
        modifiers |= Qt::AltModifier;
    }
    if ((nativeModifiers & MOD_SHIFT) != 0u) {
        modifiers |= Qt::ShiftModifier;
    }
    return modifiers;
}

void replayPausedHotkeyToFocusWidget(const MSG& nativeMessage)
{
    QWidget* focusWidget = QApplication::focusWidget();
    if (focusWidget == nullptr) {
        return;
    }

    const unsigned int nativeModifiers = LOWORD(nativeMessage.lParam);
    const unsigned int virtualKey = HIWORD(nativeMessage.lParam);
    const std::optional<QtHotkeyKey> qtHotkey = virtualKeyToQtHotkeyKey(virtualKey);
    if (!qtHotkey.has_value() || qtHotkey->key == Qt::Key_unknown) {
        return;
    }

    QKeyEvent keyPressEvent(
        QEvent::KeyPress,
        qtHotkey->key,
        nativeModifiersToQt(nativeModifiers) | qtHotkey->modifiers);
    QCoreApplication::sendEvent(focusWidget, &keyPressEvent);
}

} // namespace

WindowsGlobalHotkeyService::WindowsGlobalHotkeyService(QObject* parent)
    : QObject(parent)
{
}

WindowsGlobalHotkeyService::~WindowsGlobalHotkeyService()
{
    unregisterHotkeys();
}

OperationResult WindowsGlobalHotkeyService::registerHotkeys(const QList<GlobalHotkeyItem>& hotkeys)
{
    unregisterHotkeys();
    registeredActions_.clear();

    QCoreApplication* application = QCoreApplication::instance();
    if (application == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate(
                "WindowsGlobalHotkeyService",
                "Cannot register global hotkeys before the Qt application exists."));
    }

    const QList<WindowsGlobalHotkeyBinding> bindings = buildWindowsGlobalHotkeyBindings(hotkeys);
    if (bindings.isEmpty()) {
        return OperationResult::ok(QString());
    }

    application->installNativeEventFilter(this);
    filterInstalled_ = true;

    QList<QString> successMessages;
    QList<QString> failureMessages;
    QHash<int, WindowsGlobalHotkeyBinding> groupedBindings;
    QHash<int, QList<GlobalHotkeyAction>> groupedActions;
    QHash<int, QStringList> groupedActionNames;
    QList<int> bindingOrder;

    for (const WindowsGlobalHotkeyBinding& binding : bindings) {
        if (!groupedBindings.contains(binding.id)) {
            groupedBindings.insert(binding.id, binding);
            bindingOrder.append(binding.id);
        }

        QList<GlobalHotkeyAction>& actions = groupedActions[binding.id];
        if (!actions.contains(binding.action)) {
            actions.append(binding.action);
        }

        QStringList& actionNames = groupedActionNames[binding.id];
        if (!actionNames.contains(binding.actionName)) {
            actionNames.append(binding.actionName);
        }
    }

    for (int bindingId : bindingOrder) {
        const WindowsGlobalHotkeyBinding binding = groupedBindings.value(bindingId);
        unsigned int modifiers = binding.modifiers;
#ifdef MOD_NOREPEAT
        modifiers |= MOD_NOREPEAT;
#endif
        const QList<GlobalHotkeyAction> actions = groupedActions.value(binding.id);
        const QStringList actionNames = groupedActionNames.value(binding.id);
        if (::RegisterHotKey(nullptr, binding.id, modifiers, binding.virtualKey)) {
            registeredIds_.append(binding.id);
            registeredActions_[binding.id] = actions;
            for (const QString& actionName : actionNames) {
                successMessages.append(QStringLiteral("%1 -> %2").arg(binding.shortcut, actionName));
            }
            continue;
        }

        const QString errorCode = QString::number(::GetLastError());
        for (const QString& actionName : actionNames) {
            failureMessages.append(
                QCoreApplication::translate(
                    "WindowsGlobalHotkeyService",
                    "%1 -> %2 (Win32 error %3)")
                    .arg(binding.shortcut, actionName, errorCode));
        }
    }

    if (registeredIds_.isEmpty()) {
        unregisterHotkeys();
        return OperationResult::fail(
            QCoreApplication::translate(
                "WindowsGlobalHotkeyService",
                "Failed to register global hotkeys.\n%1")
                .arg(failureMessages.join(QStringLiteral("\n"))));
    }

    return failureMessages.isEmpty()
        ? OperationResult::ok(buildRegistrationMessage(successMessages, failureMessages))
        : OperationResult::fail(buildRegistrationMessage(successMessages, failureMessages));
}

void WindowsGlobalHotkeyService::unregisterHotkeys()
{
    for (int id : registeredIds_) {
        ::UnregisterHotKey(nullptr, id);
    }
    registeredIds_.clear();
    registeredActions_.clear();

    if (filterInstalled_ && QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
    filterInstalled_ = false;
}

void WindowsGlobalHotkeyService::setPaused(bool paused)
{
    paused_ = paused;
}

bool WindowsGlobalHotkeyService::nativeEventFilter(const QByteArray& eventType, void* message, NativeEventResult* result)
{
    if (eventType != "windows_dispatcher_MSG" && eventType != "windows_generic_MSG") {
        return false;
    }

    MSG* nativeMessage = static_cast<MSG*>(message);
    if (nativeMessage == nullptr || nativeMessage->message != WM_HOTKEY) {
        return false;
    }

    if (result != nullptr) {
        *result = 0;
    }

    const int hotkeyId = static_cast<int>(nativeMessage->wParam);
    const QList<GlobalHotkeyAction> actions = registeredActions_.value(hotkeyId);
    if (actions.isEmpty()) {
        return false;
    }
    if (paused_) {
        replayPausedHotkeyToFocusWidget(*nativeMessage);
        return true;
    }

    for (GlobalHotkeyAction action : actions) {
        switch (action) {
        case GlobalHotkeyAction::ShowForm:
            emit toggleMainWindowRequested();
            break;
        case GlobalHotkeyAction::SystemProxySet:
            emit enableProxyRequested();
            break;
        case GlobalHotkeyAction::SystemProxyClear:
            emit disableProxyRequested();
            break;
        case GlobalHotkeyAction::SystemProxyUnchanged:
            emit keepProxyUnchangedRequested();
            break;
        case GlobalHotkeyAction::SystemProxyPac:
            emit pacProxyRequested();
            break;
        default:
            break;
        }
    }

    return true;
}

QString WindowsGlobalHotkeyService::buildRegistrationMessage(
    const QList<QString>& successMessages,
    const QList<QString>& failureMessages) const
{
    QStringList lines;
    if (!successMessages.isEmpty()) {
        lines.append(QCoreApplication::translate("WindowsGlobalHotkeyService", "Global hotkeys ready:"));
        for (const QString& item : successMessages) {
            lines.append(QStringLiteral("  %1").arg(item));
        }
    }

    if (!failureMessages.isEmpty()) {
        lines.append(QCoreApplication::translate(
            "WindowsGlobalHotkeyService",
            "Global hotkey registration failed for:"));
        for (const QString& item : failureMessages) {
            lines.append(QStringLiteral("  %1").arg(item));
        }
    }

    return lines.join(QStringLiteral("\n"));
}
