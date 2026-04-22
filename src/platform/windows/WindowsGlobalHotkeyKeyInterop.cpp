#include "platform/windows/WindowsGlobalHotkeyKeyInterop.h"

#include <QKeySequence>

#include <windows.h>

namespace {

constexpr int WpfKeyA = 44;
constexpr int WpfKeyZ = 69;
constexpr int WpfKeyD0 = 34;
constexpr int WpfKeyD9 = 43;
constexpr int WpfKeyNumPad0 = 74;
constexpr int WpfKeyNumPad9 = 83;
constexpr int WpfKeyAdd = 85;
constexpr int WpfKeyMultiply = 84;
constexpr int WpfKeySubtract = 87;
constexpr int WpfKeyDecimal = 88;
constexpr int WpfKeyDivide = 89;
constexpr int WpfKeyF1 = 90;
constexpr int WpfKeyF24 = 113;
constexpr int WpfKeyEscape = 13;
constexpr int WpfKeyPause = 7;
constexpr int WpfKeyCapital = 8;
constexpr int WpfKeyTab = 3;
constexpr int WpfKeyBack = 2;
constexpr int WpfKeyReturn = 6;
constexpr int WpfKeySpace = 18;
constexpr int WpfKeyPrintScreen = 30;
constexpr int WpfKeyPageUp = 19;
constexpr int WpfKeyPageDown = 20;
constexpr int WpfKeyEnd = 21;
constexpr int WpfKeyHome = 22;
constexpr int WpfKeyLeft = 23;
constexpr int WpfKeyUp = 24;
constexpr int WpfKeyRight = 25;
constexpr int WpfKeyDown = 26;
constexpr int WpfKeyInsert = 31;
constexpr int WpfKeyDelete = 32;
constexpr int WpfKeyNumLock = 114;
constexpr int WpfKeyScroll = 115;
constexpr int WpfKeyOemSemicolon = 140;
constexpr int WpfKeyOemPlus = 141;
constexpr int WpfKeyOemComma = 142;
constexpr int WpfKeyOemMinus = 143;
constexpr int WpfKeyOemPeriod = 144;
constexpr int WpfKeyOemQuestion = 145;
constexpr int WpfKeyOemTilde = 146;
constexpr int WpfKeyOemOpenBrackets = 149;
constexpr int WpfKeyOemPipe = 150;
constexpr int WpfKeyOemCloseBrackets = 151;
constexpr int WpfKeyOemQuotes = 152;

std::optional<int> normalizeStoredKeyCode(int keyCode)
{
    switch (keyCode) {
    case WpfKeyEscape:
    case WpfKeyPause:
    case WpfKeyCapital:
    case WpfKeyTab:
    case WpfKeyBack:
    case WpfKeyReturn:
    case WpfKeySpace:
    case WpfKeyPrintScreen:
    case WpfKeyPageUp:
    case WpfKeyPageDown:
    case WpfKeyEnd:
    case WpfKeyHome:
    case WpfKeyLeft:
    case WpfKeyUp:
    case WpfKeyRight:
    case WpfKeyDown:
    case WpfKeyInsert:
    case WpfKeyDelete:
    case WpfKeyNumLock:
    case WpfKeyScroll:
    case WpfKeyAdd:
    case WpfKeyMultiply:
    case WpfKeySubtract:
    case WpfKeyDecimal:
    case WpfKeyDivide:
    case WpfKeyOemSemicolon:
    case WpfKeyOemPlus:
    case WpfKeyOemComma:
    case WpfKeyOemMinus:
    case WpfKeyOemPeriod:
    case WpfKeyOemQuestion:
    case WpfKeyOemTilde:
    case WpfKeyOemOpenBrackets:
    case WpfKeyOemPipe:
    case WpfKeyOemCloseBrackets:
    case WpfKeyOemQuotes:
        return keyCode;
    default:
        break;
    }

    if ((keyCode >= WpfKeyA && keyCode <= WpfKeyZ)
        || (keyCode >= WpfKeyD0 && keyCode <= WpfKeyD9)
        || (keyCode >= WpfKeyNumPad0 && keyCode <= WpfKeyNumPad9)
        || (keyCode >= WpfKeyF1 && keyCode <= WpfKeyF24)) {
        return keyCode;
    }

    return qtKeyToGlobalHotkeyKeyCode(keyCode);
}

} // namespace

