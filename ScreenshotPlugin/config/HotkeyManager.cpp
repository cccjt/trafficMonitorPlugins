#include "../pch/stdafx.h"
#include "HotkeyManager.h"
#include "../core/ScreenshotPlugin.h"
#include "../capture/ScreenCapture.h"
#include <sstream>
#include <thread>

// 获取当前 DLL 的 HINSTANCE
static HMODULE GetCurrentModule()
{
    HMODULE hModule = nullptr;
    ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetCurrentModule),
        &hModule);
    return hModule;
}

HotkeyManager::HotkeyManager() {}
HotkeyManager::~HotkeyManager()
{
    Destroy();
}

bool HotkeyManager::Initialize()
{
    if (m_hWnd != nullptr) return true;

    HMODULE hModule = GetCurrentModule();

    // 生成唯一类名
    std::wostringstream oss;
    oss << L"ScreenshotHotkeyManagerWnd_" << reinterpret_cast<size_t>(this);
    m_className = oss.str();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &HotkeyManager::WndProc;
    wc.hInstance = hModule;
    wc.lpszClassName = m_className.c_str();

    if (!::RegisterClassExW(&wc))
    {
        if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    // 创建消息窗口(message-only window)
    m_hWnd = ::CreateWindowExW(
        0, m_className.c_str(), L"ScreenshotHotkeyManager",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hModule, this);

    return m_hWnd != nullptr;
}

void HotkeyManager::Destroy()
{
    HMODULE hModule = GetCurrentModule();
    UnregisterAll();
    if (m_hWnd != nullptr)
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (!m_className.empty())
    {
        ::UnregisterClassW(m_className.c_str(), hModule);
        m_className.clear();
    }
}

LRESULT CALLBACK HotkeyManager::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }

    HotkeyManager* self = reinterpret_cast<HotkeyManager*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self == nullptr)
    {
        return ::DefWindowProcW(hWnd, message, wParam, lParam);
    }

    if (message == WM_HOTKEY)
    {
        self->OnHotKey(static_cast<int>(wParam));
        return 0;
    }

    // 自定义消息转发到 ScreenshotPlugin
    if (message == WM_USER_START_SCREENSHOT)
    {
        ScreenshotPlugin::Instance().OnScreenshotReady(
            static_cast<ActionType>(wParam),
            reinterpret_cast<HBITMAP>(lParam));
        return 0;
    }
    if (message == WM_USER_CLIPBOARD_ACTION)
    {
        ScreenshotPlugin::Instance().OnClipboardAction(static_cast<ActionType>(wParam));
        return 0;
    }

    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

std::vector<ActionType> HotkeyManager::RegisterAll(const std::map<ActionType, ScreenshotHotkeyItem>& hotkeys)
{
    UnregisterAll();
    std::vector<ActionType> failed;

    if (m_hWnd == nullptr)
    {
        for (const auto& kv : hotkeys) failed.push_back(kv.first);
        return failed;
    }

    for (const auto& kv : hotkeys)
    {
        const ScreenshotHotkeyItem& item = kv.second;
        if (!item.enabled) continue;
        if (!item.IsValid())
        {
            failed.push_back(kv.first);
            continue;
        }

        int id = m_nextId++;
        if (::RegisterHotKey(m_hWnd, id, item.modifiers, item.vk))
        {
            m_registered[id] = kv.first;
        }
        else
        {
            failed.push_back(kv.first);
        }
    }
    return failed;
}

void HotkeyManager::UnregisterAll()
{
    if (m_hWnd != nullptr)
    {
        for (const auto& kv : m_registered)
        {
            ::UnregisterHotKey(m_hWnd, kv.first);
        }
    }
    m_registered.clear();
}

void HotkeyManager::OnHotKey(int id)
{
    auto it = m_registered.find(id);
    if (it == m_registered.end()) return;
    ActionType action = it->second;

    // 剪贴板类操作:不需要截图选区,直接 PostMessage 到主线程
    if (action == ActionType::ClipboardOcr || action == ActionType::ClipboardTranslate)
    {
        ::PostMessageW(m_hWnd, WM_USER_CLIPBOARD_ACTION, static_cast<WPARAM>(action), 0);
        return;
    }

    // 截图类操作:后台线程预捕获屏幕,避免覆盖层进入截图
    // 注意:不捕获 this,只捕获 HWND,避免 HotkeyManager 销毁后的悬空指针
    HWND hWnd = m_hWnd;
    std::thread([action, hWnd]() {
        HBITMAP hBmp = ScreenCapture::CaptureAllMonitors();
        ::PostMessageW(hWnd, WM_USER_START_SCREENSHOT,
            static_cast<WPARAM>(action), reinterpret_cast<LPARAM>(hBmp));
    }).detach();
}
