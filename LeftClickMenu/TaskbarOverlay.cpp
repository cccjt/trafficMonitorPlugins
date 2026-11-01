#include "stdafx.h"
#include "TaskbarOverlay.h"
#include "Logger.h"
#include <windowsx.h>
#include <wchar.h>
#include <wctype.h>
#include <string>

namespace
{
const UINT_PTR SYNC_TIMER_ID = 1;
const UINT SYNC_INTERVAL_MS = 1000;

struct FindWindowData
{
    DWORD processId;
    HWND result;
};

// 任务栏窗口下与显示区域无关的窗口(鼠标提示、输入法窗口)
bool IsIgnoredTaskbarChildClass(const wchar_t* className)
{
    return _wcsicmp(className, L"tooltips_class32") == 0
        || _wcsicmp(className, L"IME") == 0
        || _wcsicmp(className, L"MSCTFIME UI") == 0;
}

// 在任务栏窗口中查找本进程的可见子窗口
BOOL CALLBACK EnumTaskbarChildProc(HWND hWnd, LPARAM lParam)
{
    FindWindowData* data = reinterpret_cast<FindWindowData*>(lParam);

    DWORD processId = 0;
    ::GetWindowThreadProcessId(hWnd, &processId);
    if (processId != data->processId) return TRUE;
    if (!::IsWindowVisible(hWnd)) return TRUE;

    wchar_t className[256] = { 0 };
    if (::GetClassNameW(hWnd, className, 256) <= 0) return TRUE;
    if (IsIgnoredTaskbarChildClass(className)) return TRUE;

    RECT rect = { 0 };
    if (!::GetWindowRect(hWnd, &rect)) return TRUE;
    if (rect.right - rect.left < 20 || rect.bottom - rect.top < 10) return TRUE;

    data->result = hWnd;
    return FALSE;
}

// 遍历所有任务栏窗口(主任务栏 + 副显示器任务栏)
BOOL CALLBACK EnumTaskbarProc(HWND hWnd, LPARAM lParam)
{
    wchar_t className[256] = { 0 };
    if (::GetClassNameW(hWnd, className, 256) <= 0) return TRUE;
    if (wcscmp(className, L"Shell_TrayWnd") != 0 && wcscmp(className, L"Shell_SecondaryTrayWnd") != 0)
        return TRUE;

    FindWindowData* data = reinterpret_cast<FindWindowData*>(lParam);
    if (data->result != nullptr) return FALSE;

    ::EnumChildWindows(hWnd, EnumTaskbarChildProc, lParam);
    return data->result == nullptr ? TRUE : FALSE;
}

// 检查当前线程是否有菜单窗口(#32768)
BOOL CALLBACK EnumThreadMenuProc(HWND hWnd, LPARAM lParam)
{
    wchar_t className[256] = { 0 };
    if (::GetClassNameW(hWnd, className, 256) > 0 && wcscmp(className, L"#32768") == 0)
    {
        *reinterpret_cast<bool*>(lParam) = true;
        return FALSE;
    }
    return TRUE;
}

// 兜底:旧版 TrafficMonitor 的任务栏窗口可能是独立的顶层窗口
BOOL CALLBACK EnumOwnTaskbarProc(HWND hWnd, LPARAM lParam)
{
    FindWindowData* data = reinterpret_cast<FindWindowData*>(lParam);

    DWORD processId = 0;
    ::GetWindowThreadProcessId(hWnd, &processId);
    if (processId != data->processId) return TRUE;
    if (!::IsWindowVisible(hWnd)) return TRUE;

    wchar_t className[256] = { 0 };
    if (::GetClassNameW(hWnd, className, 256) <= 0) return TRUE;

    std::wstring cls(className);
    for (auto& ch : cls) ch = static_cast<wchar_t>(towlower(ch));
    if (cls.find(L"taskbar") == std::wstring::npos) return TRUE;

    RECT rect = { 0 };
    if (!::GetWindowRect(hWnd, &rect)) return TRUE;
    if (rect.right - rect.left < 20 || rect.bottom - rect.top < 10) return TRUE;

    data->result = hWnd;
    return FALSE;
}
}

TaskbarOverlay& TaskbarOverlay::Instance()
{
    // 使用不会析构的实例,保证插件析构时仍可安全地卸载覆盖窗
    static TaskbarOverlay* instance = new TaskbarOverlay();
    return *instance;
}

HHOOK TaskbarOverlay::s_hook = nullptr;
HWND TaskbarOverlay::s_createdWnd = nullptr;
HWND TaskbarOverlay::s_targetWnd = nullptr;
TaskbarOverlay* TaskbarOverlay::s_owner = nullptr;
bool TaskbarOverlay::s_inCreation = false;