std::optional<int> qtKeyToGlobalHotkeyKeyCode(int qtKey, Qt::KeyboardModifiers modifiers)
{
    const bool keypad = modifiers.testFlag(Qt::KeypadModifier);
    if (keypad) {
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
            return WpfKeyNumPad0 + (qtKey - Qt::Key_0);
        }

        switch (qtKey) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            return WpfKeyAdd;
        case Qt::Key_Minus:
            return WpfKeySubtract;
        case Qt::Key_Asterisk:
            return WpfKeyMultiply;
        case Qt::Key_Slash:
            return WpfKeyDivide;
        case Qt::Key_Period:
        case Qt::Key_Comma:
            return WpfKeyDecimal;
        default:
            break;
        }
    }

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        return WpfKeyA + (qtKey - Qt::Key_A);
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        return WpfKeyD0 + (qtKey - Qt::Key_0);
    }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
        return WpfKeyF1 + (qtKey - Qt::Key_F1);
    }

    switch (qtKey) {
    case Qt::Key_Escape:
        return WpfKeyEscape;
    case Qt::Key_Pause:
        return WpfKeyPause;
    case Qt::Key_CapsLock:
        return WpfKeyCapital;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return WpfKeyTab;
    case Qt::Key_Backspace:
        return WpfKeyBack;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return WpfKeyReturn;
    case Qt::Key_Space:
        return WpfKeySpace;
    case Qt::Key_Print:
    case Qt::Key_SysReq:
        return WpfKeyPrintScreen;
    case Qt::Key_Insert:
        return WpfKeyInsert;
    case Qt::Key_Delete:
        return WpfKeyDelete;
    case Qt::Key_NumLock:
        return WpfKeyNumLock;
    case Qt::Key_ScrollLock:
        return WpfKeyScroll;
    case Qt::Key_Home:
        return WpfKeyHome;
    case Qt::Key_End:
        return WpfKeyEnd;
    case Qt::Key_PageUp:
        return WpfKeyPageUp;
    case Qt::Key_PageDown:
        return WpfKeyPageDown;
    case Qt::Key_Left:
        return WpfKeyLeft;
    case Qt::Key_Up:
        return WpfKeyUp;
    case Qt::Key_Right:
        return WpfKeyRight;
    case Qt::Key_Down:
        return WpfKeyDown;
    case Qt::Key_Semicolon:
    case Qt::Key_Colon:
        return WpfKeyOemSemicolon;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
        return WpfKeyOemPlus;
    case Qt::Key_Comma:
    case Qt::Key_Less:
        return WpfKeyOemComma;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        return WpfKeyOemMinus;
    case Qt::Key_Period:
    case Qt::Key_Greater:
        return WpfKeyOemPeriod;
    case Qt::Key_Slash:
    case Qt::Key_Question:
        return WpfKeyOemQuestion;
    case Qt::Key_QuoteLeft:
    case Qt::Key_AsciiTilde:
        return WpfKeyOemTilde;
    case Qt::Key_BracketLeft:
    case Qt::Key_BraceLeft:
        return WpfKeyOemOpenBrackets;
    case Qt::Key_Backslash:
    case Qt::Key_Bar:
        return WpfKeyOemPipe;
    case Qt::Key_BracketRight:
    case Qt::Key_BraceRight:
        return WpfKeyOemCloseBrackets;
    case Qt::Key_Apostrophe:
    case Qt::Key_QuoteDbl:
        return WpfKeyOemQuotes;
    default:
        return std::nullopt;
    }
}

