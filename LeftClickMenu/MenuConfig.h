#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// 单条菜单配置项(同一项既可以显示在左键菜单中,也可以绑定全局热键)
struct MenuConfigItem
{
    std::wstring name;          // 菜单项显示名称
    std::wstring iconSource;    // 图标来源:内置名称(script/folder/program/url/settings)或文件路径
    int iconIndex = 0;          // 图标索引(文件加载时使用,默认 0)
    std::wstring scriptCode;    // PowerShell 脚本代码(内存中为明文,INI 中以 Base64 存储)
    bool enabled = true;        // 是否启用
    bool isSeparator = false;   // 是否为分隔线

    UINT hotkeyModifiers = 0;   // 热键修饰键(MOD_CONTROL/MOD_ALT/MOD_SHIFT/MOD_WIN)
    UINT hotkeyVk = 0;          // 热键虚拟键码,0 表示未设置热键
    bool showInMenu = true;     // 是否显示在左键菜单中(仅设置了热键的项可以隐藏)

    // 判断是否为分隔线
    bool IsSeparator() const { return isSeparator; }

    // 是否设置了热键
    bool HasHotkey() const { return !isSeparator && hotkeyVk != 0; }

    // 获取热键显示文本,如 "Ctrl+Alt+P";未设置时返回空字符串
    std::wstring GetHotkeyString() const { return HotkeyToString(hotkeyModifiers, hotkeyVk); }

    // 将修饰键和虚拟键码转换为显示文本
    static std::wstring HotkeyToString(UINT modifiers, UINT vk);
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

    // 统计信息(启用且显示在左键菜单中的非分隔线项)
    int GetVisibleMenuCount() const;

    // 面板显示设置
    int GetPanelWidth() const { return m_panelWidth; }          // 96 DPI 下的宽度像素
    void SetPanelWidth(int width);                              // 越界时恢复默认值
    int GetTileColumns() const { return m_tileColumns; }        // 平铺模式每行列数
    void SetTileColumns(int columns);                           // 越界时恢复默认值
    bool IsTileMode() const { return m_viewMode == 1; }         // 是否平铺显示
    void SetTileMode(bool tileMode) { m_viewMode = tileMode ? 1 : 0; }

    // 一次性导入旧版 HotkeyPlugin.ini
    // 脚本相同的项合并并挂上热键,其余热键项追加为菜单项
    // 存在旧配置文件(且尚未迁移)时返回 true,调用方负责备份旧文件并保存配置
    bool ImportLegacyHotkeyConfig(const std::wstring& hotkeyIniPath);

private:
    std::wstring m_configPath;                  // 配置文件完整路径
    std::vector<MenuConfigItem> m_items;        // 菜单项列表
    bool m_legacyImported = false;              // 是否已经完成旧热键配置迁移

    int m_panelWidth = 200;                     // 面板宽度(96 DPI 像素)
    int m_tileColumns = 4;                      // 平铺模式每行列数
    int m_viewMode = 0;                         // 显示模式:0 列表,1 平铺

    // INI 读写辅助
    std::wstring ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const;
    int ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const;
    bool WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const;
    bool WriteInt(const wchar_t* section, const wchar_t* key, int value) const;
};
