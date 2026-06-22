#include "stdafx.h"
#include "TaskbarSubclasser.h"
#include "Logger.h"
#include <windowsx.h>
#include <string>

TaskbarSubclasser& TaskbarSubclasser::Instance()
{
    static TaskbarSubclasser instance;
    return instance;
}

TaskbarSubclasser::TaskbarSubclasser()
{
    m_currentProcessId = ::GetCurrentProcessId();
}

TaskbarSubclasser::~TaskbarSubclasser()
{
    Uninstall();
}

// 回调函数类型,用于 EnumWindows
struct EnumData
{
    DWORD processId;
    HWND result;
};

static BOOL CALLBACK EnumWndProc(HWND hWnd, LPARAM lParam)
{
    EnumData* data = reinterpret_cast<EnumData*>(lParam);

    DWORD pid = 0;
    ::GetWindowThreadProcessId(hWnd, &pid);

    if (pid != data->processId) return TRUE;

    // 检查窗口类名
    wchar_t className[256] = { 0 };
    int len = ::GetClassNameW(hWnd, className, 256);
    if (len <= 0) return TRUE;

    std::wstring cls(className);
    for (auto& ch : cls) ch = static_cast<wchar_t>(towlower(ch));

    // TrafficMonitor 任务栏窗口类名包含 "taskbar"
    if (cls.find(L"taskbar") != std::wstring::npos)
    {
        // 确保窗口是可见的
        if (::IsWindowVisible(hWnd))
        {
            data->result = hWnd;
            return FALSE;  // 找到了,停止枚举
        }
    }

    return TRUE;
}

HWND TaskbarSubclasser::FindTaskbarWindow() const
{
    EnumData data = { m_currentProcessId, nullptr };
    ::EnumWindows(EnumWndProc, reinterpret_cast<LPARAM>(&data));
    return data.result;
}

bool TaskbarSubclasser::Install()
{
    if (m_hTargetWnd != nullptr)
    {
        Logger::Instance().Info(L"TaskbarSubclasser::Install: 已安装,跳过");
        return true;
    }

    // 查找 TrafficMonitor 任务栏窗口
    m_hTargetWnd = FindTaskbarWindow();
    if (m_hTargetWnd == nullptr)
    {
        Logger::Instance().Warn(L"TaskbarSubclasser::Install: 未找到 TrafficMonitor 任务栏窗口");
        return false;
    }

    // 记录窗口类名(用于日志)
    wchar_t className[256] = { 0 };
    ::GetClassNameW(m_hTargetWnd, className, 256);
    Logger::Instance().Info(L"TaskbarSubclasser::Install: 找到任务栏窗口, 类名=[" + std::wstring(className) + L"]");

    // 子类化:替换窗口过程
    LONG_PTR original = ::SetWindowLongPtrW(m_hTargetWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubclassProc));
    if (original == 0)
    {
        DWORD err = ::GetLastError();
        Logger::Instance().Error(L"TaskbarSubclasser::Install: SetWindowLongPtrW 失败, 错误码=" + std::to_wstring(err));
        m_hTargetWnd = nullptr;
        return false;
    }

    m_originalProc = reinterpret_cast<WNDPROC>(original);
    Logger::Instance().Info(L"TaskbarSubclasser::Install: 子类化成功");
    return true;
}

void TaskbarSubclasser::Uninstall()
{
    if (m_hTargetWnd != nullptr && m_originalProc != nullptr)
    {
        // 恢复原始窗口过程
        ::SetWindowLongPtrW(m_hTargetWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalProc));
        Logger::Instance().Info(L"TaskbarSubclasser::Uninstall: 已恢复原始窗口过程");
    }
    m_hTargetWnd = nullptr;
    m_originalProc = nullptr;
    m_callback = nullptr;
}

LRESULT CALLBACK TaskbarSubclasser::SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TaskbarSubclasser& mgr = Instance();

    // 只拦截左键按下
    if (uMsg == WM_LBUTTONDOWN)
    {
        // 获取屏幕坐标
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ::ClientToScreen(hWnd, &pt);

        Logger::Instance().Info(L"TaskbarSubclasser: 拦截左键点击, 屏幕坐标=(" + std::to_wstring(pt.x) + L"," + std::to_wstring(pt.y) + L")");

        // 调用回调弹出菜单
        if (mgr.m_callback != nullptr)
        {
            mgr.m_callback(pt.x, pt.y);
        }

        // 返回 0,表示已处理,阻止 TrafficMonitor 默认行为
        return 0;
    }

    // 其他消息传递给原始窗口过程
    return ::CallWindowProcW(mgr.m_originalProc, hWnd, uMsg, wParam, lParam);
}
