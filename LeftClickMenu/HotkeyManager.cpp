#include "stdafx.h"
#include "HotkeyManager.h"
#include "Logger.h"
#include <sstream>

namespace
{
// 获取当前 DLL 的 HINSTANCE
HMODULE GetCurrentModule()
{
    HMODULE hModule = nullptr;
    ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetCurrentModule),
        &hModule);
    return hModule;
}
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
    oss << L"LeftClickMenuHotkeyWnd_" << reinterpret_cast<size_t>(this);
    m_className = oss.str();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &HotkeyManager::WndProc;
    wc.hInstance = hModule;
    wc.lpszClassName = m_className.c_str();

    if (!::RegisterClassExW(&wc))
    {
        if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            Logger::Instance().Warn(L"HotkeyManager::Initialize: 注册窗口类失败, 错误码="
                + std::to_wstring(::GetLastError()));
            return false;
        }
    }

    // 创建消息窗口(message-only window)
    m_hWnd = ::CreateWindowExW(
        0, m_className.c_str(), L"LeftClickMenuHotkey",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hModule, this);

    if (m_hWnd == nullptr)
    {
        Logger::Instance().Warn(L"HotkeyManager::Initialize: 创建消息窗口失败, 错误码="
            + std::to_wstring(::GetLastError()));
        return false;
    }

    Logger::Instance().Info(L"HotkeyManager: 消息窗口创建成功");
    return true;
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
        // 保存 this 指针
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

    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

std::vector<size_t> HotkeyManager::RegisterAll(const std::vector<MenuConfigItem>& items)
{
    UnregisterAll();
    std::vector<size_t> failed;

    if (m_hWnd == nullptr)
    {
        Logger::Instance().Warn(L"HotkeyManager::RegisterAll: 消息窗口未创建,无法注册热键");
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (!items[i].isSeparator && items[i].enabled && items[i].hotkeyVk != 0)
            {
                failed.push_back(i);
            }
        }
        return failed;
    }

    for (size_t i = 0; i < items.size(); ++i)
    {
        const MenuConfigItem& item = items[i];
        if (item.isSeparator || !item.enabled || item.hotkeyVk == 0) continue;

        int id = m_nextId++;
        if (::RegisterHotKey(m_hWnd, id, item.hotkeyModifiers, item.hotkeyVk))
        {
            m_registered[id] = item;
            Logger::Instance().Info(L"HotkeyManager: 已注册热键 ["
                + item.GetHotkeyString() + L"], 菜单项=[" + item.name + L"]");
        }
        else
        {
            failed.push_back(i);
            Logger::Instance().Warn(L"HotkeyManager: 热键注册失败(可能被占用) ["
                + item.GetHotkeyString() + L"], 菜单项=[" + item.name + L"]");
        }
    }

    Logger::Instance().Info(L"HotkeyManager: 注册完成, 成功 "
        + std::to_wstring(m_registered.size()) + L" 个, 失败 "
        + std::to_wstring(failed.size()) + L" 个");
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

    const MenuConfigItem& item = it->second;
    Logger::Instance().Info(L"HotkeyManager: 热键触发 ["
        + item.GetHotkeyString() + L"], 菜单项=[" + item.name + L"]");

    if (m_callback)
    {
        m_callback(item);
    }
}
