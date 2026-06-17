#include "stdafx.h"
#include "HotkeyConfig.h"
#include <Windows.h>
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
};

// 根据虚拟键码获取字符串
static const wchar_t* VkToString(UINT vk)
{
    for (const auto& m : g_vkMap)
    {
        if (m.vk == vk) return m.name;
    }
    return L"?";
}

// 根据字符串获取虚拟键码
static UINT StringToVk(const std::wstring& str)
{
    for (const auto& m : g_vkMap)
    {
        if (str == m.name) return m.vk;
    }
    return 0;
}

std::wstring HotkeyConfigItem::ToHotkeyString() const
{
    std::wostringstream oss;
    if (modifiers & MOD_CONTROL) oss << L"Ctrl+";
    if (modifiers & MOD_ALT) oss << L"Alt+";
    if (modifiers & MOD_SHIFT) oss << L"Shift+";
    if (modifiers & MOD_WIN) oss << L"Win+";
    oss << VkToString(vk);
    return oss.str();
}

bool HotkeyConfigItem::ParseHotkeyString(const std::wstring& str, UINT& modifiers, UINT& vk)
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

HotkeyConfig::HotkeyConfig() {}
HotkeyConfig::~HotkeyConfig() {}

void HotkeyConfig::SetConfigPath(const std::wstring& path)
{
    m_configPath = path;
}

std::wstring HotkeyConfig::ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const
{
    // 脚本代码可能较长,使用 INI API 允许的最大缓冲区
    const DWORD bufSize = 32768;
    std::vector<wchar_t> buf(bufSize, 0);
    ::GetPrivateProfileStringW(section, key, defaultVal, buf.data(), bufSize, m_configPath.c_str());
    return std::wstring(buf.data());
}

int HotkeyConfig::ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const
{
    return static_cast<int>(::GetPrivateProfileIntW(section, key, defaultVal, m_configPath.c_str()));
}

bool HotkeyConfig::WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const
{
    return ::WritePrivateProfileStringW(section, key, value, m_configPath.c_str()) != 0;
}

bool HotkeyConfig::WriteInt(const wchar_t* section, const wchar_t* key, int value) const
{
    wchar_t buf[32] = { 0 };
    _itow_s(value, buf, 10);
    return ::WritePrivateProfileStringW(section, key, buf, m_configPath.c_str()) != 0;
}

bool HotkeyConfig::Load()
{
    m_items.clear();
    int count = ReadInt(L"General", L"Count", 0);
    if (count <= 0) return true;

    for (int i = 0; i < count; ++i)
    {
        std::wostringstream section;
        section << L"Item" << i;
        std::wstring sec = section.str();

        HotkeyConfigItem item;
        item.modifiers = static_cast<UINT>(ReadInt(sec.c_str(), L"Modifiers", 0));
        item.vk = static_cast<UINT>(ReadInt(sec.c_str(), L"VK", 0));
        item.scriptCode = ReadString(sec.c_str(), L"ScriptCode", L"");
        item.description = ReadString(sec.c_str(), L"Description", L"");
        item.enabled = ReadInt(sec.c_str(), L"Enabled", 1) != 0;

        if (item.IsValid())
        {
            m_items.push_back(item);
        }
    }
    return true;
}

bool HotkeyConfig::Save() const
{
    // 先写入数量
    WriteInt(L"General", L"Count", static_cast<int>(m_items.size()));

    // 写入每一项
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        std::wostringstream section;
        section << L"Item" << i;
        std::wstring sec = section.str();

        const HotkeyConfigItem& item = m_items[i];
        WriteInt(sec.c_str(), L"Modifiers", static_cast<int>(item.modifiers));
        WriteInt(sec.c_str(), L"VK", static_cast<int>(item.vk));
        WriteString(sec.c_str(), L"ScriptCode", item.scriptCode.c_str());
        WriteString(sec.c_str(), L"Description", item.description.c_str());
        WriteInt(sec.c_str(), L"Enabled", item.enabled ? 1 : 0);
    }
    return true;
}

void HotkeyConfig::AddItem(const HotkeyConfigItem& item)
{
    m_items.push_back(item);
}

void HotkeyConfig::RemoveItem(size_t index)
{
    if (index < m_items.size())
    {
        m_items.erase(m_items.begin() + index);
    }
}

void HotkeyConfig::UpdateItem(size_t index, const HotkeyConfigItem& item)
{
    if (index < m_items.size())
    {
        m_items[index] = item;
    }
}

int HotkeyConfig::GetEnabledCount() const
{
    int count = 0;
    for (const auto& item : m_items)
    {
        if (item.enabled) ++count;
    }
    return count;
}
