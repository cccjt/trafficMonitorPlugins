#include <Windows.h>
#include <WebView2.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "../resource.h"
#include "UsageReportPopup.h"

#pragma comment(lib, "ole32.lib")

namespace
{
const UINT kMsgNavigate = WM_APP + 2;
const UINT kMsgFailed = WM_APP + 3;

const int kBaseWidth = 1000;
const int kBaseHeight = 760;
const int kMinWidth = 620;
const int kMinHeight = 460;

std::wstring ReportHost() { return L"usagereport.local"; }

// WebView2 回调接口的最小 COM 实现
template <typename T>
class ComCallback : public T
{
public:
    ComCallback() : m_ref(1) {}

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(++m_ref);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG ref = static_cast<ULONG>(--m_ref);
        if (ref == 0) delete this;
        return ref;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (ppvObject == nullptr) return E_POINTER;
        if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, __uuidof(T)))
        {
            *ppvObject = static_cast<T*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

private:
    std::atomic<long> m_ref;
};

class EnvironmentHandler : public ComCallback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>
{
public:
    explicit EnvironmentHandler(UsageReportPopup* owner) : m_owner(owner) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
        ICoreWebView2Environment* createdEnvironment) override
    {
        m_owner->OnEnvironmentCreated(static_cast<long>(result), createdEnvironment);
        return S_OK;
    }

private:
    UsageReportPopup* m_owner;
};

class ControllerHandler : public ComCallback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>
{
public:
    explicit ControllerHandler(UsageReportPopup* owner) : m_owner(owner) {}

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
        ICoreWebView2Controller* createdController) override
    {
        m_owner->OnControllerCreated(static_cast<long>(result), createdController);
        return S_OK;
    }

private:
    UsageReportPopup* m_owner;
};

int SystemDpi()
{
    HDC dc = ::GetDC(nullptr);
    int dpi = (dc != nullptr) ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc != nullptr) ::ReleaseDC(nullptr, dc);
    return dpi > 0 ? dpi : 96;
}

HINSTANCE PluginModule()
{
    static HINSTANCE module = nullptr;
    if (module == nullptr)
    {
        ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&PluginModule), &module);
    }
    return module;
}
}

const wchar_t* UsageReportPopup::kWindowClass = L"UsageReportPopupWindow";

UsageReportPopup& UsageReportPopup::Instance()
{
    static UsageReportPopup instance;
    return instance;
}

UsageReportPopup::~UsageReportPopup()
{
    HWND hwnd = m_hWnd.load();
    if (hwnd != nullptr) ::PostMessageW(hwnd, WM_QUIT, 0, 0);
    if (m_thread.joinable()) m_thread.join();
    if (m_readyEvent != nullptr)
    {
        ::CloseHandle(m_readyEvent);
        m_readyEvent = nullptr;
    }
}

void UsageReportPopup::Initialize(std::function<std::wstring()> reportDirProvider,
    std::wstring htmlName, std::function<bool()> regenerate)
{
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_reportDirProvider = std::move(reportDirProvider);
        m_htmlName = std::move(htmlName);
        m_regenerate = std::move(regenerate);
    }

    if (m_thread.joinable()) return;

    m_readyEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_thread = std::thread(&UsageReportPopup::ThreadMain, this);
}

void UsageReportPopup::RequestShow()
{
    // 窗口由 UI 线程创建，首次调用时稍等片刻
    if (m_hWnd.load() == nullptr && m_readyEvent != nullptr)
    {
        ::WaitForSingleObject(m_readyEvent, 3000);
    }

    HWND hwnd = m_hWnd.load();
    if (hwnd == nullptr) return;
    ::PostMessageW(hwnd, kMessageShow, 0, 0);
}

void UsageReportPopup::ThreadMain()
{
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    CreateHiddenWindow();
    if (m_readyEvent != nullptr) ::SetEvent(m_readyEvent);

    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    DestroyWebView();
    ::CoUninitialize();
}

