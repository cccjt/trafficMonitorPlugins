#pragma once
#include "PluginInterface.h"
#include "MenuConfig.h"
#include "MenuManager.h"
#include "HotkeyManager.h"
#include "MenuItem.h"
#include "IconManager.h"
#include "TaskbarOverlay.h"
#include <string>

// 主插件类:实现 ITMPlugin 接口
class MenuPlugin : public ITMPlugin
{
public:
    // 获取单例实例
    static MenuPlugin& Instance();

    // ITMPlugin 接口实现
    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    void OnInitialize(ITrafficMonitor* pApp) override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

    // 供 MenuItem 调用
    // 获取显示文本(菜单项数量)
    std::wstring GetDisplayText() const;

    // 由显示项目触发的左键菜单弹出(屏幕坐标)
    void ShowMenuFromItem(void* hWnd, int screenX, int screenY);

    // 由任务栏覆盖窗触发的菜单弹出(屏幕坐标)
    void ShowMenuFromTaskbar(HWND hTargetWnd, int screenX, int screenY);

    // 静态回调函数,供 TaskbarOverlay 调用
    static void TaskbarLeftClickCallback(HWND hTargetWnd, int x, int y);

    // 获取配置(供 MenuManager 使用)
    MenuConfig& GetConfig() { return m_config; }
    MenuManager& GetManager() { return m_manager; }

private:
    MenuPlugin();
    ~MenuPlugin();

    // 按当前配置重新注册全局热键
    void RegisterHotkeys();

    // 构造左键菜单中实际显示的项(过滤隐藏项并整理分隔线)
    std::vector<MenuConfigItem> BuildVisibleMenuItems() const;

private:
    MenuConfig m_config;            // 配置
    MenuManager m_manager;          // 菜单管理器
    HotkeyManager m_hotkeyManager;  // 全局热键管理器
    MenuItem m_item;                // 显示项目
    ITrafficMonitor* m_pApp = nullptr;  // 主程序接口

    bool m_initialized = false;     // 是否已初始化
    bool m_overlayInstalled = false;    // 任务栏覆盖窗是否已创建
    DWORD m_lastOverlayRetryTick = 0;   // 上次尝试创建覆盖窗的时刻
};