TaskbarOverlay::TaskbarOverlay() {}
TaskbarOverlay::~TaskbarOverlay() {}

HWND TaskbarOverlay::FindTaskbarWindow() const
{
    FindWindowData data = { ::GetCurrentProcessId(), nullptr };
    ::EnumWindows(EnumTaskbarProc, reinterpret_cast<LPARAM>(&data));
    if (data.result != nullptr) return data.result;

    FindWindowData topLevel = { ::GetCurrentProcessId(), nullptr };
    ::EnumWindows(EnumOwnTaskbarProc, reinterpret_cast<LPARAM>(&topLevel));
    if (topLevel.result != nullptr) return topLevel.result;

    HWND hWnd = ::FindWindowW(L"TrafficMonitor::TaskBarDlg", nullptr);
    if (hWnd != nullptr)
    {
        DWORD processId = 0;
        ::GetWindowThreadProcessId(hWnd, &processId);
        if (processId == ::GetCurrentProcessId()) return hWnd;
    }

    return nullptr;
}

HWND TaskbarOverlay::CreateOverlayWindow(HWND hTargetWnd, TaskbarOverlay* owner)
{
    if (hTargetWnd == nullptr || owner == nullptr) return nullptr;

    RECT targetRect = { 0 };
    ::GetWindowRect(hTargetWnd, &targetRect);

    HINSTANCE hInstance = ::GetModuleHandleW(L"LeftClickMenu.dll");
    if (hInstance == nullptr) hInstance = ::GetModuleHandleW(nullptr);

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &TaskbarOverlay::OverlayProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"LeftClickMenuOverlayWnd";
        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            Logger::Instance().Error(L"TaskbarOverlay::CreateOverlayWindow: 注册覆盖窗类失败, 错误码="
                + std::to_wstring(::GetLastError()));
            return nullptr;
        }
        classRegistered = true;
    }

    HWND hOverlay = ::CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        L"LeftClickMenuOverlayWnd", L"", WS_POPUP,
        targetRect.left, targetRect.top,
        targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (hOverlay == nullptr)
    {
        Logger::Instance().Error(L"TaskbarOverlay::CreateOverlayWindow: 创建覆盖窗失败, 错误码="
            + std::to_wstring(::GetLastError()));
        return nullptr;
    }

    ::SetWindowLongPtrW(hOverlay, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));

    // 几乎完全透明,但 alpha 必须大于 0:
    // alpha=0 的层窗口连鼠标消息也会穿透,窗口将永远收不到点击
    ::SetLayeredWindowAttributes(hOverlay, 0, 1, LWA_ALPHA);
    ::SetWindowPos(hOverlay, HWND_TOPMOST, targetRect.left, targetRect.top,
        targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ::SetTimer(hOverlay, SYNC_TIMER_ID, SYNC_INTERVAL_MS, nullptr);

    Logger::Instance().Info(L"TaskbarOverlay::Install: 覆盖窗创建成功, 区域=("
        + std::to_wstring(targetRect.left) + L"," + std::to_wstring(targetRect.top) + L")-("
        + std::to_wstring(targetRect.right) + L"," + std::to_wstring(targetRect.bottom) + L")");
    return hOverlay;
}

LRESULT CALLBACK TaskbarOverlay::CreateWindowHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && !s_inCreation && s_createdWnd == nullptr
        && s_targetWnd != nullptr && s_owner != nullptr)
    {
        s_inCreation = true;
        s_createdWnd = CreateOverlayWindow(s_targetWnd, s_owner);
        s_inCreation = false;
    }
    return ::CallNextHookEx(s_hook, nCode, wParam, lParam);
}

