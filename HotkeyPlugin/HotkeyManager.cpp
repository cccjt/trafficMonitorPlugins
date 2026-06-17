#include "stdafx.h"
#include "HotkeyManager.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <thread>
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

    std::wstring psExe = FindPowerShellExe();

    // 生成临时脚本文件,避免 -EncodedCommand 编码问题
    wchar_t tempPath[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, tempPath);
    std::wstring scriptFile = std::wstring(tempPath) + L"HotkeyPlugin_Run_"
        + std::to_wstring(::GetCurrentProcessId()) + L"_"
        + std::to_wstring(::GetTickCount()) + L".ps1";

    {
        // 将脚本代码保存为 UTF-8 with BOM,兼容性最好
        int size = ::WideCharToMultiByte(CP_UTF8, 0, scriptCode.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            DebugLog(L"ExecuteScript: WideCharToMultiByte failed");
            return false;
        }
        std::string utf8(size, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, scriptCode.c_str(), -1, &utf8[0], size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();

        std::ofstream fs(scriptFile, std::ios::binary);
        if (!fs.is_open())
        {
            DebugLog(L"ExecuteScript: failed to write temp script " + scriptFile);
            return false;
        }
        const char bom[] = "\xEF\xBB\xBF";
        fs.write(bom, 3);
        fs.write(utf8.data(), utf8.size());
    }

    DebugLog(L"ExecuteScript: scriptFile=" + scriptFile);

    // 构造命令行:<powershell> -ExecutionPolicy Bypass -NoProfile -File "temp.ps1"
    std::wostringstream cmd;
    cmd << L"\"" << psExe << L"\" -ExecutionPolicy Bypass -NoProfile -File \""
        << scriptFile << L"\"";

    std::wstring cmdLine = cmd.str();
    DebugLog(L"ExecuteScript: cmdLen=" + std::to_wstring(cmdLine.size()));

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
        ::DeleteFileW(scriptFile.c_str());
        return false;
    }

    DebugLog(L"ExecuteScript: CreateProcess ok pid=" + std::to_wstring(pi.dwProcessId));

    // 启动一个后台线程,等进程结束后删除临时脚本文件
    HANDLE hProcess = pi.hProcess;
    HANDLE hThread = pi.hThread;
    std::thread([hProcess, hThread, scriptFile]() {
        ::WaitForSingleObject(hProcess, INFINITE);
        ::CloseHandle(hProcess);
        ::CloseHandle(hThread);
        ::DeleteFileW(scriptFile.c_str());
    }).detach();

    return true;
}