std::optional<QtHotkeyKey> globalHotkeyKeyCodeToQtKey(int keyCode)
{
    const std::optional<int> normalizedKeyCode = normalizeStoredKeyCode(keyCode);
    if (!normalizedKeyCode.has_value()) {
        return std::nullopt;
    }
    keyCode = normalizedKeyCode.value();

    QtHotkeyKey hotkey;
    if (keyCode >= WpfKeyA && keyCode <= WpfKeyZ) {
        hotkey.key = Qt::Key_A + (keyCode - WpfKeyA);
        return hotkey;
    }
    if (keyCode >= WpfKeyD0 && keyCode <= WpfKeyD9) {
        hotkey.key = Qt::Key_0 + (keyCode - WpfKeyD0);
        return hotkey;
    }
    if (keyCode >= WpfKeyNumPad0 && keyCode <= WpfKeyNumPad9) {
        hotkey.key = Qt::Key_0 + (keyCode - WpfKeyNumPad0);
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    }
    if (keyCode >= WpfKeyF1 && keyCode <= WpfKeyF24) {
        hotkey.key = Qt::Key_F1 + (keyCode - WpfKeyF1);
        return hotkey;
    }

    switch (keyCode) {
    case WpfKeyEscape:
        hotkey.key = Qt::Key_Escape;
        return hotkey;
    case WpfKeyPause:
        hotkey.key = Qt::Key_Pause;
        return hotkey;
    case WpfKeyCapital:
        hotkey.key = Qt::Key_CapsLock;
        return hotkey;
    case WpfKeyTab:
        hotkey.key = Qt::Key_Tab;
        return hotkey;
    case WpfKeyBack:
        hotkey.key = Qt::Key_Backspace;
        return hotkey;
    case WpfKeyReturn:
        hotkey.key = Qt::Key_Return;
        return hotkey;
    case WpfKeySpace:
        hotkey.key = Qt::Key_Space;
        return hotkey;
    case WpfKeyPrintScreen:
        hotkey.key = Qt::Key_Print;
        return hotkey;
    case WpfKeyPageUp:
        hotkey.key = Qt::Key_PageUp;
        return hotkey;
    case WpfKeyPageDown:
        hotkey.key = Qt::Key_PageDown;
        return hotkey;
    case WpfKeyEnd:
        hotkey.key = Qt::Key_End;
        return hotkey;
    case WpfKeyHome:
        hotkey.key = Qt::Key_Home;
        return hotkey;
    case WpfKeyLeft:
        hotkey.key = Qt::Key_Left;
        return hotkey;
    case WpfKeyUp:
        hotkey.key = Qt::Key_Up;
        return hotkey;
    case WpfKeyRight:
        hotkey.key = Qt::Key_Right;
        return hotkey;
    case WpfKeyDown:
        hotkey.key = Qt::Key_Down;
        return hotkey;
    case WpfKeyInsert:
        hotkey.key = Qt::Key_Insert;
        return hotkey;
    case WpfKeyDelete:
        hotkey.key = Qt::Key_Delete;
        return hotkey;
    case WpfKeyNumLock:
        hotkey.key = Qt::Key_NumLock;
        return hotkey;
    case WpfKeyScroll:
        hotkey.key = Qt::Key_ScrollLock;
        return hotkey;
    case WpfKeyOemSemicolon:
        hotkey.key = Qt::Key_Semicolon;
        return hotkey;
    case WpfKeyOemPlus:
        hotkey.key = Qt::Key_Equal;
        return hotkey;
    case WpfKeyOemComma:
        hotkey.key = Qt::Key_Comma;
        return hotkey;
    case WpfKeyOemMinus:
        hotkey.key = Qt::Key_Minus;
        return hotkey;
    case WpfKeyOemPeriod:
        hotkey.key = Qt::Key_Period;
        return hotkey;
    case WpfKeyOemQuestion:
        hotkey.key = Qt::Key_Slash;
        return hotkey;
    case WpfKeyOemTilde:
        hotkey.key = Qt::Key_QuoteLeft;
        return hotkey;
    case WpfKeyOemOpenBrackets:
        hotkey.key = Qt::Key_BracketLeft;
        return hotkey;
    case WpfKeyOemPipe:
        hotkey.key = Qt::Key_Backslash;
        return hotkey;
    case WpfKeyOemCloseBrackets:
        hotkey.key = Qt::Key_BracketRight;
        return hotkey;
    case WpfKeyOemQuotes:
        hotkey.key = Qt::Key_Apostrophe;
        return hotkey;
    case WpfKeyAdd:
        hotkey.key = Qt::Key_Plus;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case WpfKeyMultiply:
        hotkey.key = Qt::Key_Asterisk;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case WpfKeySubtract:
        hotkey.key = Qt::Key_Minus;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case WpfKeyDecimal:
        hotkey.key = Qt::Key_Period;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case WpfKeyDivide:
        hotkey.key = Qt::Key_Slash;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    default:
        return std::nullopt;
    }
}

