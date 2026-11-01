#include "stdafx.h"
#include "MenuPlugin.h"
#include "OptionsDialog.h"
#include "IconManager.h"
#include "Logger.h"
#include <Windows.h>

namespace
{
// 生成不覆盖已有备份的备份路径
std::wstring MakeBackupPath(const std::wstring& path)
{
    std::wstring candidate = path + L".bak";
    if (::GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return candidate;
    }

    for (int i = 1; i < 100; ++i)
    {
        candidate = path + L".bak" + std::to_wstring(i);
        if (::GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            return candidate;
        }
    }
    return path + L".bak" + std::to_wstring(::GetTickCount());
}

// 迁移前备份旧配置文件
void BackupConfigFile(const std::wstring& path)
{
    if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return;

    std::wstring backupPath = MakeBackupPath(path);
    if (::CopyFileW(path.c_str(), backupPath.c_str(), FALSE))
    {
        Logger::Instance().Info(L"已备份旧配置: " + path + L" -> " + backupPath);
    }
    else
    {
        Logger::Instance().Warn(L"备份旧配置失败: " + path + L", 错误码="
            + std::to_wstring(::GetLastError()));
    }
}
}

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

    // 一次性导入旧版 HotkeyPlugin 配置,迁移前备份旧文件
    std::wstring legacyHotkeyIni = configDir + L"HotkeyPlugin.ini";
    if (m_config.ImportLegacyHotkeyConfig(legacyHotkeyIni))
    {
        BackupConfigFile(configDir + L"LeftClickMenu.ini");
        BackupConfigFile(legacyHotkeyIni);
        m_config.Save();
    }

    // 初始化全局热键:热键触发时执行对应菜单项的脚本
    m_hotkeyManager.SetActionCallback([this](const MenuConfigItem& item) {
        m_manager.ExecuteAction(item);
    });
    RegisterHotkeys();

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
    // 覆盖窗窗口失效时(例如任务栏窗口或所属线程被重建)需要重新创建
    if (TaskbarOverlay::Instance().IsInstalled())
    {
        m_overlayInstalled = true;
        return;
    }

    bool wasInstalled = m_overlayInstalled;
    m_overlayInstalled = false;

    DWORD now = ::GetTickCount();
    if (wasInstalled) m_lastOverlayRetryTick = 0;   // 覆盖窗刚刚失效,立即重试

    // 任务栏窗口可能在插件初始化之后才创建,未安装成功时每分钟重试一次
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

ITMPlugin::OptionReturn MenuPlugin::ShowOptionsDialog(void* hParent)
{
    // 切换 MFC 资源模块到当前 DLL,否则无法找到对话框资源
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    COptionsDialog dlg(m_config, CWnd::FromHandle(static_cast<HWND>(hParent)));
    if (dlg.DoModal() == IDOK)
    {
        // 配置已由对话框保存,这里按新配置重新注册热键
        RegisterHotkeys();
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

const wchar_t* MenuPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"快捷菜单与热键插件";
    case TMI_DESCRIPTION: return L"左键点击显示项弹出自定义菜单,并支持为每个菜单项设置全局热键执行 PowerShell 脚本";
    case TMI_AUTHOR:      return L"TrafficMonitor User";
    case TMI_COPYRIGHT:   return L"Copyright (C) 2026";
    case TMI_VERSION:     return L"1.13";
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
    // 显示左键菜单中的菜单项数量
    return std::to_wstring(m_config.GetVisibleMenuCount());
}

std::vector<MenuConfigItem> MenuPlugin::BuildVisibleMenuItems() const
{
    std::vector<MenuConfigItem> visible;
    for (const auto& item : m_config.GetItems())
    {
        if (item.isSeparator)
        {
            // 跳过开头和连续的分隔线
            if (visible.empty() || visible.back().isSeparator) continue;
            visible.push_back(item);
            continue;
        }

        if (!item.showInMenu) continue;
        visible.push_back(item);
    }

    // 去掉末尾的分隔线
    while (!visible.empty() && visible.back().isSeparator)
    {
        visible.pop_back();
    }
    return visible;
}

void MenuPlugin::RegisterHotkeys()
{
    if (!m_hotkeyManager.Initialize())
    {
        Logger::Instance().Warn(L"MenuPlugin::RegisterHotkeys: 热键管理器初始化失败");
        return;
    }

    // 注册失败(热键无效或被占用)只记录日志,不影响菜单功能
    m_hotkeyManager.RegisterAll(m_config.GetItems());
}

void MenuPlugin::ShowMenuFromItem(void* hWnd, int screenX, int screenY)
{
    HWND hOwner = static_cast<HWND>(hWnd);
    if (hOwner == nullptr) hOwner = ::GetForegroundWindow();
    if (hOwner == nullptr) hOwner = ::GetDesktopWindow();

    ShowPopupMenuInternal(hOwner, screenX, screenY, hWnd);
}

void MenuPlugin::ShowMenuFromTaskbar(HWND hTargetWnd, int screenX, int screenY)
{
    HWND hOwner = hTargetWnd;
    if (hOwner == nullptr) hOwner = ::GetForegroundWindow();
    if (hOwner == nullptr) hOwner = ::GetDesktopWindow();

    ShowPopupMenuInternal(hOwner, screenX, screenY, hTargetWnd);
}

void MenuPlugin::ShowPopupMenuInternal(HWND hOwner, int screenX, int screenY, void* hParent)
{
    std::vector<MenuConfigItem> visibleItems = BuildVisibleMenuItems();

    MenuViewOptions viewOptions;
    viewOptions.panelWidth = m_config.GetPanelWidth();
    viewOptions.tileMode = m_config.IsTileMode();
    viewOptions.tileColumns = m_config.GetTileColumns();

    bool editRequested = false;
    bool viewModeChanged = false;
    bool tileMode = viewOptions.tileMode;
    m_manager.ShowPopupMenu(visibleItems, hOwner, screenX, screenY, viewOptions,
        &editRequested, &viewModeChanged, &tileMode);

    // 面板标题栏切换了显示模式,立即写入配置
    if (viewModeChanged)
    {
        m_config.SetTileMode(tileMode);
        m_config.Save();
        Logger::Instance().Info(L"面板显示模式已切换为"
            + std::wstring(tileMode ? L"平铺" : L"列表"));
    }

    if (editRequested)
    {
        ShowOptionsDialog(hParent);
    }
}

// 静态回调函数,供 TaskbarOverlay 调用
void MenuPlugin::TaskbarLeftClickCallback(HWND hTargetWnd, int x, int y)
{
    Instance().ShowMenuFromTaskbar(hTargetWnd, x, y);
}
