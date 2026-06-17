#include "stdafx.h"
#include "HotkeyManager.h"
#include <sstream>
#include <fstream>
#include <thread>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

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
    oss << L"HotkeyManagerWnd_" << reinterpret_cast<size_t>(this);
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
        0, m_className.c_str(), L"HotkeyManager",
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
        for (size_t i = 0; i < items.size(); ++i) failed.push_back(i);
        return failed;
    }

    for (size_t i = 0; i < items.size(); ++i)
    {
        const HotkeyConfigItem& item = items[i];
        if (!item.enabled) continue;
        if (!item.IsValid())
        {
            failed.push_back(i);
            continue;
        }

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

    m_lastScript = item.scriptCode.substr(0, 80);
    m_lastSuccess = success;
    ::GetLocalTime(&m_lastTime);
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
        if (size <= 0) return false;
        std::string utf8(size, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, scriptCode.c_str(), -1, &utf8[0], size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();

        std::ofstream fs(scriptFile, std::ios::binary);
        if (!fs.is_open()) return false;
        const char bom[] = "\xEF\xBB\xBF";
        fs.write(bom, 3);
        fs.write(utf8.data(), utf8.size());
    }

    // 构造命令行:<powershell> -ExecutionPolicy Bypass -NoProfile -File "temp.ps1"
    std::wostringstream cmd;
    cmd << L"\"" << psExe << L"\" -ExecutionPolicy Bypass -NoProfile -File \""
        << scriptFile << L"\"";

    std::wstring cmdLine = cmd.str();

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
        ::DeleteFileW(scriptFile.c_str());
        return false;
    }

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
