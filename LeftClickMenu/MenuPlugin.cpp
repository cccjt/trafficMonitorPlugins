#include "stdafx.h"
#include "MenuPlugin.h"
#include "OptionsDialog.h"
#include "IconManager.h"
#include "Logger.h"
#include <Windows.h>

// 导出函数
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) ITMPlugin* TMPluginGetInstance();
#ifdef __cplusplus
}
#endif

ITMPlugin* TMPluginGetInstance()
{
    return &MenuPlugin::Instance();
}

MenuPlugin& MenuPlugin::Instance()
{
    static MenuPlugin instance;
    return instance;
}

MenuPlugin::MenuPlugin()
{
    m_item.SetOwner(this);
}

MenuPlugin::~MenuPlugin()
{
    // 插件卸载时释放覆盖窗和图标缓存
    TaskbarOverlay::Instance().Uninstall();
    IconManager::Instance().Cleanup();
}

void MenuPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    if (m_initialized || pApp == nullptr)
        return;

    m_pApp = pApp;

    // 初始化图标管理器
    HINSTANCE hInst = ::GetModuleHandleW(L"LeftClickMenu.dll");
    IconManager::Instance().Initialize(hInst);

    // 配置文件放到 TrafficMonitor 配置目录,升级插件后数据不丢失
    std::wstring configDir = pApp->GetPluginConfigDir();
    if (!configDir.empty() && configDir.back() != L'\\' && configDir.back() != L'/')
    {
        configDir += L'\\';
    }

    // 初始化日志
    Logger::Instance().SetLogDir(configDir);
    Logger::Instance().Info(L"===== LeftClickMenu 插件初始化 =====");

    m_config.SetConfigPath(configDir + L"LeftClickMenu.ini");
    m_config.Load();

    // 在任务栏显示区域上创建透明覆盖窗,接管左键点击
    // 不修改主程序窗口过程,右键/悬停等消息仍转发给主程序
    TaskbarOverlay::Instance().SetLeftClickCallback(&MenuPlugin::TaskbarLeftClickCallback);
    if (TaskbarOverlay::Instance().Install())
    {
        m_overlayInstalled = true;
        Logger::Instance().Info(L"任务栏显示区域覆盖窗创建成功");
    }
    else
    {
        Logger::Instance().Warn(L"任务栏显示区域覆盖窗创建失败,左键菜单仅支持点击本插件显示项触发");
    }

    m_initialized = true;
}

IPluginItem* MenuPlugin::GetItem(int index)
{
    if (index == 0) return &m_item;
    return nullptr;
}

void MenuPlugin::DataRequired()
{
    // 任务栏窗口可能在插件初始化之后才创建,未安装成功时每分钟重试一次
    if (!m_overlayInstalled)
    {
        DWORD now = ::GetTickCount();
        if (now - m_lastOverlayRetryTick >= 60000)
        {
            m_lastOverlayRetryTick = now;
            TaskbarOverlay::Instance().SetLeftClickCallback(&MenuPlugin::TaskbarLeftClickCallback);
            if (TaskbarOverlay::Instance().Install())
            {
                m_overlayInstalled = true;
                Logger::Instance().Info(L"任务栏显示区域覆盖窗已创建(延迟重试成功)");
            }
        }
    }
}

ITMPlugin::OptionReturn MenuPlugin::ShowOptionsDialog(void* hParent)
{
    // 切换 MFC 资源模块到当前 DLL,否则无法找到对话框资源
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    COptionsDialog dlg(m_config, CWnd::FromHandle(static_cast<HWND>(hParent)));
    if (dlg.DoModal() == IDOK)
    {
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

const wchar_t* MenuPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"快捷菜单插件";
    case TMI_DESCRIPTION: return L"左键点击显示项弹出自定义菜单,执行 PowerShell 脚本";
    case TMI_AUTHOR:      return L"TrafficMonitor User";
    case TMI_COPYRIGHT:   return L"Copyright (C) 2026";
    case TMI_VERSION:     return L"1.00";
    case TMI_URL:         return L"";
    }
    return L"";
}

const wchar_t* MenuPlugin::GetTooltipInfo()
{
    return L"";
}

void MenuPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_CONFIG_DIR && data != nullptr)
    {
        // 主程序传递配置目录,作为备选方案
        std::wstring configDir(data);
        if (!configDir.empty() && configDir.back() != L'\\' && configDir.back() != L'/')
        {
            configDir += L'\\';
        }
        // 如果 OnInitialize 中已设置路径,这里不覆盖
        if (!m_config.IsPathSet())
        {
            m_config.SetConfigPath(configDir + L"LeftClickMenu.ini");
            m_config.Load();
        }
    }
}

std::wstring MenuPlugin::GetDisplayText() const
{
    // 显示菜单项数量
    int count = m_config.GetEnabledCount();
    return std::to_wstring(count);
}

void MenuPlugin::ShowMenuFromItem(void* hWnd, int screenX, int screenY)
{
    HWND hOwner = static_cast<HWND>(hWnd);
    if (hOwner == nullptr) hOwner = ::GetForegroundWindow();
    if (hOwner == nullptr) hOwner = ::GetDesktopWindow();
    m_manager.ShowPopupMenu(m_config.GetItems(), hOwner, screenX, screenY);
}

void MenuPlugin::ShowMenuFromTaskbar(HWND hTargetWnd, int screenX, int screenY)
{
    if (m_config.GetItems().empty())
    {
        ::MessageBoxW(hTargetWnd, L"菜单项为空,请先在插件选项中添加菜单项。",
            L"快捷菜单", MB_OK | MB_ICONINFORMATION);
        return;
    }

    HWND hOwner = hTargetWnd;
    if (hOwner == nullptr) hOwner = ::GetForegroundWindow();
    if (hOwner == nullptr) hOwner = ::GetDesktopWindow();
    m_manager.ShowPopupMenu(m_config.GetItems(), hOwner, screenX, screenY);
}

// 静态回调函数,供 TaskbarOverlay 调用
void MenuPlugin::TaskbarLeftClickCallback(HWND hTargetWnd, int x, int y)
{
    Instance().ShowMenuFromTaskbar(hTargetWnd, x, y);
}
