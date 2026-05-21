#pragma once

#include <optional>

enum class GlobalHotkeyAction : int {
    ShowForm = 0,
    SystemProxyClear = 1,
    SystemProxySet = 2
};

struct GlobalHotkeyItem {
    GlobalHotkeyAction action = GlobalHotkeyAction::ShowForm;
    bool alt = false;
    bool control = false;
    bool shift = false;
    std::optional<int> keyCode;
};
