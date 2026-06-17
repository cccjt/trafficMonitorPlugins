#include "stdafx.h"
#include "HotkeyPlugin.h"
#include "OptionsDialog.h"
#include <Windows.h>
#include <sstream>

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

    // 初始化配置路径
    InitConfigPath();

    // 加载配置
    m_config.Load();

    // 初始化热键管理器
    if (m_manager.Initialize())
    {
        m_manager.SetCallback(&HotkeyPlugin::OnHotkeyExecuted, this);
        ApplyConfig();
    }

    m_initialized = true;
}

HotkeyPlugin::~HotkeyPlugin()
{
    m_manager.Destroy();
}

std::wstring HotkeyPlugin::GetPluginDir() const
{
    wchar_t path[MAX_PATH] = { 0 };
    HMODULE hModule = nullptr;
    if (::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&HotkeyPlugin::Instance),
        &hModule) && hModule != nullptr)
    {
        ::GetModuleFileNameW(hModule, path, MAX_PATH);
        std::wstring p(path);
        size_t pos = p.find_last_of(L"\\/");
        if (pos != std::wstring::npos) p = p.substr(0, pos);
        return p;
    }
    return L".";
}

void HotkeyPlugin::InitConfigPath()
{
    std::wstring dir = GetPluginDir();
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/')
    {
        dir += L'\\';
    }
    m_config.SetConfigPath(dir + L"HotkeyPlugin.ini");
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
    std::wostringstream oss;
    int total = static_cast<int>(m_config.GetItems().size());
    int enabled = m_config.GetEnabledCount();
    int registered = m_manager.GetRegisteredCount();

    oss << L"热键脚本插件\r\n";
    oss << L"已启用: " << enabled << L" / " << total << L"\r\n";
    oss << L"已注册: " << registered;

    if (!m_lastScriptName.empty())
    {
        oss << L"\r\n最后执行: " << m_lastScriptName;
    }

    m_tooltip = oss.str();
    return m_tooltip.c_str();
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

void HotkeyPlugin::OnHotkeyExecuted(const std::wstring& scriptPath, bool success, void* userData)
{
    HotkeyPlugin* self = static_cast<HotkeyPlugin*>(userData);
    if (self == nullptr) return;

    // 提取脚本文件名用于显示
    std::wstring name = scriptPath;
    size_t pos = name.find_last_of(L"\\/");
    if (pos != std::wstring::npos) name = name.substr(pos + 1);
    self->m_lastScriptName = name;
}