std::optional<unsigned int> globalHotkeyKeyCodeToVirtualKey(int keyCode)
{
    const std::optional<int> normalizedKeyCode = normalizeStoredKeyCode(keyCode);
    if (!normalizedKeyCode.has_value()) {
        return std::nullopt;
    }
    keyCode = normalizedKeyCode.value();

    if (keyCode >= WpfKeyA && keyCode <= WpfKeyZ) {
        return static_cast<unsigned int>('A' + (keyCode - WpfKeyA));
    }
    if (keyCode >= WpfKeyD0 && keyCode <= WpfKeyD9) {
        return static_cast<unsigned int>('0' + (keyCode - WpfKeyD0));
    }
    if (keyCode >= WpfKeyNumPad0 && keyCode <= WpfKeyNumPad9) {
        return static_cast<unsigned int>(VK_NUMPAD0 + (keyCode - WpfKeyNumPad0));
    }
    if (keyCode >= WpfKeyF1 && keyCode <= WpfKeyF24) {
        return static_cast<unsigned int>(VK_F1 + (keyCode - WpfKeyF1));
    }

    switch (keyCode) {
    case WpfKeyEscape:
        return VK_ESCAPE;
    case WpfKeyPause:
        return VK_PAUSE;
    case WpfKeyCapital:
        return VK_CAPITAL;
    case WpfKeyTab:
        return VK_TAB;
    case WpfKeyBack:
        return VK_BACK;
    case WpfKeyReturn:
        return VK_RETURN;
    case WpfKeySpace:
        return VK_SPACE;
    case WpfKeyPrintScreen:
        return VK_SNAPSHOT;
    case WpfKeyPageUp:
        return VK_PRIOR;
    case WpfKeyPageDown:
        return VK_NEXT;
    case WpfKeyEnd:
        return VK_END;
    case WpfKeyHome:
        return VK_HOME;
    case WpfKeyLeft:
        return VK_LEFT;
    case WpfKeyUp:
        return VK_UP;
    case WpfKeyRight:
        return VK_RIGHT;
    case WpfKeyDown:
        return VK_DOWN;
    case WpfKeyInsert:
        return VK_INSERT;
    case WpfKeyDelete:
        return VK_DELETE;
    case WpfKeyNumLock:
        return VK_NUMLOCK;
    case WpfKeyScroll:
        return VK_SCROLL;
    case WpfKeyOemSemicolon:
        return VK_OEM_1;
    case WpfKeyOemPlus:
        return VK_OEM_PLUS;
    case WpfKeyOemComma:
        return VK_OEM_COMMA;
    case WpfKeyOemMinus:
        return VK_OEM_MINUS;
    case WpfKeyOemPeriod:
        return VK_OEM_PERIOD;
    case WpfKeyOemQuestion:
        return VK_OEM_2;
    case WpfKeyOemTilde:
        return VK_OEM_3;
    case WpfKeyOemOpenBrackets:
        return VK_OEM_4;
    case WpfKeyOemPipe:
        return VK_OEM_5;
    case WpfKeyOemCloseBrackets:
        return VK_OEM_6;
    case WpfKeyOemQuotes:
        return VK_OEM_7;
    case WpfKeyMultiply:
        return VK_MULTIPLY;
    case WpfKeyAdd:
        return VK_ADD;
    case WpfKeySubtract:
        return VK_SUBTRACT;
    case WpfKeyDecimal:
        return VK_DECIMAL;
    case WpfKeyDivide:
        return VK_DIVIDE;
    default:
        return std::nullopt;
    }
}

