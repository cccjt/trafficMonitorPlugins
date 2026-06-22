#pragma once
#include <string>
#include <vector>

// 单条菜单配置项
struct MenuConfigItem
{
    std::wstring name;          // 菜单项显示名称
    std::wstring iconSource;    // 图标来源:内置名称(script/folder/program/url/settings)或文件路径
    int iconIndex = 0;          // 图标索引(文件加载时使用,默认 0)
    std::wstring scriptCode;    // PowerShell 脚本代码(Base64 编码存储)
    bool enabled = true;        // 是否启用
    bool isSeparator = false;   // 是否为分隔线

    // 判断是否为分隔线
    bool IsSeparator() const { return isSeparator; }
};

// 配置管理类:负责 INI 文件读写
class MenuConfig
{
public:
    MenuConfig();
    ~MenuConfig();

    // 设置配置文件路径(由主插件类调用,基于插件目录)
    void SetConfigPath(const std::wstring& path);

    // 检查路径是否已设置
    bool IsPathSet() const { return !m_configPath.empty(); }

    // 加载配置
    bool Load();

    // 保存配置
    bool Save() const;

    // 获取菜单项列表
    const std::vector<MenuConfigItem>& GetItems() const { return m_items; }
    std::vector<MenuConfigItem>& GetItems() { return m_items; }

    // 添加/删除/修改
    void AddItem(const MenuConfigItem& item);
    void RemoveItem(size_t index);
    void UpdateItem(size_t index, const MenuConfigItem& item);

    // 统计信息
    int GetEnabledCount() const;

private:
    std::wstring m_configPath;                  // 配置文件完整路径
    std::vector<MenuConfigItem> m_items;        // 菜单项列表

    // INI 读写辅助
    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const;
    int ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const;
    bool WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const;
    bool WriteInt(const wchar_t* section, const wchar_t* key, int value) const;
};
