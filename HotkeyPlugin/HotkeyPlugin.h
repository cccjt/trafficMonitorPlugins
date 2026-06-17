#pragma once
#include "PluginInterface.h"
#include "HotkeyConfig.h"
#include "HotkeyManager.h"
#include "HotkeyItem.h"
#include <string>

// 主插件类:实现 ITMPlugin 接口
class HotkeyPlugin : public ITMPlugin
{
public:
    // 获取单例实例
    static HotkeyPlugin& Instance();

    // ITMPlugin 接口实现
    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;

    // 供 HotkeyItem 调用
    // 获取显示文本(已注册热键数/总数)
    std::wstring GetDisplayText() const;

    // 由显示项目触发的选项对话框
    void ShowOptionsFromItem(void* hWnd);

    // 获取热键管理器(供 OptionsDialog 使用)
    HotkeyManager& GetManager() { return m_manager; }
    HotkeyConfig& GetConfig() { return m_config; }

    // 配置变更后重新注册热键
    void ApplyConfig();

private:
    HotkeyPlugin();
    ~HotkeyPlugin();

    // 获取插件目录
    std::wstring GetPluginDir() const;

    // 初始化配置文件路径
    void InitConfigPath();

private:
    HotkeyConfig m_config;          // 配置
    HotkeyManager m_manager;        // 热键管理器
    HotkeyItem m_item;              // 显示项目

    bool m_initialized = false;     // 是否已初始化
};