bool TaskbarOverlay::Install()
{
    if (m_hOverlayWnd != nullptr)
    {
        if (::IsWindow(m_hOverlayWnd)) return true;
        m_hOverlayWnd = nullptr;    // 窗口句柄已失效,重新创建
    }

    m_hTargetWnd = FindTaskbarWindow();
    if (m_hTargetWnd == nullptr)
    {
        Logger::Instance().Warn(L"TaskbarOverlay::Install: 未找到 TrafficMonitor 任务栏窗口");
        return false;
    }

    // 覆盖窗必须创建在会处理其窗口消息的线程上
    // 插件初始化可能在后台线程执行,直接创建会导致窗口收不到任何鼠标消息
    DWORD targetThreadId = ::GetWindowThreadProcessId(m_hTargetWnd, nullptr);
    if (targetThreadId == ::GetCurrentThreadId())
    {
        m_hOverlayWnd = CreateOverlayWindow(m_hTargetWnd, this);
    }
    else
    {
        HINSTANCE hInstance = ::GetModuleHandleW(L"LeftClickMenu.dll");
        if (hInstance == nullptr) hInstance = ::GetModuleHandleW(nullptr);

        s_createdWnd = nullptr;
        s_targetWnd = m_hTargetWnd;
        s_owner = this;
        s_inCreation = false;

        // 用一个临时钩子在任务栏窗口所属线程上执行创建
        s_hook = ::SetWindowsHookExW(WH_CALLWNDPROC,
            &TaskbarOverlay::CreateWindowHookProc, hInstance, targetThreadId);
        if (s_hook == nullptr)
        {
            Logger::Instance().Error(L"TaskbarOverlay::Install: 安装线程钩子失败, 错误码="
                + std::to_wstring(::GetLastError()));
        }
        else
        {
            DWORD_PTR sendResult = 0;
            ::SendMessageTimeoutW(m_hTargetWnd, WM_NULL, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &sendResult);
            ::UnhookWindowsHookEx(s_hook);
            s_hook = nullptr;

            m_hOverlayWnd = s_createdWnd;
            s_createdWnd = nullptr;
            s_targetWnd = nullptr;
            s_owner = nullptr;
        }
    }

    if (m_hOverlayWnd == nullptr)
    {
        Logger::Instance().Error(L"TaskbarOverlay::Install: 创建覆盖窗失败");
        m_hTargetWnd = nullptr;
        return false;
    }
    return true;
}

void TaskbarOverlay::Uninstall()
{
    if (m_hOverlayWnd != nullptr)
    {
        if (::IsWindow(m_hOverlayWnd))
        {
            // 窗口由主程序 UI 线程处理消息,交给它自己销毁,避免跨线程 DestroyWindow
            ::PostMessageW(m_hOverlayWnd, WM_CLOSE, 0, 0);
        }
        m_hOverlayWnd = nullptr;
        Logger::Instance().Info(L"TaskbarOverlay::Uninstall: 覆盖窗已销毁");
    }
    m_hTargetWnd = nullptr;
    m_trackingLeave = false;
}

