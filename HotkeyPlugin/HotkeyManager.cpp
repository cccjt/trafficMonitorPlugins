#include "stdafx.h"
#include "HotkeyManager.h"
#include <sstream>

// 自定义窗口消息用于在窗口过程中获取对象指针
static const UINT WM_HOTKEY_PLUGIN_NOTIFY = WM_USER + 0x100;

HotkeyManager::HotkeyManager() {}
HotkeyManager::~HotkeyManager()
{
    Destroy();
}

bool HotkeyManager::Initialize()
{
    if (m_hWnd != nullptr) return true;

    // 生成唯一类名
    std::wostringstream oss;
    oss << L"HotkeyManagerWnd_" << reinterpret_cast<size_t>(this);
    m_className = oss.str();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &HotkeyManager::WndProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = m_className.c_str();

    if (!::RegisterClassExW(&wc))
    {
        DWORD err = ::GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    // 创建消息窗口(message-only window)
    m_hWnd = ::CreateWindowExW(
        0, m_className.c_str(), L"HotkeyManager",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, ::GetModuleHandleW(nullptr), this);

    return m_hWnd != nullptr;
}

void HotkeyManager::Destroy()
{
    UnregisterAll();
    if (m_hWnd != nullptr)
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (!m_className.empty())
    {
        ::UnregisterClassW(m_className.c_str(), ::GetModuleHandleW(nullptr));
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

std::vector<size_t> HotkeyManager::RegisterAll(const std::vector<HotkeyConfigItem>& items)
{
    UnregisterAll();
    std::vector<size_t> failed;

    if (m_hWnd == nullptr)
    {
        // 窗口未初始化,全部失败
        for (size_t i = 0; i < items.size(); ++i) failed.push_back(i);
        return failed;
    }

    for (size_t i = 0; i < items.size(); ++i)
    {
        const HotkeyConfigItem& item = items[i];
        if (!item.enabled || !item.IsValid()) continue;

        int id = m_nextId++;
        if (::RegisterHotKey(m_hWnd, id, item.modifiers, item.vk))
        {
            m_registered[id] = item;
        }
        else
        {
            failed.push_back(i);
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

    const HotkeyConfigItem& item = it->second;
    bool success = ExecuteScript(item.scriptCode);

    // 仅记录脚本代码的前缀用于调试/状态查询
    m_lastScript = item.scriptCode.substr(0, 80);
    m_lastSuccess = success;
    ::GetLocalTime(&m_lastTime);
}

// 将宽字符串按 UTF-16 LE 字节数组进行 Base64 编码(PowerShell -EncodedCommand 需要)
static std::wstring Base64EncodeWString(const std::wstring& str)
{
    static const wchar_t base64Chars[] =
        L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const BYTE* bytes = reinterpret_cast<const BYTE*>(str.c_str());
    size_t byteCount = str.size() * sizeof(wchar_t);
    std::wstring result;
    result.reserve(((byteCount + 2) / 3) * 4);

    for (size_t i = 0; i < byteCount; i += 3)
    {
        size_t remaining = byteCount - i;
        DWORD triple = bytes[i];
        if (remaining > 1) triple |= (static_cast<DWORD>(bytes[i + 1]) << 8);
        if (remaining > 2) triple |= (static_cast<DWORD>(bytes[i + 2]) << 16);

        result.push_back(base64Chars[(triple >> 0) & 0x3F]);
        result.push_back(base64Chars[(triple >> 6) & 0x3F]);
        result.push_back(remaining > 1 ? base64Chars[(triple >> 12) & 0x3F] : L'=');
        result.push_back(remaining > 2 ? base64Chars[(triple >> 18) & 0x3F] : L'=');
    }
    return result;
}

bool HotkeyManager::ExecuteScript(const std::wstring& scriptCode)
{
    if (scriptCode.empty()) return false;

    std::wstring encoded = Base64EncodeWString(scriptCode);

    // 构造命令行:powershell.exe -ExecutionPolicy Bypass -NoProfile -EncodedCommand <base64>
    std::wostringstream cmd;
    cmd << L"powershell.exe -ExecutionPolicy Bypass -NoProfile -EncodedCommand "
        << encoded;

    std::wstring cmdLine = cmd.str();

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(0);

    BOOL ok = ::CreateProcessW(
        nullptr,
        buf.data(),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);

    if (!ok) return false;

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}