std::optional<QtHotkeyKey> virtualKeyToQtHotkeyKey(unsigned int virtualKey)
{
    QtHotkeyKey hotkey;
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        hotkey.key = Qt::Key_A + static_cast<int>(virtualKey - 'A');
        return hotkey;
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        hotkey.key = Qt::Key_0 + static_cast<int>(virtualKey - '0');
        return hotkey;
    }
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9) {
        hotkey.key = Qt::Key_0 + static_cast<int>(virtualKey - VK_NUMPAD0);
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    }
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        hotkey.key = Qt::Key_F1 + static_cast<int>(virtualKey - VK_F1);
        return hotkey;
    }

    switch (virtualKey) {
    case VK_ESCAPE:
        hotkey.key = Qt::Key_Escape;
        return hotkey;
    case VK_PAUSE:
        hotkey.key = Qt::Key_Pause;
        return hotkey;
    case VK_CAPITAL:
        hotkey.key = Qt::Key_CapsLock;
        return hotkey;
    case VK_TAB:
        hotkey.key = Qt::Key_Tab;
        return hotkey;
    case VK_BACK:
        hotkey.key = Qt::Key_Backspace;
        return hotkey;
    case VK_RETURN:
        hotkey.key = Qt::Key_Return;
        return hotkey;
    case VK_SPACE:
        hotkey.key = Qt::Key_Space;
        return hotkey;
    case VK_SNAPSHOT:
        hotkey.key = Qt::Key_Print;
        return hotkey;
    case VK_PRIOR:
        hotkey.key = Qt::Key_PageUp;
        return hotkey;
    case VK_NEXT:
        hotkey.key = Qt::Key_PageDown;
        return hotkey;
    case VK_END:
        hotkey.key = Qt::Key_End;
        return hotkey;
    case VK_HOME:
        hotkey.key = Qt::Key_Home;
        return hotkey;
    case VK_LEFT:
        hotkey.key = Qt::Key_Left;
        return hotkey;
    case VK_UP:
        hotkey.key = Qt::Key_Up;
        return hotkey;
    case VK_RIGHT:
        hotkey.key = Qt::Key_Right;
        return hotkey;
    case VK_DOWN:
        hotkey.key = Qt::Key_Down;
        return hotkey;
    case VK_INSERT:
        hotkey.key = Qt::Key_Insert;
        return hotkey;
    case VK_DELETE:
        hotkey.key = Qt::Key_Delete;
        return hotkey;
    case VK_NUMLOCK:
        hotkey.key = Qt::Key_NumLock;
        return hotkey;
    case VK_SCROLL:
        hotkey.key = Qt::Key_ScrollLock;
        return hotkey;
    case VK_OEM_1:
        hotkey.key = Qt::Key_Semicolon;
        return hotkey;
    case VK_OEM_PLUS:
        hotkey.key = Qt::Key_Equal;
        return hotkey;
    case VK_OEM_COMMA:
        hotkey.key = Qt::Key_Comma;
        return hotkey;
    case VK_OEM_MINUS:
        hotkey.key = Qt::Key_Minus;
        return hotkey;
    case VK_OEM_PERIOD:
        hotkey.key = Qt::Key_Period;
        return hotkey;
    case VK_OEM_2:
        hotkey.key = Qt::Key_Slash;
        return hotkey;
    case VK_OEM_3:
        hotkey.key = Qt::Key_QuoteLeft;
        return hotkey;
    case VK_OEM_4:
        hotkey.key = Qt::Key_BracketLeft;
        return hotkey;
    case VK_OEM_5:
        hotkey.key = Qt::Key_Backslash;
        return hotkey;
    case VK_OEM_6:
        hotkey.key = Qt::Key_BracketRight;
        return hotkey;
    case VK_OEM_7:
        hotkey.key = Qt::Key_Apostrophe;
        return hotkey;
    case VK_MULTIPLY:
        hotkey.key = Qt::Key_Asterisk;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case VK_ADD:
        hotkey.key = Qt::Key_Plus;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case VK_SUBTRACT:
        hotkey.key = Qt::Key_Minus;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case VK_DECIMAL:
        hotkey.key = Qt::Key_Period;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    case VK_DIVIDE:
        hotkey.key = Qt::Key_Slash;
        hotkey.modifiers = Qt::KeypadModifier;
        return hotkey;
    default:
        return std::nullopt;
    }
}

QString globalHotkeyShortcutText(const GlobalHotkeyItem& hotkey)
{
    if (!hotkey.keyCode.has_value() || hotkey.keyCode.value() == 0) {
        return QString();
    }

    const std::optional<QtHotkeyKey> qtHotkey = globalHotkeyKeyCodeToQtKey(hotkey.keyCode.value());
    if (!qtHotkey.has_value() || qtHotkey->key == Qt::Key_unknown) {
        return QString();
    }

    int sequenceValue = qtHotkey->key;
    if (hotkey.control) {
        sequenceValue |= Qt::CTRL;
    }
    if (hotkey.alt) {
        sequenceValue |= Qt::ALT;
    }
    if (hotkey.shift) {
        sequenceValue |= Qt::SHIFT;
    }
    if (qtHotkey->modifiers.testFlag(Qt::KeypadModifier)) {
        sequenceValue |= Qt::KeypadModifier;
    }
    return QKeySequence(sequenceValue).toString(QKeySequence::NativeText);
}
