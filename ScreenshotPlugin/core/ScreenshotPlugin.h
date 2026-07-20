#pragma once
#include "PluginInterface.h"
#include "../config/ScreenshotConfig.h"
#include "../config/HotkeyManager.h"
#include "../config/ActionType.h"
#include <string>
#include <memory>

class ScreenshotItem;

// 截图插件主类:实现 ITMPlugin 接口
class ScreenshotPlugin : public ITMPlugin
{
public:
    static ScreenshotPlugin& Instance();

    // === ITMPlugin 接口实现 ===
    IPluginItem* GetItem(int index) override;
    void DataRequired() override {}
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnInitialize(ITrafficMonitor* pApp) override;

    // === 内部接口 ===

    // 热键触发:截图捕获完成,显示选区覆盖层
    void OnScreenshotReady(ActionType action, HBITMAP hScreenBitmap);

    // 热键触发:剪贴板类操作(不经过截图流程)
    void OnClipboardAction(ActionType action);

    // 获取主程序接口
    ITrafficMonitor* GetApp() const { return m_pApp; }

    // 获取配置
    ScreenshotConfig& GetConfig() { return m_config; }

    // 获取热键管理器
    HotkeyManager& GetHotkeyManager() { return m_hotkeyMgr; }

    // 重新加载配置(从 INI)
    void ReloadConfig();

    // 保存配置
    void SaveConfig();

    // 应用配置(注册/重新注册热键)
    std::vector<ActionType> ApplyHotkeys();

private:
    ScreenshotPlugin();
    ~ScreenshotPlugin() override;

    // 计算配置文件路径(基于主程序提供的插件目录)
    void EnsureConfigPath();

private:
    ITrafficMonitor* m_pApp = nullptr;
    ScreenshotConfig m_config;
    HotkeyManager m_hotkeyMgr;

    std::unique_ptr<ScreenshotItem> m_item;

    bool m_initialized = false;
};

// 导出函数
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
