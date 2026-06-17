#include "stdafx.h"
#include "HotkeyManager.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

// 自定义窗口消息用于在窗口过程中获取对象指针
static const UINT WM_HOTKEY_PLUGIN_NOTIFY = WM_USER + 0x100;

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

// 写入调试日志到 %TEMP%\HotkeyPlugin_Debug.log
static void DebugLog(const std::wstring& msg)
{
    wchar_t tempPath[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, tempPath);
    std::wstring logPath = std::wstring(tempPath) + L"HotkeyPlugin_Debug.log";

    SYSTEMTIME st = { 0 };
    ::GetLocalTime(&st);

    std::wofstream fs(logPath, std::ios::app);
    if (!fs.is_open()) return;

    fs << L"[" << std::setfill(L'0')
       << std::setw(4) << st.wYear << L"-"
       << std::setw(2) << st.wMonth << L"-"
       << std::setw(2) << st.wDay << L" "
       << std::setw(2) << st.wHour << L":"
       << std::setw(2) << st.wMinute << L":"
       << std::setw(2) << st.wSecond << L"."
       << std::setw(3) << st.wMilliseconds << L"] "
       << msg << std::endl;
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
    DebugLog(L"Initialize: hModule=" + std::to_wstring(reinterpret_cast<size_t>(hModule)));

    // 生成唯一类名
    std::wostringstream oss;
    oss << L"HotkeyManagerWnd_" << reinterpret_cast<size_t>(this);
    m_className = oss.str();

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &HotkeyManager::WndProc;
    wc.hInstance = hModule;
    wc.lpszClassName = m_className.c_str();

    if (!::RegisterClassExW(&wc))
    {
        DWORD err = ::GetLastError();
        DebugLog(L"RegisterClassExW failed, err=" + std::to_wstring(err));
        if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
    }
    else
    {
        DebugLog(L"RegisterClassExW ok, class=" + m_className);
    }

    // 创建消息窗口(message-only window)
    m_hWnd = ::CreateWindowExW(
        0, m_className.c_str(), L"HotkeyManager",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hModule, this);

    if (m_hWnd == nullptr)
    {
        DWORD err = ::GetLastError();
        DebugLog(L"CreateWindowExW failed, err=" + std::to_wstring(err));
    }
    else
    {
        DebugLog(L"CreateWindowExW ok, hWnd=" + std::to_wstring(reinterpret_cast<size_t>(m_hWnd)));
    }

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
        DebugLog(L"WndProc WM_HOTKEY id=" + std::to_wstring(static_cast<int>(wParam)));
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
        DebugLog(L"RegisterAll: window not initialized, count=" + std::to_wstring(items.size()));
        for (size_t i = 0; i < items.size(); ++i) failed.push_back(i);
        return failed;
    }

    DebugLog(L"RegisterAll: item count=" + std::to_wstring(items.size()));
    for (size_t i = 0; i < items.size(); ++i)
    {
        const HotkeyConfigItem& item = items[i];
        if (!item.enabled)
        {
            DebugLog(L"RegisterAll[" + std::to_wstring(i) + L"]: disabled");
            continue;
        }
        if (!item.IsValid())
        {
            DebugLog(L"RegisterAll[" + std::to_wstring(i) + L"]: invalid (mod="
                + std::to_wstring(item.modifiers) + L" vk=" + std::to_wstring(item.vk) + L")");
            continue;
        }

        int id = m_nextId++;
        BOOL ok = ::RegisterHotKey(m_hWnd, id, item.modifiers, item.vk);
        if (ok)
        {
            DebugLog(L"RegisterAll[" + std::to_wstring(i) + L"]: ok id=" + std::to_wstring(id)
                + L" mod=" + std::to_wstring(item.modifiers) + L" vk=" + std::to_wstring(item.vk));
            m_registered[id] = item;
        }
        else
        {
            DWORD err = ::GetLastError();
            DebugLog(L"RegisterAll[" + std::to_wstring(i) + L"]: FAILED err=" + std::to_wstring(err)
                + L" mod=" + std::to_wstring(item.modifiers) + L" vk=" + std::to_wstring(item.vk));
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
    if (it == m_registered.end())
    {
        DebugLog(L"OnHotKey: id " + std::to_wstring(id) + L" not found");
        return;
    }

    const HotkeyConfigItem& item = it->second;
    DebugLog(L"OnHotKey: id=" + std::to_wstring(id) + L" script=" + item.scriptCode.substr(0, 40));

    bool success = ExecuteScript(item.scriptCode);

    // 仅记录脚本代码的前缀用于调试/状态查询
    m_lastScript = item.scriptCode.substr(0, 80);
    m_lastSuccess = success;
    ::GetLocalTime(&m_lastTime);

    DebugLog(L"OnHotKey: ExecuteScript returned " + std::wstring(success ? L"true" : L"false"));
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

// 查找可用的 PowerShell 可执行文件
static std::wstring FindPowerShellExe()
{
    wchar_t sysDir[MAX_PATH] = { 0 };
    ::GetSystemDirectoryW(sysDir, MAX_PATH);
    std::wstring psPath = std::wstring(sysDir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (::PathFileExistsW(psPath.c_str())) return psPath;

    psPath = std::wstring(sysDir) + L"\\powershell.exe";
    if (::PathFileExistsW(psPath.c_str())) return psPath;

    wchar_t pathEnv[32768] = { 0 };
    if (::GetEnvironmentVariableW(L"PATH", pathEnv, 32768) > 0)
    {
        std::wistringstream iss(pathEnv);
        std::wstring dir;
        while (std::getline(iss, dir, L';'))
        {
            if (dir.empty()) continue;
            if (dir.back() != L'\\' && dir.back() != L'/') dir += L'\\';
            std::wstring candidate = dir + L"powershell.exe";
            if (::PathFileExistsW(candidate.c_str())) return candidate;
            candidate = dir + L"pwsh.exe";
            if (::PathFileExistsW(candidate.c_str())) return candidate;
        }
    }

    // 最后尝试直接调用,依赖 PATH
    return L"powershell.exe";
}

bool HotkeyManager::ExecuteScript(const std::wstring& scriptCode)
{
    if (scriptCode.empty()) return false;

    std::wstring encoded = Base64EncodeWString(scriptCode);
    std::wstring psExe = FindPowerShellExe();

    // 构造命令行:<powershell> -ExecutionPolicy Bypass -NoProfile -EncodedCommand <base64>
    std::wostringstream cmd;
    cmd << L"\"" << psExe << L"\" -ExecutionPolicy Bypass -NoProfile -EncodedCommand " << encoded;

    std::wstring cmdLine = cmd.str();
    DebugLog(L"ExecuteScript: exe=" + psExe + L" cmdLen=" + std::to_wstring(cmdLine.size()));

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };

    std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
    buf.push_back(0);

    BOOL ok = ::CreateProcessW(
        psExe.c_str(),
        buf.data(),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);

    if (!ok)
    {
        DWORD err = ::GetLastError();
        DebugLog(L"ExecuteScript: CreateProcess failed err=" + std::to_wstring(err));
        return false;
    }

    DebugLog(L"ExecuteScript: CreateProcess ok pid=" + std::to_wstring(pi.dwProcessId));
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}
