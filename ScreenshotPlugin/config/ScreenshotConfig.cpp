#include "../pch/stdafx.h"
#include "ScreenshotConfig.h"
#include <Windows.h>
#include <sstream>
#include <vector>

ScreenshotConfig::ScreenshotConfig() { ResetAllToDefault(); }
ScreenshotConfig::~ScreenshotConfig() {}

void ScreenshotConfig::SetConfigPath(const std::wstring& path)
{
    m_configPath = path;
}

std::wstring ScreenshotConfig::GetSectionName(ActionType a)
{
    switch (a)
    {
    case ActionType::Capture:            return L"Hotkey_Capture";
    case ActionType::Pin:                return L"Hotkey_Pin";
    case ActionType::Ocr:                return L"Hotkey_Ocr";
    case ActionType::Translate:          return L"Hotkey_Translate";
    case ActionType::Save:               return L"Hotkey_Save";
    case ActionType::QrCode:             return L"Hotkey_QrCode";
    case ActionType::ClipboardOcr:       return L"Hotkey_ClipboardOcr";
    case ActionType::ClipboardTranslate: return L"Hotkey_ClipboardTranslate";
    }
    return L"";
}

std::wstring ScreenshotConfig::ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const
{
    const DWORD bufSize = 4096;
    std::vector<wchar_t> buf(bufSize, 0);
    ::GetPrivateProfileStringW(section, key, defaultVal, buf.data(), bufSize, m_configPath.c_str());
    return std::wstring(buf.data());
}

int ScreenshotConfig::ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const
{
    return static_cast<int>(::GetPrivateProfileIntW(section, key, defaultVal, m_configPath.c_str()));
}

bool ScreenshotConfig::WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const
{
    return ::WritePrivateProfileStringW(section, key, value, m_configPath.c_str()) != 0;
}

bool ScreenshotConfig::WriteInt(const wchar_t* section, const wchar_t* key, int value) const
{
    wchar_t buf[32] = { 0 };
    _itow_s(value, buf, 10);
    return ::WritePrivateProfileStringW(section, key, buf, m_configPath.c_str()) != 0;
}

bool ScreenshotConfig::Load()
{
    // 加载各功能热键
    static const ActionType allActions[ACTION_COUNT] = {
        ActionType::Capture, ActionType::Pin, ActionType::Ocr, ActionType::Translate,
        ActionType::Save, ActionType::QrCode,
        ActionType::ClipboardOcr, ActionType::ClipboardTranslate
    };
    for (int i = 0; i < ACTION_COUNT; ++i)
    {
        ActionType a = allActions[i];
        std::wstring sec = GetSectionName(a);
        const DefaultHotkey& def = GetDefaultHotkey(a);

        ScreenshotHotkeyItem item;
        item.modifiers = static_cast<UINT>(ReadInt(sec.c_str(), L"Modifiers", def.modifiers));
        item.vk = static_cast<UINT>(ReadInt(sec.c_str(), L"VK", def.vk));
        item.enabled = ReadInt(sec.c_str(), L"Enabled", def.enabled ? 1 : 0) != 0;
        m_hotkeys[a] = item;
    }

    // 通用设置
    m_showToolbar = ReadInt(L"General", L"ShowToolbar", 1) != 0;
    m_defaultSaveDir = ReadString(L"General", L"DefaultSaveDir", L"");
    m_autoOcrLang = ReadString(L"General", L"AutoOcrLang", L"zh-Hans-CN");

    // 百度配置
    m_baidu.appId = ReadString(L"Baidu", L"AppId", L"");
    m_baidu.secretKey = ReadString(L"Baidu", L"SecretKey", L"");
    m_baidu.fromLang = ReadString(L"Baidu", L"FromLang", L"auto");
    m_baidu.toLang = ReadString(L"Baidu", L"ToLang", L"zh");

    return true;
}

bool ScreenshotConfig::Save() const
{
    // 写入各功能热键
    static const ActionType allActions[ACTION_COUNT] = {
        ActionType::Capture, ActionType::Pin, ActionType::Ocr, ActionType::Translate,
        ActionType::Save, ActionType::QrCode,
        ActionType::ClipboardOcr, ActionType::ClipboardTranslate
    };
    for (int i = 0; i < ACTION_COUNT; ++i)
    {
        ActionType a = allActions[i];
        std::wstring sec = GetSectionName(a);
        auto it = m_hotkeys.find(a);
        if (it == m_hotkeys.end()) continue;

        const ScreenshotHotkeyItem& item = it->second;
        WriteInt(sec.c_str(), L"Modifiers", static_cast<int>(item.modifiers));
        WriteInt(sec.c_str(), L"VK", static_cast<int>(item.vk));
        WriteInt(sec.c_str(), L"Enabled", item.enabled ? 1 : 0);
    }

    // 通用设置
    WriteInt(L"General", L"ShowToolbar", m_showToolbar ? 1 : 0);
    WriteString(L"General", L"DefaultSaveDir", m_defaultSaveDir.c_str());
    WriteString(L"General", L"AutoOcrLang", m_autoOcrLang.c_str());

    // 百度配置
    WriteString(L"Baidu", L"AppId", m_baidu.appId.c_str());
    WriteString(L"Baidu", L"SecretKey", m_baidu.secretKey.c_str());
    WriteString(L"Baidu", L"FromLang", m_baidu.fromLang.c_str());
    WriteString(L"Baidu", L"ToLang", m_baidu.toLang.c_str());

    return true;
}

const ScreenshotHotkeyItem& ScreenshotConfig::GetHotkey(ActionType a) const
{
    auto it = m_hotkeys.find(a);
    if (it != m_hotkeys.end()) return it->second;
    // 不应该到这里
    static ScreenshotHotkeyItem s_empty;
    return s_empty;
}

ScreenshotHotkeyItem& ScreenshotConfig::GetHotkeyRef(ActionType a)
{
    auto it = m_hotkeys.find(a);
    if (it != m_hotkeys.end()) return it->second;
    // 不应该到这里
    static ScreenshotHotkeyItem s_empty;
    return s_empty;
}

void ScreenshotConfig::SetHotkey(ActionType a, const ScreenshotHotkeyItem& item)
{
    m_hotkeys[a] = item;
}

void ScreenshotConfig::ResetToDefault(ActionType a)
{
    const DefaultHotkey& def = GetDefaultHotkey(a);
    ScreenshotHotkeyItem item;
    item.modifiers = def.modifiers;
    item.vk = def.vk;
    item.enabled = def.enabled;
    m_hotkeys[a] = item;
}

void ScreenshotConfig::ResetAllToDefault()
{
    static const ActionType allActions[ACTION_COUNT] = {
        ActionType::Capture, ActionType::Pin, ActionType::Ocr, ActionType::Translate,
        ActionType::Save, ActionType::QrCode,
        ActionType::ClipboardOcr, ActionType::ClipboardTranslate
    };
    for (int i = 0; i < ACTION_COUNT; ++i)
    {
        ResetToDefault(allActions[i]);
    }
}
