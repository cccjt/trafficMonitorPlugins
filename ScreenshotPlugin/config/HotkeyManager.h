#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include "ActionType.h"
#include "ScreenshotConfig.h"

// 自定义消息:启动截图流程
//   wParam = ActionType
//   lParam = HBITMAP(完整屏幕截图,所有权转交主线程)
#define WM_USER_START_SCREENSHOT   (WM_USER + 100)

// 自定义消息:剪贴板类操作(不截图)
//   wParam = ActionType
#define WM_USER_CLIPBOARD_ACTION   (WM_USER + 101)

// 热键管理器:负责注册全局热键、接收消息、路由到 ScreenshotPlugin
class HotkeyManager
{
public:
    HotkeyManager();
    ~HotkeyManager();

    // 初始化:创建隐藏消息窗口
    bool Initialize();

    // 销毁:注销所有热键并销毁窗口
    void Destroy();

    // 根据配置注册所有热键
    // 返回失败的功能列表(用于提示用户冲突)
    std::vector<ActionType> RegisterAll(const std::map<ActionType, ScreenshotHotkeyItem>& hotkeys);

    // 注销所有热键
    void UnregisterAll();

    // 获取已注册的热键数量
    int GetRegisteredCount() const { return static_cast<int>(m_registered.size()); }

    // 获取消息窗口句柄(供主线程 PostMessage)
    HWND GetHwnd() const { return m_hWnd; }

private:
    // 隐藏窗口的窗口过程(静态,转发到对象)
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 处理 WM_HOTKEY 消息
    void OnHotKey(int id);

private:
    HWND m_hWnd = nullptr;                                  // 隐藏消息窗口句柄
    std::wstring m_className;                               // 窗口类名
    std::map<int, ActionType> m_registered;                 // 已注册的热键 id -> 功能
    int m_nextId = 0xC000;                                  // 下一个热键 ID(从 0xC000 开始,避免冲突)
};
