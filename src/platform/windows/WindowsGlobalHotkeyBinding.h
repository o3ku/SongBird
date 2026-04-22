#pragma once

#include <QList>
#include <QString>

#include "domain/models/GlobalHotkeyItem.h"

struct WindowsGlobalHotkeyBinding {
    GlobalHotkeyAction action = GlobalHotkeyAction::ShowForm;
    int id = 0;
    unsigned int modifiers = 0;
    unsigned int virtualKey = 0;
    QString actionName;
    QString shortcut;
};

QList<WindowsGlobalHotkeyBinding> buildWindowsGlobalHotkeyBindings(const QList<GlobalHotkeyItem>& hotkeys);
