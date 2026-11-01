#pragma once
#include <Windows.h>
#include <string>

// 任务栏显示区域透明覆盖窗
// 在 TrafficMonitor 任务栏窗口(Shell_TrayWnd 的子窗口)正上方创建一个全透明的分层窗口,
// 覆盖住现有的显示区域,只接管鼠标左键:
//   - 左键按下:通过回调弹出菜单
//   - 右键、移动、滚轮等消息原样转发给任务栏窗口,保留主程序原有的右键菜单和鼠标提示
// 不修改任务栏窗口过程,卸载时直接销毁覆盖窗即可
class TaskbarOverlay
{
public:
    static TaskbarOverlay& Instance();

    // 查找任务栏窗口并创建覆盖窗,失败返回 false
    bool Install();

    // 销毁覆盖窗
    void Uninstall();

    bool IsInstalled() const { return m_hOverlayWnd != nullptr; }

    // 左键点击回调(屏幕坐标)
    typedef void (*LeftClickCallback)(HWND hTargetWnd, int x, int y);
    void SetLeftClickCallback(LeftClickCallback callback) { m_callback = callback; }

private:
    TaskbarOverlay();
    ~TaskbarOverlay();

    TaskbarOverlay(const TaskbarOverlay&) = delete;
    TaskbarOverlay& operator=(const TaskbarOverlay&) = delete;

    // 覆盖窗窗口过程
    static LRESULT CALLBACK OverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 查找 TrafficMonitor 任务栏窗口
    HWND FindTaskbarWindow() const;

    // 让覆盖窗跟随任务栏窗口的位置和可见状态
    void SyncPosition();

    // 把鼠标消息转发给任务栏窗口
    void ForwardMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND m_hTargetWnd = nullptr;        // TrafficMonitor 任务栏窗口
    HWND m_hOverlayWnd = nullptr;       // 透明覆盖窗
    LeftClickCallback m_callback = nullptr;
    bool m_trackingLeave = false;       // 是否已注册 WM_MOUSELEAVE 追踪
    DWORD m_lastRButtonUpTick = 0;      // 最近一次转发右键抬起的时刻,用于避免重复弹出右键菜单
};
