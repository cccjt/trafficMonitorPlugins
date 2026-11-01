#pragma once
#include <Windows.h>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "MenuConfig.h"

// 全局热键管理器:创建隐藏消息窗口注册热键,触发时执行对应菜单项的脚本
class HotkeyManager
{
public:
    // 热键触发时的执行回调
    using ActionCallback = std::function<void(const MenuConfigItem&)>;

    HotkeyManager();
    ~HotkeyManager();

    // 设置热键触发后的执行回调
    void SetActionCallback(ActionCallback callback) { m_callback = std::move(callback); }

    // 初始化:创建隐藏消息窗口
    bool Initialize();

    // 销毁:注销所有热键并销毁窗口
    void Destroy();

    // 注册所有启用且设置了热键的菜单项
    // 返回注册失败的项索引(热键无效或被其他程序占用)
    std::vector<size_t> RegisterAll(const std::vector<MenuConfigItem>& items);

    // 注销所有热键
    void UnregisterAll();

    // 已注册的热键数量
    int GetRegisteredCount() const { return static_cast<int>(m_registered.size()); }

private:
    // 隐藏窗口的窗口过程(静态,转发到对象)
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 处理 WM_HOTKEY 消息
    void OnHotKey(int id);

private:
    HWND m_hWnd = nullptr;                          // 隐藏消息窗口句柄
    std::wstring m_className;                       // 窗口类名
    std::map<int, MenuConfigItem> m_registered;     // 已注册的热键 id -> 菜单项副本
    int m_nextId = 0xC000;                          // 下一个热键 ID(从 0xC000 开始,避免冲突)
    ActionCallback m_callback;                      // 热键触发回调
};
