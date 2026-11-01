#include "stdafx.h"
#include "MenuConfig.h"
#include "Logger.h"
#include <Windows.h>
#include <wincrypt.h>
#include <sstream>
#include <vector>
#include <cstring>

#pragma comment(lib, "Crypt32.lib")

namespace
{
const std::wstring SCRIPT_BASE64_PREFIX = L"B64:";

// 虚拟键码到显示名称的映射(常用按键)
struct VkNameEntry
{
    UINT vk;
    const wchar_t* name;
};

const VkNameEntry kHotkeyVkMap[] = {
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

const wchar_t* VkToDisplayName(UINT vk)
{
    for (const auto& entry : kHotkeyVkMap)
    {
        if (entry.vk == vk) return entry.name;
    }
    return nullptr;
}

// 去掉脚本首尾空白,用于判断两个脚本是否相同
std::wstring TrimScript(const std::wstring& script)
{
    size_t begin = script.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) return std::wstring();
    size_t end = script.find_last_not_of(L" \t\r\n");
    return script.substr(begin, end - begin + 1);
}

// 读取任意 INI 文件的字符串(用于导入旧版 HotkeyPlugin.ini)
std::wstring ReadIniStringFrom(const std::wstring& file, const wchar_t* section,
    const wchar_t* key, const wchar_t* defaultVal)
{
    const DWORD bufSize = 32768;
    std::vector<wchar_t> buf(bufSize, 0);
    ::GetPrivateProfileStringW(section, key, defaultVal, buf.data(), bufSize, file.c_str());
    return std::wstring(buf.data());
}

int ReadIniIntFrom(const std::wstring& file, const wchar_t* section, const wchar_t* key, int defaultVal)
{
    return static_cast<int>(::GetPrivateProfileIntW(section, key, defaultVal, file.c_str()));
}

// 判断 INI 文件中是否存在指定段
bool IniSectionExists(const std::wstring& file, const wchar_t* section)
{
    std::vector<wchar_t> buf(4096, 0);
    return ::GetPrivateProfileSectionW(section, buf.data(),
        static_cast<DWORD>(buf.size()), file.c_str()) > 0;
}

// 脚本按 UTF-16LE 编码后转成 Base64,避免 INI 无法保存多行值
std::wstring EncodeScript(const std::wstring& script)
{
    if (script.empty()) return std::wstring();

    std::vector<BYTE> data(script.size() * sizeof(wchar_t));
    std::memcpy(data.data(), script.data(), data.size());

    DWORD base64Length = 0;
    if (!::CryptBinaryToStringW(data.data(), static_cast<DWORD>(data.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Length)
        || base64Length == 0)
    {
        Logger::Instance().Warn(L"MenuConfig::EncodeScript: Base64 编码失败");
        return std::wstring();
    }

    std::vector<wchar_t> buffer(base64Length + 1, 0);
    if (!::CryptBinaryToStringW(data.data(), static_cast<DWORD>(data.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, buffer.data(), &base64Length))
    {
        Logger::Instance().Warn(L"MenuConfig::EncodeScript: Base64 编码失败,按明文保存");
        return script;
    }

    std::wstring base64(buffer.data(), base64Length);
    while (!base64.empty() && base64.back() == L'\0')
    {
        base64.pop_back();
    }
    return SCRIPT_BASE64_PREFIX + base64;
}

// 解码 Base64 脚本;没有 Base64 前缀时按旧版明文配置处理
std::wstring DecodeScript(const std::wstring& value)
{
    if (value.size() <= SCRIPT_BASE64_PREFIX.size()
        || value.compare(0, SCRIPT_BASE64_PREFIX.size(), SCRIPT_BASE64_PREFIX) != 0)
    {
        return value;
    }

    std::wstring base64 = value.substr(SCRIPT_BASE64_PREFIX.size());

    DWORD dataSize = 0;
    if (!::CryptStringToBinaryW(base64.c_str(), static_cast<DWORD>(base64.size()),
            CRYPT_STRING_BASE64, nullptr, &dataSize, nullptr, nullptr)
        || dataSize == 0)
    {
        Logger::Instance().Warn(L"MenuConfig::DecodeScript: Base64 解码失败,按明文处理");
        return value;
    }

    std::vector<BYTE> data(dataSize, 0);
    if (!::CryptStringToBinaryW(base64.c_str(), static_cast<DWORD>(base64.size()),
            CRYPT_STRING_BASE64, data.data(), &dataSize, nullptr, nullptr))
    {
        Logger::Instance().Warn(L"MenuConfig::DecodeScript: Base64 解码失败,按明文处理");
        return value;
    }

    if (dataSize % sizeof(wchar_t) != 0)
    {
        Logger::Instance().Warn(L"MenuConfig::DecodeScript: 脚本数据长度无效,按明文处理");
        return value;
    }

    return std::wstring(reinterpret_cast<const wchar_t*>(data.data()),
        dataSize / sizeof(wchar_t));
}
}

std::wstring MenuConfigItem::HotkeyToString(UINT modifiers, UINT vk)
{
    if (vk == 0) return std::wstring();

    std::wostringstream oss;
    if (modifiers & MOD_CONTROL) oss << L"Ctrl+";
    if (modifiers & MOD_ALT) oss << L"Alt+";
    if (modifiers & MOD_SHIFT) oss << L"Shift+";
    if (modifiers & MOD_WIN) oss << L"Win+";

    const wchar_t* name = VkToDisplayName(vk);
    if (name != nullptr)
    {
        oss << name;
    }
    else
    {
        // 未收录的按键尝试用系统名称
        wchar_t keyName[64] = { 0 };
        UINT scanCode = ::MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        LONG lParam = static_cast<LONG>(scanCode) << 16;
        if (::GetKeyNameTextW(lParam, keyName, 64) != 0 && keyName[0] != L'\0')
        {
            oss << keyName;
        }
        else
        {
            oss << L"VK " << vk;
        }
    }
    return oss.str();
}

MenuConfig::MenuConfig() {}
MenuConfig::~MenuConfig() {}

void MenuConfig::SetConfigPath(const std::wstring& path)
{
    m_configPath = path;
}

void MenuConfig::SetPanelWidth(int width)
{
    // 宽度限制在 160~600(96 DPI),越界或非法值恢复默认 200
    if (width < 160 || width > 600) width = 200;
    m_panelWidth = width;
}

void MenuConfig::SetTileColumns(int columns)
{
    // 每行列数限制在 2~8,越界或非法值恢复默认 4
    if (columns < 2 || columns > 8) columns = 4;
    m_tileColumns = columns;
}

std::wstring MenuConfig::ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const
{
    // 脚本代码可能较长,使用 INI API 允许的最大缓冲区
    const DWORD bufSize = 32768;
    std::vector<wchar_t> buf(bufSize, 0);
    ::GetPrivateProfileStringW(section, key, defaultVal, buf.data(), bufSize, m_configPath.c_str());
    return std::wstring(buf.data());
}

int MenuConfig::ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const
{
    return static_cast<int>(::GetPrivateProfileIntW(section, key, defaultVal, m_configPath.c_str()));
}

bool MenuConfig::WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const
{
    return ::WritePrivateProfileStringW(section, key, value, m_configPath.c_str()) != 0;
}

bool MenuConfig::WriteInt(const wchar_t* section, const wchar_t* key, int value) const
{
    wchar_t buf[32] = { 0 };
    _itow_s(value, buf, 10);
    return ::WritePrivateProfileStringW(section, key, buf, m_configPath.c_str()) != 0;
}

bool MenuConfig::Load()
{
    m_items.clear();

    if (m_configPath.empty())
    {
        Logger::Instance().Warn(L"MenuConfig::Load: 配置文件路径为空");
        return false;
    }

    m_legacyImported = ReadInt(L"General", L"MergedHotkeys", 0) != 0;

    // 面板显示设置
    SetPanelWidth(ReadInt(L"General", L"PanelWidth", 200));
    SetTileColumns(ReadInt(L"General", L"TileColumns", 4));
    m_viewMode = ReadInt(L"General", L"ViewMode", 0) == 1 ? 1 : 0;

    int count = ReadInt(L"General", L"Count", 0);
    if (count <= 0)
    {
        Logger::Instance().Info(L"MenuConfig::Load: 配置为空或首次加载");
        return true;
    }

    // 限制最大菜单项数量,防止 INI 损坏导致异常
    const int MAX_ITEMS = 500;
    if (count > MAX_ITEMS)
    {
        Logger::Instance().Warn(L"MenuConfig::Load: 菜单项数量超过上限 " + std::to_wstring(MAX_ITEMS) + L", 已截断");
        count = MAX_ITEMS;
    }

    for (int i = 0; i < count; ++i)
    {
        std::wostringstream section;
        section << L"MenuItem_" << i;
        std::wstring sec = section.str();

        MenuConfigItem item;
        item.name = ReadString(sec.c_str(), L"Name", L"");
        item.iconSource = ReadString(sec.c_str(), L"IconSource", L"");
        item.iconIndex = ReadInt(sec.c_str(), L"IconIndex", 0);
        item.scriptCode = DecodeScript(ReadString(sec.c_str(), L"ScriptCode", L""));
        item.enabled = ReadInt(sec.c_str(), L"Enabled", 1) != 0;
        item.isSeparator = ReadInt(sec.c_str(), L"IsSeparator", 0) != 0;
        item.hotkeyModifiers = static_cast<UINT>(ReadInt(sec.c_str(), L"HotkeyModifiers", 0));
        item.hotkeyVk = static_cast<UINT>(ReadInt(sec.c_str(), L"HotkeyVk", 0));
        item.showInMenu = ReadInt(sec.c_str(), L"ShowInMenu", 1) != 0;

        // 分隔线不参与热键与菜单显示;没有热键的项无法从菜单之外触发,必须显示在菜单中
        if (item.isSeparator)
        {
            item.hotkeyModifiers = 0;
            item.hotkeyVk = 0;
            item.showInMenu = true;
        }
        else if (item.hotkeyVk == 0)
        {
            item.showInMenu = true;
        }

        // 跳过完全空的项(可能 INI 被损坏)
        if (item.name.empty() && item.scriptCode.empty() && !item.isSeparator)
        {
            Logger::Instance().Warn(L"MenuConfig::Load: 跳过空的菜单项 #" + std::to_wstring(i));
            continue;
        }

        m_items.push_back(item);
    }

    Logger::Instance().Info(L"MenuConfig::Load: 已加载 " + std::to_wstring(m_items.size()) + L" 个菜单项");
    return true;
}

bool MenuConfig::Save() const
{
    if (m_configPath.empty())
    {
        Logger::Instance().Warn(L"MenuConfig::Save: 配置文件路径为空");
        return false;
    }

    // 先写入数量和迁移标记
    WriteInt(L"General", L"Count", static_cast<int>(m_items.size()));
    if (m_legacyImported)
    {
        WriteInt(L"General", L"MergedHotkeys", 1);
    }
    WriteInt(L"General", L"PanelWidth", m_panelWidth);
    WriteInt(L"General", L"TileColumns", m_tileColumns);
    WriteInt(L"General", L"ViewMode", m_viewMode);

    // 写入每一项
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        std::wostringstream section;
        section << L"MenuItem_" << i;
        std::wstring sec = section.str();

        const MenuConfigItem& item = m_items[i];
        WriteString(sec.c_str(), L"Name", item.name.c_str());
        WriteString(sec.c_str(), L"IconSource", item.iconSource.c_str());
        WriteInt(sec.c_str(), L"IconIndex", item.iconIndex);
        WriteString(sec.c_str(), L"ScriptCode", EncodeScript(item.scriptCode).c_str());
        WriteInt(sec.c_str(), L"Enabled", item.enabled ? 1 : 0);
        WriteInt(sec.c_str(), L"IsSeparator", item.isSeparator ? 1 : 0);
        WriteInt(sec.c_str(), L"HotkeyModifiers", static_cast<int>(item.hotkeyModifiers));
        WriteInt(sec.c_str(), L"HotkeyVk", static_cast<int>(item.hotkeyVk));
        WriteInt(sec.c_str(), L"ShowInMenu", item.showInMenu ? 1 : 0);
    }

    // 清理多余的旧项(如果新数量比旧数量少)
    for (size_t i = m_items.size(); i < m_items.size() + 50; ++i)
    {
        std::wostringstream section;
        section << L"MenuItem_" << i;
        std::wstring sec = section.str();
        // 尝试删除旧项,忽略失败
        WriteString(sec.c_str(), nullptr, nullptr);
    }

    Logger::Instance().Info(L"MenuConfig::Save: 已保存 " + std::to_wstring(m_items.size()) + L" 个菜单项");
    return true;
}

void MenuConfig::AddItem(const MenuConfigItem& item)
{
    m_items.push_back(item);
}

void MenuConfig::RemoveItem(size_t index)
{
    if (index < m_items.size())
    {
        m_items.erase(m_items.begin() + index);
    }
}

void MenuConfig::UpdateItem(size_t index, const MenuConfigItem& item)
{
    if (index < m_items.size())
    {
        m_items[index] = item;
    }
}

int MenuConfig::GetVisibleMenuCount() const
{
    int count = 0;
    for (const auto& item : m_items)
    {
        // 只统计实际显示在左键菜单中的可执行项
        if (item.enabled && item.showInMenu && !item.isSeparator) ++count;
    }
    return count;
}

bool MenuConfig::ImportLegacyHotkeyConfig(const std::wstring& hotkeyIniPath)
{
    if (m_legacyImported) return false;
    if (::GetFileAttributesW(hotkeyIniPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    m_legacyImported = true;

    int mergedCount = 0;
    int appendedCount = 0;
    const int MAX_ITEMS = 500;

    // 旧版配置中的 Count 可能不准确,这里按连续存在的 [ItemN] 扫描
    for (int i = 0; i < MAX_ITEMS; ++i)
    {
        std::wstring section = L"Item" + std::to_wstring(i);
        if (!IniSectionExists(hotkeyIniPath, section.c_str())) break;

        UINT vk = static_cast<UINT>(ReadIniIntFrom(hotkeyIniPath, section.c_str(), L"VK", 0));
        if (vk == 0) continue;

        UINT modifiers = static_cast<UINT>(
            ReadIniIntFrom(hotkeyIniPath, section.c_str(), L"Modifiers", 0));
        std::wstring script = ReadIniStringFrom(hotkeyIniPath, section.c_str(), L"ScriptCode", L"");
        if (script.empty())
        {
            Logger::Instance().Warn(L"MenuConfig::ImportLegacyHotkeyConfig: 跳过空脚本的旧热键项 ["
                + section + L"]");
            continue;
        }

        std::wstring description = ReadIniStringFrom(hotkeyIniPath, section.c_str(),
            L"Description", L"");
        bool enabled = ReadIniIntFrom(hotkeyIniPath, section.c_str(), L"Enabled", 1) != 0;

        // 脚本相同的菜单项直接挂上热键
        bool merged = false;
        std::wstring trimmedScript = TrimScript(script);
        for (auto& item : m_items)
        {
            if (item.isSeparator || item.hotkeyVk != 0) continue;
            if (TrimScript(item.scriptCode) != trimmedScript) continue;

            item.hotkeyModifiers = modifiers;
            item.hotkeyVk = vk;
            merged = true;
            ++mergedCount;
            Logger::Instance().Info(L"MenuConfig::ImportLegacyHotkeyConfig: 合并热键 ["
                + item.GetHotkeyString() + L"] 到菜单项 [" + item.name + L"]");
            break;
        }
        if (merged) continue;

        if (m_items.size() >= static_cast<size_t>(MAX_ITEMS))
        {
            Logger::Instance().Warn(L"MenuConfig::ImportLegacyHotkeyConfig: 菜单项数量达到上限,停止导入");
            break;
        }

        // 其余热键项追加为菜单项
        MenuConfigItem newItem;
        newItem.name = TrimScript(description);
        if (newItem.name.empty())
        {
            newItem.name = L"热键脚本" + std::to_wstring(i + 1);
        }
        newItem.iconSource = L"script";
        newItem.scriptCode = script;
        newItem.enabled = enabled;
        newItem.hotkeyModifiers = modifiers;
        newItem.hotkeyVk = vk;
        newItem.showInMenu = true;
        m_items.push_back(newItem);
        ++appendedCount;

        Logger::Instance().Info(L"MenuConfig::ImportLegacyHotkeyConfig: 追加菜单项 ["
            + newItem.name + L"], 热键 [" + newItem.GetHotkeyString() + L"]");
    }

    Logger::Instance().Info(L"MenuConfig::ImportLegacyHotkeyConfig: 旧热键配置导入完成, 合并 "
        + std::to_wstring(mergedCount) + L" 项, 新增 "
        + std::to_wstring(appendedCount) + L" 项");
    return true;
}
