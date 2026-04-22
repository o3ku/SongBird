#pragma once

#include <optional>

#include <Qt>
#include <QString>

#include "domain/models/GlobalHotkeyItem.h"

struct QtHotkeyKey {
    int key = Qt::Key_unknown;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
};

std::optional<int> qtKeyToGlobalHotkeyKeyCode(int qtKey, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
std::optional<QtHotkeyKey> globalHotkeyKeyCodeToQtKey(int keyCode);
std::optional<unsigned int> globalHotkeyKeyCodeToVirtualKey(int keyCode);
std::optional<QtHotkeyKey> virtualKeyToQtHotkeyKey(unsigned int virtualKey);
QString globalHotkeyShortcutText(const GlobalHotkeyItem& hotkey);
