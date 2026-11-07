#pragma once
#include <Windows.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

struct ICoreWebView2;
struct ICoreWebView2Controller;

// 使用 WebView2 承载统计报表的独立弹窗（不经过浏览器）
// 窗口在插件初始化时创建为隐藏状态，左键菜单脚本通过窗口消息唤起。
class UsageReportPopup
{
public:
    // 左键菜单脚本与插件内部共用的唤起消息
    static const UINT kMessageShow = WM_APP + 1;
    static const wchar_t* kWindowClass;

    static UsageReportPopup& Instance();

    // 启动 UI 线程并创建隐藏窗口
    // reportDirProvider 每次创建 WebView2 时调用，保证配置目录变化后仍然正确
    // regenerate 在后台线程执行，返回是否成功
    // initScriptProvider 在页面创建前注入脚本（用于向页面传递心跳配置）
    // messageHandler 接收页面通过 postMessage 发来的文本消息
    void Initialize(std::function<std::wstring()> reportDirProvider,
        std::wstring htmlName, std::function<bool()> regenerate,
        std::function<std::wstring()> initScriptProvider,
        std::function<void(const std::wstring&)> messageHandler);

    // 线程安全：显示窗口并刷新数据
    void RequestShow();

private:
    UsageReportPopup() = default;
    ~UsageReportPopup();
    UsageReportPopup(const UsageReportPopup&) = delete;
    UsageReportPopup& operator=(const UsageReportPopup&) = delete;

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void ThreadMain();
    void CreateHiddenWindow();
    void LayoutChildren();
    void ShowAndRefresh();
    void StartRegeneration();
    void CreateWebView();
    void DestroyWebView();
    void ConfigureWebView();
    void NavigateToReport();
    void SetLoadingVisible(bool visible);
    void ReportLoadFailure();
    std::wstring GetReportDir();

public:
    // 仅供 WebView2 回调调用
    void OnEnvironmentCreated(long hr, void* environment);
    void OnControllerCreated(long hr, void* controller);
    void OnWebMessage(void* args);

private:

    std::thread m_thread;
    std::atomic<HWND> m_hWnd{ nullptr };
    HANDLE m_readyEvent = nullptr;

    std::mutex m_configMutex;
    std::function<std::wstring()> m_reportDirProvider;
    std::wstring m_htmlName;
    std::function<bool()> m_regenerate;
    std::function<std::wstring()> m_initScriptProvider;
    std::function<void(const std::wstring&)> m_messageHandler;

    HWND m_hLoading = nullptr;
    ICoreWebView2Controller* m_controller = nullptr;
    ICoreWebView2* m_webView = nullptr;
    bool m_webViewReady = false;
    bool m_pendingNavigate = false;
    bool m_environmentFailed = false;
    std::atomic<bool> m_busy{ false };
    long long m_messageToken = 0;
};
