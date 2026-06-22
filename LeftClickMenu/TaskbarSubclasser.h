#pragma once
#include <Windows.h>
#include <string>

// 任务栏窗口子类化
// 通过 SetWindowLongPtr 替换 TrafficMonitor 任务栏窗口的窗口过程
// 只拦截 WM_LBUTTONDOWN,其他消息原样传递给原始窗口过程
// 相比全局鼠标钩子,只影响一个窗口,性能开销极小
class TaskbarSubclasser
{
public:
    static TaskbarSubclasser& Instance();

    // 查找并子类化 TrafficMonitor 任务栏窗口
    // 返回是否成功
    bool Install();

    // 卸载子类化,恢复原始窗口过程
    void Uninstall();

    bool IsInstalled() const { return m_hTargetWnd != nullptr; }

    // 设置左键点击回调(屏幕坐标)
    typedef void (*LeftClickCallback)(int x, int y);
    void SetLeftClickCallback(LeftClickCallback callback) { m_callback = callback; }

private:
    TaskbarSubclasser();
    ~TaskbarSubclasser();

    TaskbarSubclasser(const TaskbarSubclasser&) = delete;
    TaskbarSubclasser& operator=(const TaskbarSubclasser&) = delete;

    // 新的窗口过程
    static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 查找 TrafficMonitor 任务栏窗口
    HWND FindTaskbarWindow() const;

    HWND m_hTargetWnd = nullptr;        // 目标窗口句柄
    WNDPROC m_originalProc = nullptr;   // 原始窗口过程
    LeftClickCallback m_callback = nullptr;
    DWORD m_currentProcessId = 0;       // 当前进程ID(TrafficMonitor)
};
