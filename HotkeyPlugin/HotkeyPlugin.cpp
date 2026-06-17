#include "stdafx.h"
#include "HotkeyPlugin.h"
#include "OptionsDialog.h"
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
    return &HotkeyPlugin::Instance();
}

HotkeyPlugin& HotkeyPlugin::Instance()
{
    static HotkeyPlugin instance;
    return instance;
}

HotkeyPlugin::HotkeyPlugin()
{
    m_item.SetOwner(this);
}

HotkeyPlugin::~HotkeyPlugin()
{
    m_manager.Destroy();
}

void HotkeyPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    if (m_initialized || pApp == nullptr)
        return;

    m_pApp = pApp;

    // 配置文件放到 TrafficMonitor 配置目录,升级插件后数据不丢失
    std::wstring configDir = pApp->GetPluginConfigDir();
    if (!configDir.empty() && configDir.back() != L'\\' && configDir.back() != L'/')
    {
        configDir += L'\\';
    }
    m_config.SetConfigPath(configDir + L"HotkeyPlugin.ini");
    m_config.Load();

    // 初始化热键管理器
    if (m_manager.Initialize())
    {
        ApplyConfig();
    }

    m_initialized = true;
}

IPluginItem* HotkeyPlugin::GetItem(int index)
{
    if (index == 0) return &m_item;
    return nullptr;
}

void HotkeyPlugin::DataRequired()
{
    // 这里不需要频繁更新数据,显示文本在 GetDisplayText 中按需生成
}

ITMPlugin::OptionReturn HotkeyPlugin::ShowOptionsDialog(void* hParent)
{
    // 切换 MFC 资源模块到当前 DLL,否则无法找到对话框资源
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    // 保存原始配置以便比较
    std::vector<HotkeyConfigItem> original = m_config.GetItems();

    CWnd* parent = hParent != nullptr ? CWnd::FromHandle(static_cast<HWND>(hParent)) : nullptr;
    COptionsDialog dlg(m_config, parent);
    INT_PTR result = dlg.DoModal();

    if (result == IDOK)
    {
        // 保存配置并重新注册热键
        m_config.Save();
        ApplyConfig();
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

const wchar_t* HotkeyPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"热键脚本插件";
    case TMI_DESCRIPTION: return L"通过键盘热键执行指定的 PowerShell 脚本";
    case TMI_AUTHOR:      return L"TrafficMonitor User";
    case TMI_COPYRIGHT:   return L"Copyright (C) 2026";
    case TMI_VERSION:     return L"1.00";
    case TMI_URL:         return L"";
    }
    return L"";
}

const wchar_t* HotkeyPlugin::GetTooltipInfo()
{
    // 不显示鼠标悬停提示
    return L"";
}

std::wstring HotkeyPlugin::GetDisplayText() const
{
    // 显示已注册的热键数量
    int registered = m_manager.GetRegisteredCount();
    return std::to_wstring(registered);
}

void HotkeyPlugin::ShowOptionsFromItem(void* hWnd)
{
    ShowOptionsDialog(hWnd);
}

void HotkeyPlugin::ApplyConfig()
{
    m_manager.RegisterAll(m_config.GetItems());
}

