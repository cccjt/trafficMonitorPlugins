#pragma once
#include "PluginInterface.h"
#include <atomic>
#include <mutex>
#include <string>

// 使用时长报告插件
// 通过插件命令读取 Windows 电源/锁屏事件，实时生成报告并在内嵌 WebView2 窗口中显示
class UsageReportPlugin : public ITMPlugin
{
public:
    static UsageReportPlugin& Instance();

    // 本插件不提供常驻显示项目
    IPluginItem* GetItem(int index) override { return nullptr; }
    void DataRequired() override {}

    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnInitialize(ITrafficMonitor* pApp) override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    // 兼容只提供“选项对话框”入口的主程序版本
    OptionReturn ShowOptionsDialog(void* hParent) override;

    // 嵌入在 DLL 中的图标
    void* GetPluginIcon() override;
    void* GetCommandIcon(int command_index) override;

    int GetCommandCount() override { return 1; }
    const wchar_t* GetCommandName(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;

    // 显示器电源状态变化回调（0=关，1=开，2=变暗）
    void OnDisplayStateChanged(unsigned long state);

private:
    UsageReportPlugin() = default;
    ~UsageReportPlugin();
    UsageReportPlugin(const UsageReportPlugin&) = delete;
    UsageReportPlugin& operator=(const UsageReportPlugin&) = delete;

    // 把嵌入的报告文件释放到插件配置目录
    void EnsureReportFiles();
    // 调用生成脚本，返回是否成功
    bool GenerateReport();

    void RegisterDisplayNotification();
    void AppendDisplayEvent(const wchar_t* state);

    std::wstring ResolveConfigDir() const;
    std::wstring ReportDir() const;

    ITrafficMonitor* m_pApp = nullptr;
    std::wstring m_configDir;

    void* m_hDisplayNotify = nullptr;
    std::mutex m_logMutex;
    int m_lastDisplayState = -1;
};
