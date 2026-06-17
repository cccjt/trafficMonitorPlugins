#pragma once
#include <string>
#include <vector>

// 单条热键配置项
struct HotkeyConfigItem
{
    UINT modifiers = 0;         // MOD_CONTROL/MOD_ALT/MOD_SHIFT/MOD_WIN 组合
    UINT vk = 0;                // 虚拟键码
    std::wstring scriptPath;    // PowerShell 脚本路径
    std::wstring description;   // 描述
    bool enabled = true;        // 是否启用

    // 判断是否有效(至少要有键码)
    bool IsValid() const { return vk != 0; }

    // 转换为可显示的热键字符串,如 "Ctrl+Alt+P"
    std::wstring ToHotkeyString() const;

    // 从字符串解析热键(逆向操作)
    static bool ParseHotkeyString(const std::wstring& str, UINT& modifiers, UINT& vk);
};

// 配置管理类:负责 INI 文件读写
class HotkeyConfig
{
public:
    HotkeyConfig();
    ~HotkeyConfig();

    // 设置配置文件路径(由主插件类调用,基于插件目录)
    void SetConfigPath(const std::wstring& path);

    // 加载配置
    bool Load();

    // 保存配置
    bool Save() const;

    // 获取热键列表
    const std::vector<HotkeyConfigItem>& GetItems() const { return m_items; }
    std::vector<HotkeyConfigItem>& GetItems() { return m_items; }

    // 添加/删除/修改
    void AddItem(const HotkeyConfigItem& item);
    void RemoveItem(size_t index);
    void UpdateItem(size_t index, const HotkeyConfigItem& item);

    // 统计信息
    int GetEnabledCount() const;

private:
    std::wstring m_configPath;                  // 配置文件完整路径
    std::vector<HotkeyConfigItem> m_items;      // 热键列表

    // INI 读写辅助
    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const;
    int ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const;
    bool WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const;
    bool WriteInt(const wchar_t* section, const wchar_t* key, int value) const;
};