void UsageReportPopup::CreateHiddenWindow()
{
    HINSTANCE module = PluginModule();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &UsageReportPopup::WindowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
    wc.lpszClassName = kWindowClass;
    wc.hIcon = static_cast<HICON>(::LoadImageW(module, MAKEINTRESOURCEW(IDI_PLUGIN_ICON),
        IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(::LoadImageW(module, MAKEINTRESOURCEW(IDI_PLUGIN_ICON),
        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    ::RegisterClassExW(&wc);

    int scale = SystemDpi();
    int width = ::MulDiv(kBaseWidth, scale, 96);
    int height = ::MulDiv(kBaseHeight, scale, 96);

    POINT cursor = { 0 };
    ::GetCursorPos(&cursor);
    HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    ::GetMonitorInfoW(monitor, &mi);

    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;

    HWND hwnd = ::CreateWindowExW(0, kWindowClass, L"使用时长报告",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, width, height,
        nullptr, nullptr, wc.hInstance, this);

    if (hwnd == nullptr) return;

    m_hWnd = hwnd;

    m_hLoading = ::CreateWindowExW(0, L"STATIC", L"正在统计使用数据，请稍候…",
        WS_CHILD | SS_CENTER | SS_NOPREFIX, 0, 0, 100, 100, hwnd, nullptr, wc.hInstance, nullptr);
    if (m_hLoading != nullptr)
    {
        ::SendMessageW(m_hLoading, WM_SETFONT,
            reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        ::ShowWindow(m_hLoading, SW_HIDE);
    }

    LayoutChildren();
}

void UsageReportPopup::LayoutChildren()
{
    HWND hwnd = m_hWnd.load();
    if (hwnd == nullptr) return;

    RECT rc = { 0 };
    ::GetClientRect(hwnd, &rc);

    if (m_hLoading != nullptr)
    {
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        int lineHeight = ::MulDiv(24, SystemDpi(), 96);
        ::SetWindowPos(m_hLoading, nullptr, 0, (height - lineHeight) / 2,
            width, lineHeight, SWP_NOZORDER);
    }
}

LRESULT CALLBACK UsageReportPopup::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }

    UsageReportPopup* self = reinterpret_cast<UsageReportPopup*>(
        ::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self == nullptr) return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    return self->HandleMessage(hWnd, msg, wParam, lParam);
}

LRESULT UsageReportPopup::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        LayoutChildren();
        if (m_controller != nullptr)
        {
            RECT rc = { 0 };
            ::GetClientRect(hWnd, &rc);
            m_controller->put_Bounds(rc);
        }
        return 0;

    case WM_GETMINMAXINFO:
    {
        int scale = SystemDpi();
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = ::MulDiv(kMinWidth, scale, 96);
        mmi->ptMinTrackSize.y = ::MulDiv(kMinHeight, scale, 96);
        return 0;
    }

    case WM_CLOSE:
        // 关闭时释放 WebView2，降低常驻内存；窗口本身保留以便下次唤起
        DestroyWebView();
        ::ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    case kMessageShow:
        ShowAndRefresh();
        return 0;

    case kMsgNavigate:
        NavigateToReport();
        return 0;

    case kMsgFailed:
        ::MessageBoxW(hWnd,
            L"生成使用时长报告失败。\n\n"
            L"可能原因：事件日志不可读、PowerShell 不可用，或插件配置目录不可写。",
            L"使用时长报告", MB_ICONERROR | MB_OK);
        return 0;

    default:
        break;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void UsageReportPopup::ShowAndRefresh()
{
    HWND hwnd = m_hWnd.load();
    if (hwnd == nullptr) return;

    if (!::IsWindowVisible(hwnd))
    {
        ::ShowWindow(hwnd, SW_SHOWNORMAL);
    }
    ::SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::BringWindowToTop(hwnd);
    ::SetForegroundWindow(hwnd);

    SetLoadingVisible(true);

    if (!m_webViewReady && !m_environmentFailed)
    {
        CreateWebView();
    }

    StartRegeneration();
}

void UsageReportPopup::StartRegeneration()
{
    // 已有刷新在进行时不重复触发，窗口已在前面带到前台
    bool expected = false;
    if (!m_busy.compare_exchange_strong(expected, true)) return;

    std::function<bool()> task;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        task = m_regenerate;
    }

    std::thread([this, task]()
    {
        bool ok = (task != nullptr) && task();
        m_busy = false;

        HWND hwnd = m_hWnd.load();
        if (hwnd != nullptr)
        {
            ::PostMessageW(hwnd, ok ? kMsgNavigate : kMsgFailed, 0, 0);
        }
    }).detach();
}

void UsageReportPopup::CreateWebView()
{
    HWND hwnd = m_hWnd.load();
    if (hwnd == nullptr) return;

    std::wstring reportDir = GetReportDir();
    if (reportDir.empty()) return;

    // WebView2 的用户数据目录必须可用，首次打开时报告目录可能还没生成
    ::CreateDirectoryW(reportDir.c_str(), nullptr);

    std::wstring userDataDir = reportDir + L"\\WebView2";
    EnvironmentHandler* handler = new EnvironmentHandler(this);

    HRESULT hr = ::CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataDir.c_str(), nullptr, handler);
    handler->Release();

    if (FAILED(hr))
    {
        m_environmentFailed = true;
        ReportLoadFailure();
    }
}

void UsageReportPopup::OnEnvironmentCreated(long hr, void* environment)
{
    if (hr != S_OK || environment == nullptr)
    {
        m_environmentFailed = true;
        ReportLoadFailure();
        return;
    }

    ControllerHandler* handler = new ControllerHandler(this);
    static_cast<ICoreWebView2Environment*>(environment)
        ->CreateCoreWebView2Controller(m_hWnd.load(), handler);
    handler->Release();
}

void UsageReportPopup::OnControllerCreated(long hr, void* controller)
{
    if (hr != S_OK || controller == nullptr)
    {
        m_environmentFailed = true;
        ReportLoadFailure();
        return;
    }

    m_controller = static_cast<ICoreWebView2Controller*>(controller);
    m_controller->AddRef();

    RECT rc = { 0 };
    ::GetClientRect(m_hWnd.load(), &rc);
    m_controller->put_Bounds(rc);
    m_controller->put_IsVisible(TRUE);

    ICoreWebView2* webView = nullptr;
    if (SUCCEEDED(m_controller->get_CoreWebView2(&webView)) && webView != nullptr)
    {
        m_webView = webView;
        ConfigureWebView();
        m_webViewReady = true;

        if (m_pendingNavigate)
        {
            NavigateToReport();
        }
    }
}

void UsageReportPopup::ConfigureWebView()
{
    if (m_webView == nullptr) return;

    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(m_webView->get_Settings(&settings)) && settings != nullptr)
    {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsBuiltInErrorPageEnabled(FALSE);
        settings->Release();
    }

    ICoreWebView2_3* webView3 = nullptr;
    if (SUCCEEDED(m_webView->QueryInterface(IID_PPV_ARGS(&webView3))) && webView3 != nullptr)
    {
        // 把报告目录映射为虚拟主机，页面内的相对路径资源可以直接加载
        webView3->SetVirtualHostNameToFolderMapping(ReportHost().c_str(),
            GetReportDir().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
        webView3->Release();
    }
}

void UsageReportPopup::NavigateToReport()
{
    if (!m_webViewReady || m_webView == nullptr)
    {
        m_pendingNavigate = true;
        return;
    }

    m_pendingNavigate = false;

    std::wstring htmlName;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        htmlName = m_htmlName;
    }

    SetLoadingVisible(false);

    // 时间戳让页面每次都重新读取最新的 data.js
    std::wstring url = L"https://" + ReportHost() + L"/" + htmlName
        + L"?t=" + std::to_wstring(::GetTickCount64());
    m_webView->Navigate(url.c_str());
}

void UsageReportPopup::DestroyWebView()
{
    if (m_controller != nullptr)
    {
        m_controller->Close();
        m_controller->Release();
        m_controller = nullptr;
    }

    if (m_webView != nullptr)
    {
        m_webView->Release();
        m_webView = nullptr;
    }

    m_webViewReady = false;
    m_pendingNavigate = false;
}

void UsageReportPopup::SetLoadingVisible(bool visible)
{
    if (m_hLoading == nullptr) return;
    ::ShowWindow(m_hLoading, visible ? SW_SHOW : SW_HIDE);
    if (visible) LayoutChildren();
}

void UsageReportPopup::ReportLoadFailure()
{
    if (m_hWnd.load() == nullptr) return;
    SetLoadingVisible(false);
    ::MessageBoxW(m_hWnd.load(),
        L"无法启动内嵌浏览器组件（WebView2）。\n\n"
        L"请先安装 Microsoft Edge WebView2 Runtime 后重试。",
        L"使用时长报告", MB_ICONERROR | MB_OK);
}

std::wstring UsageReportPopup::GetReportDir()
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return (m_reportDirProvider != nullptr) ? m_reportDirProvider() : std::wstring();
}
