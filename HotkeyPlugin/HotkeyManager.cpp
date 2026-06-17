#include "HotkeyManager.h"
#include <sstream>

// 自定义窗口消息用于在窗口过程中获取对象指针
static const UINT WM_HOTKEY_PLUGIN_NOTIFY = WM_USER + 0x100;

HotkeyManager::HotkeyManager() {}
HotkeyManager::~HotkeyManager()
{
    Destroy();
}

std::wstring HotkeyManager::GetPluginDir()
{
    wchar_t path[MAX_PATH] = { 0 };
    HMODULE hModule = nullptr;
    if (::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetPluginDir),
        &hModule) && hModule != nullptr)
    {
        ::GetModuleFileNameW(hModule, path, MAX_PATH);
        std::wstring p(path);
        size_t pos = p.find_last_of(L"\\/");
        if (pos != std::wstring::npos) p = p.substr(0, pos);
        return p;
    }
    return L".";
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

std::vector<size_t> HotkeyManager::RegisterAll(const std::vector<HotkeyItem>& items)
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
        const HotkeyItem& item = items[i];
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

void HotkeyManager::SetCallback(HotkeyExecutedCallback cb, void* userData)
{
    m_callback = cb;
    m_callbackUserData = userData;
}

void HotkeyManager::OnHotKey(int id)
{
    auto it = m_registered.find(id);
    if (it == m_registered.end()) return;

    const HotkeyItem& item = it->second;
    bool success = ExecuteScript(item.scriptPath);

    m_lastScript = item.scriptPath;
    m_lastSuccess = success;
    ::GetLocalTime(&m_lastTime);

    if (m_callback)
    {
        m_callback(item.scriptPath, success, m_callbackUserData);
    }
}

std::wstring HotkeyManager::ResolveScriptPath(const std::wstring& scriptPath) const
{
    if (scriptPath.empty()) return scriptPath;

    // 判断是否为绝对路径
    if (scriptPath.size() >= 2 && scriptPath[1] == L':')
    {
        return scriptPath;
    }
    if (!scriptPath.empty() && (scriptPath[0] == L'\\' || scriptPath[0] == L'/'))
    {
        return scriptPath;
    }

    // 相对路径:基于插件目录
    std::wstring dir = GetPluginDir();
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/')
    {
        dir += L'\\';
    }
    return dir + scriptPath;
}

bool HotkeyManager::ExecuteScript(const std::wstring& scriptPath)
{
    std::wstring fullPath = ResolveScriptPath(scriptPath);
    if (fullPath.empty()) return false;

    // 检查文件是否存在
    DWORD attr = ::GetFileAttributesW(fullPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        return false;
    }

    // 构造命令行:powershell.exe -ExecutionPolicy Bypass -NoProfile -File "<path>"
    std::wostringstream cmd;
    cmd << L"powershell.exe -ExecutionPolicy Bypass -NoProfile -File \""
        << fullPath << L"\"";

    std::wstring cmdLine = cmd.str();

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    // CreateProcess 可能修改命令行,需要可写缓冲
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

    // 不等待脚本完成,关闭句柄立即返回(异步执行)
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}
