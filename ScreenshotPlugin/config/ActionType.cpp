#include "../pch/stdafx.h"
#include "ActionType.h"
#include <sstream>

// 虚拟键码到字符串的映射(常用按键)
struct VkStringMap
{
    UINT vk;
    const wchar_t* name;
};

static const VkStringMap g_vkMap[] = {
    { VK_F1, L"F1" }, { VK_F2, L"F2" }, { VK_F3, L"F3" }, { VK_F4, L"F4" },
    { VK_F5, L"F5" }, { VK_F6, L"F6" }, { VK_F7, L"F7" }, { VK_F8, L"F8" },
    { VK_F9, L"F9" }, { VK_F10, L"F10" }, { VK_F11, L"F11" }, { VK_F12, L"F12" },
    { '0', L"0" }, { '1', L"1" }, { '2', L"2" }, { '3', L"3" }, { '4', L"4" },
    { '5', L"5" }, { '6', L"6" }, { '7', L"7" }, { '8', L"8" }, { '9', L"9" },
    { 'A', L"A" }, { 'B', L"B" }, { 'C', L"C" }, { 'D', L"D" }, { 'E', L"E" },
    { 'F', L"F" }, { 'G', L"G" }, { 'H', L"H" }, { 'I', L"I" }, { 'J', L"J" },
    { 'K', L"K" }, { 'L', L"L" }, { 'M', L"M" }, { 'N', L"N" }, { 'O', L"O" },
    { 'P', L"P" }, { 'Q', L"Q" }, { 'R', L"R" }, { 'S', L"S" }, { 'T', L"T" },
    { 'U', L"U" }, { 'V', L"V" }, { 'W', L"W" }, { 'X', L"X" }, { 'Y', L"Y" },
    { 'Z', L"Z" },
    { VK_OEM_3, L"`" }, { VK_OEM_MINUS, L"-" }, { VK_OEM_PLUS, L"=" },
    { VK_OEM_4, L"[" }, { VK_OEM_6, L"]" }, { VK_OEM_5, L"\\" },
    { VK_OEM_1, L";" }, { VK_OEM_7, L"'" }, { VK_OEM_COMMA, L"," },
    { VK_OEM_PERIOD, L"." }, { VK_OEM_2, L"/" },
    { VK_SPACE, L"Space" }, { VK_TAB, L"Tab" }, { VK_RETURN, L"Enter" },
    { VK_ESCAPE, L"Esc" }, { VK_BACK, L"Backspace" },
    { VK_INSERT, L"Insert" }, { VK_DELETE, L"Delete" },
    { VK_HOME, L"Home" }, { VK_END, L"End" },
    { VK_PRIOR, L"PageUp" }, { VK_NEXT, L"PageDown" },
    { VK_LEFT, L"Left" }, { VK_RIGHT, L"Right" }, { VK_UP, L"Up" }, { VK_DOWN, L"Down" },
    { VK_NUMPAD0, L"Num0" }, { VK_NUMPAD1, L"Num1" }, { VK_NUMPAD2, L"Num2" },
    { VK_NUMPAD3, L"Num3" }, { VK_NUMPAD4, L"Num4" }, { VK_NUMPAD5, L"Num5" },
    { VK_NUMPAD6, L"Num6" }, { VK_NUMPAD7, L"Num7" }, { VK_NUMPAD8, L"Num8" },
    { VK_NUMPAD9, L"Num9" },
    { VK_PAUSE, L"Pause" }, { VK_SNAPSHOT, L"PrintScreen" },
    { VK_SCROLL, L"ScrollLock" }, { VK_NUMLOCK, L"NumLock" },
    { VK_CAPITAL, L"CapsLock" }, { VK_MULTIPLY, L"Num*" },
    { VK_ADD, L"Num+" }, { VK_SUBTRACT, L"Num-" },
    { VK_DECIMAL, L"Num." }, { VK_DIVIDE, L"Num/" },
};

static const wchar_t* VkToString(UINT vk)
{
    for (const auto& m : g_vkMap)
    {
        if (m.vk == vk) return m.name;
    }
    return L"?";
}

static UINT StringToVk(const std::wstring& str)
{
    for (const auto& m : g_vkMap)
    {
        if (str == m.name) return m.vk;
    }
    return 0;
}

std::wstring ScreenshotHotkeyItem::ToHotkeyString() const
{
    if (vk == 0) return L"(未设置)";
    std::wostringstream oss;
    if (modifiers & MOD_CONTROL) oss << L"Ctrl+";
    if (modifiers & MOD_ALT) oss << L"Alt+";
    if (modifiers & MOD_SHIFT) oss << L"Shift+";
    if (modifiers & MOD_WIN) oss << L"Win+";
    oss << VkToString(vk);
    return oss.str();
}

bool ScreenshotHotkeyItem::ParseHotkeyString(const std::wstring& str, UINT& modifiers, UINT& vk)
{
    modifiers = 0;
    vk = 0;
    if (str.empty()) return false;

    std::wstring s = str;
    size_t pos = 0;
    while (true)
    {
        size_t plus = s.find(L'+', pos);
        if (plus == std::wstring::npos) break;
        std::wstring token = s.substr(pos, plus - pos);
        if (token == L"Ctrl") modifiers |= MOD_CONTROL;
        else if (token == L"Alt") modifiers |= MOD_ALT;
        else if (token == L"Shift") modifiers |= MOD_SHIFT;
        else if (token == L"Win") modifiers |= MOD_WIN;
        pos = plus + 1;
    }
    std::wstring last = s.substr(pos);
    vk = StringToVk(last);
    return vk != 0;
}

// 默认热键配置表
// Capture:            Ctrl+Alt+A   (MOD_CONTROL|MOD_ALT = 6)
// Pin:                Ctrl+Alt+P
// Ocr:                Ctrl+Alt+O
// Translate:          Ctrl+Alt+T
// Save:               Ctrl+Alt+S   (默认禁用)
// QrCode:             Ctrl+Alt+Q   (默认禁用)
// ClipboardOcr:       Ctrl+Alt+Shift+O  (MOD_CONTROL|MOD_ALT|MOD_SHIFT = 7)
// ClipboardTranslate: Ctrl+Alt+Shift+T  (默认禁用)
static const DefaultHotkey g_defaults[ACTION_COUNT] = {
    { 6, 'A', true  },  // Capture
    { 6, 'P', true  },  // Pin
    { 6, 'O', true  },  // Ocr
    { 6, 'T', true  },  // Translate
    { 6, 'S', false },  // Save
    { 6, 'Q', false },  // QrCode
    { 7, 'O', false },  // ClipboardOcr
    { 7, 'T', false },  // ClipboardTranslate
};

const DefaultHotkey& GetDefaultHotkey(ActionType a)
{
    int idx = static_cast<int>(a);
    if (idx < 0 || idx >= ACTION_COUNT)
    {
        // 兜底返回第一个
        return g_defaults[0];
    }
    return g_defaults[idx];
}
