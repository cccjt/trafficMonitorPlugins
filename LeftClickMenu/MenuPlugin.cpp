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

    m_initialized = true;
}

IPluginItem* MenuPlugin::GetItem(int index)
{
    if (index == 0) return &m_item;
    return nullptr;
}

void MenuPlugin::DataRequired()
{
    // 这里不需要频繁更新数据,显示文本在 GetDisplayText 中按需生成
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

void MenuPlugin::ShowMenuFromItem(void* hWnd, int x, int y)
{
    m_manager.ShowPopupMenu(m_config.GetItems(), static_cast<HWND>(hWnd), x, y);
}
