#include "platform/windows/WindowsGlobalHotkeyBinding.h"

#include <QCoreApplication>

#include <optional>

#include "platform/windows/WindowsGlobalHotkeyKeyInterop.h"
#include <windows.h>

namespace {

int bindingId(unsigned int modifiers, unsigned int virtualKey)
{
    return static_cast<int>((virtualKey << 16) | (modifiers & 0xffffu));
}

QString actionName(GlobalHotkeyAction action)
{
    switch (action) {
    case GlobalHotkeyAction::ShowForm:
        return QCoreApplication::translate("GlobalHotkeyDialog", "Show Main Window");
    case GlobalHotkeyAction::SystemProxyClear:
        return QCoreApplication::translate("GlobalHotkeyDialog", "Clear System Proxy");
    case GlobalHotkeyAction::SystemProxySet:
        return QCoreApplication::translate("GlobalHotkeyDialog", "Set System Proxy");
    default:
        return QString();
    }
}

unsigned int modifierFlags(const GlobalHotkeyItem& hotkey)
{
    unsigned int value = 0;
    if (hotkey.control) {
        value |= MOD_CONTROL;
    }
    if (hotkey.alt) {
        value |= MOD_ALT;
    }
    if (hotkey.shift) {
        value |= MOD_SHIFT;
    }
    return value;
}

} // namespace

QList<WindowsGlobalHotkeyBinding> buildWindowsGlobalHotkeyBindings(const QList<GlobalHotkeyItem>& hotkeys)
{
    QList<WindowsGlobalHotkeyBinding> bindings;
    for (const GlobalHotkeyItem& hotkey : hotkeys) {
        if (!hotkey.keyCode.has_value() || hotkey.keyCode.value() == 0) {
            continue;
        }

        const std::optional<unsigned int> virtualKey = globalHotkeyKeyCodeToVirtualKey(hotkey.keyCode.value());
        if (!virtualKey.has_value()) {
            continue;
        }

        WindowsGlobalHotkeyBinding binding;
        binding.action = hotkey.action;
        binding.modifiers = modifierFlags(hotkey);
        binding.virtualKey = virtualKey.value();
        binding.id = bindingId(binding.modifiers, binding.virtualKey);
        binding.actionName = actionName(hotkey.action);
        binding.shortcut = globalHotkeyShortcutText(hotkey);
        bindings.append(binding);
    }

    return bindings;
}