void TaskbarOverlay::SyncPosition()
{
    if (m_hOverlayWnd == nullptr) return;
    if (!::IsWindow(m_hOverlayWnd))
    {
        m_hOverlayWnd = nullptr;
        return;
    }

    // 菜单打开期间不移动/置顶覆盖窗,避免挡住菜单
    if (IsMenuOpen()) return;

    // 任务栏窗口被重建时重新查找
    if (m_hTargetWnd == nullptr || !::IsWindow(m_hTargetWnd))
    {
        m_hTargetWnd = FindTaskbarWindow();
        if (m_hTargetWnd == nullptr)
        {
            if (::IsWindowVisible(m_hOverlayWnd)) ::ShowWindow(m_hOverlayWnd, SW_HIDE);
            return;
        }
        Logger::Instance().Info(L"TaskbarOverlay::SyncPosition: 已重新找到任务栏窗口");
    }

    RECT targetRect = { 0 };
    if (!::GetWindowRect(m_hTargetWnd, &targetRect)) return;

    int width = targetRect.right - targetRect.left;
    int height = targetRect.bottom - targetRect.top;
    bool targetVisible = ::IsWindowVisible(m_hTargetWnd) != FALSE && width > 0 && height > 0;

    if (!targetVisible)
    {
        if (::IsWindowVisible(m_hOverlayWnd)) ::ShowWindow(m_hOverlayWnd, SW_HIDE);
        return;
    }

    RECT overlayRect = { 0 };
    ::GetWindowRect(m_hOverlayWnd, &overlayRect);

    bool rectChanged = overlayRect.left != targetRect.left
        || overlayRect.top != targetRect.top
        || overlayRect.right - overlayRect.left != width
        || overlayRect.bottom - overlayRect.top != height;

    // 任务栏会被系统重新置顶,需要定期把覆盖窗拉回任务栏之上
    bool needRaise = rectChanged || !::IsWindowVisible(m_hOverlayWnd) || IsOverlayBelowTaskbar();

    if (needRaise)
    {
        ::SetWindowPos(m_hOverlayWnd, HWND_TOPMOST, targetRect.left, targetRect.top, width, height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

bool TaskbarOverlay::IsMenuOpen() const
{
    bool found = false;
    ::EnumThreadWindows(::GetCurrentThreadId(), EnumThreadMenuProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

bool TaskbarOverlay::IsOverlayBelowTaskbar() const
{
    if (m_hOverlayWnd == nullptr) return false;

    // 从 z 序顶部向下遍历,先遇到覆盖窗说明它仍在任务栏之上
    HWND hWnd = ::GetTopWindow(nullptr);
    while (hWnd != nullptr)
    {
        if (hWnd == m_hOverlayWnd) return false;

        wchar_t className[256] = { 0 };
        if (::GetClassNameW(hWnd, className, 256) > 0
            && (wcscmp(className, L"Shell_TrayWnd") == 0
                || wcscmp(className, L"Shell_SecondaryTrayWnd") == 0))
        {
            return true;
        }
        hWnd = ::GetWindow(hWnd, GW_HWNDNEXT);
    }
    return false;
}

void TaskbarOverlay::ForwardMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (m_hTargetWnd == nullptr || !::IsWindow(m_hTargetWnd)) return;

    if (uMsg == WM_CONTEXTMENU)
    {
        // WM_CONTEXTMENU 的 lParam 本身就是屏幕坐标
        ::PostMessageW(m_hTargetWnd, uMsg, wParam, lParam);
        return;
    }

    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    if (uMsg == WM_MOUSEWHEEL)
    {
        // 滚轮消息的坐标已经是屏幕坐标
        ::ScreenToClient(m_hTargetWnd, &pt);
    }
    else
    {
        ::ClientToScreen(m_hOverlayWnd, &pt);
        ::ScreenToClient(m_hTargetWnd, &pt);
    }
    ::PostMessageW(m_hTargetWnd, uMsg, wParam, MAKELPARAM(pt.x, pt.y));
}

LRESULT CALLBACK TaskbarOverlay::OverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TaskbarOverlay* self = reinterpret_cast<TaskbarOverlay*>(
        ::GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (uMsg)
    {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_NCHITTEST:
        return HTCLIENT;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps = { 0 };
        ::BeginPaint(hWnd, &ps);
        ::EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (self == nullptr) return 0;

        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ::ClientToScreen(hWnd, &pt);

        // 弹出菜单期间隐藏覆盖窗,避免它挡住菜单;同时暂停位置同步
        ::KillTimer(hWnd, SYNC_TIMER_ID);
        ::ShowWindow(hWnd, SW_HIDE);
        if (self->m_callback != nullptr && self->m_hTargetWnd != nullptr)
        {
            self->m_callback(self->m_hTargetWnd, pt.x, pt.y);
        }
        self->SyncPosition();
        ::SetTimer(hWnd, SYNC_TIMER_ID, SYNC_INTERVAL_MS, nullptr);
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        // 左键由插件自己处理,不再传给主程序
        return 0;

    case WM_MOUSEMOVE:
        if (self != nullptr)
        {
            if (!self->m_trackingLeave)
            {
                TRACKMOUSEEVENT tme = { 0 };
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                if (::TrackMouseEvent(&tme)) self->m_trackingLeave = true;
            }
            self->ForwardMouseMessage(uMsg, wParam, lParam);
        }
        return 0;

    case WM_MOUSELEAVE:
        if (self != nullptr)
        {
            self->m_trackingLeave = false;
            if (self->m_hTargetWnd != nullptr && ::IsWindow(self->m_hTargetWnd))
            {
                ::PostMessageW(self->m_hTargetWnd, WM_MOUSELEAVE, 0, 0);
            }
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (self != nullptr && self->m_hTargetWnd != nullptr)
        {
            // 让任务栏窗口处于前台,否则它弹出的右键菜单会立刻消失
            ::SetForegroundWindow(::GetAncestor(self->m_hTargetWnd, GA_ROOT));
            self->ForwardMouseMessage(uMsg, wParam, lParam);
        }
        return 0;

    case WM_RBUTTONUP:
        if (self != nullptr)
        {
            self->m_lastRButtonUpTick = ::GetTickCount();
            self->ForwardMouseMessage(uMsg, wParam, lParam);
        }
        return 0;

    case WM_RBUTTONDBLCLK:
        if (self != nullptr) self->ForwardMouseMessage(uMsg, wParam, lParam);
        return 0;

    case WM_CONTEXTMENU:
        // 主程序若在右键抬起时已经弹过菜单,这里就不再重复转发
        if (self != nullptr && ::GetTickCount() - self->m_lastRButtonUpTick > 1000)
        {
            if (self->m_hTargetWnd != nullptr)
            {
                ::SetForegroundWindow(::GetAncestor(self->m_hTargetWnd, GA_ROOT));
                self->ForwardMouseMessage(uMsg, wParam, lParam);
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (self != nullptr) self->ForwardMouseMessage(uMsg, wParam, lParam);
        return 0;

    case WM_TIMER:
        if (wParam == SYNC_TIMER_ID && self != nullptr) self->SyncPosition();
        return 0;

    case WM_CLOSE:
        // 覆盖窗由 Uninstall 通过 WM_CLOSE 请求销毁,必须在自己所属线程上销毁
        ::DestroyWindow(hWnd);
        return 0;

    case WM_NCDESTROY:
        ::KillTimer(hWnd, SYNC_TIMER_ID);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }

    return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
