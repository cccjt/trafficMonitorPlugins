#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include "HotkeyConfig.h"

// 热键执行结果回调(用于通知主插件类更新显示)
typedef void (*HotkeyExecutedCallback)(const std::wstring& scriptPath, bool success, void* userData);

// 热键管理器:负责注册全局热键、接收消息、执行 PowerShell 脚本
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
    // 返回失败的索引列表(用于提示用户冲突)
    std::vector<size_t> RegisterAll(const std::vector<HotkeyConfigItem>& items);

    // 注销所有热键
    void UnregisterAll();

    // 设置脚本执行回调
    void SetCallback(HotkeyExecutedCallback cb, void* userData);

    // 获取最后执行的脚本路径
    const std::wstring& GetLastScript() const { return m_lastScript; }

    // 获取最后执行是否成功
    bool GetLastSuccess() const { return m_lastSuccess; }

    // 获取最后执行时间
    const SYSTEMTIME& GetLastTime() const { return m_lastTime; }

    // 获取已注册的热键数量
    int GetRegisteredCount() const { return static_cast<int>(m_registered.size()); }

private:
    // 隐藏窗口的窗口过程(静态,转发到对象)
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 处理 WM_HOTKEY 消息
    void OnHotKey(int id);

    // 执行 PowerShell 脚本
    bool ExecuteScript(const std::wstring& scriptPath);

    // 获取脚本绝对路径(支持相对路径,相对于插件目录)
    std::wstring ResolveScriptPath(const std::wstring& scriptPath) const;

    // 获取插件 DLL 所在目录
    static std::wstring GetPluginDir();

private:
    HWND m_hWnd = nullptr;                              // 隐藏消息窗口句柄
    std::wstring m_className;                           // 窗口类名
    std::map<int, HotkeyConfigItem> m_registered;       // 已注册的热键 id -> 配置
    int m_nextId = 0xC000;                              // 下一个热键 ID(从 0xC000 开始,避免冲突)

    HotkeyExecutedCallback m_callback = nullptr;        // 执行回调
    void* m_callbackUserData = nullptr;                 // 回调用户数据

    std::wstring m_lastScript;                          // 最后执行的脚本路径
    bool m_lastSuccess = false;                         // 最后执行是否成功
    SYSTEMTIME m_lastTime = { 0 };                      // 最后执行时间
};
