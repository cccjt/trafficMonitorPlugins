#pragma once
#include <string>
#include <map>
#include "ActionType.h"

// 百度翻译 API 配置
struct BaiduConfig
{
    std::wstring appId;
    std::wstring secretKey;
    std::wstring fromLang = L"auto";
    std::wstring toLang = L"zh";
};

// 截图插件配置:管理 8 个功能热键 + 百度配置 + 行为开关
class ScreenshotConfig
{
public:
    ScreenshotConfig();
    ~ScreenshotConfig();

    // 设置配置文件路径(由主插件类调用,基于插件目录)
    void SetConfigPath(const std::wstring& path);

    // 加载配置
    bool Load();

    // 保存配置
    bool Save() const;

    // 获取某个功能的热键
    const ScreenshotHotkeyItem& GetHotkey(ActionType a) const;
    ScreenshotHotkeyItem& GetHotkeyRef(ActionType a);

    // 设置某个功能的热键
    void SetHotkey(ActionType a, const ScreenshotHotkeyItem& item);

    // 重置某功能为默认热键
    void ResetToDefault(ActionType a);

    // 重置所有为默认
    void ResetAllToDefault();

    // 获取所有热键(用于 HotkeyManager 注册)
    const std::map<ActionType, ScreenshotHotkeyItem>& GetAllHotkeys() const { return m_hotkeys; }

    // 行为开关
    bool GetShowToolbar() const { return m_showToolbar; }
    void SetShowToolbar(bool v) { m_showToolbar = v; }

    std::wstring GetDefaultSaveDir() const { return m_defaultSaveDir; }
    void SetDefaultSaveDir(const std::wstring& v) { m_defaultSaveDir = v; }

    std::wstring GetAutoOcrLang() const { return m_autoOcrLang; }
    void SetAutoOcrLang(const std::wstring& v) { m_autoOcrLang = v; }

    // 百度翻译配置
    const BaiduConfig& GetBaidu() const { return m_baidu; }
    BaiduConfig& GetBaiduRef() { return m_baidu; }
    void SetBaidu(const BaiduConfig& b) { m_baidu = b; }

private:
    // INI 段名,如 "Hotkey_Capture"
    static std::wstring GetSectionName(ActionType a);

    // INI 读写辅助
    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const;
    int ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const;
    bool WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const;
    bool WriteInt(const wchar_t* section, const wchar_t* key, int value) const;

private:
    std::wstring m_configPath;
    std::map<ActionType, ScreenshotHotkeyItem> m_hotkeys;

    bool m_showToolbar = true;
    std::wstring m_defaultSaveDir;
    std::wstring m_autoOcrLang = L"zh-Hans-CN";

    BaiduConfig m_baidu;
};
