#include <Windows.h>
#include <powrprof.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>

#include "../resource.h"
#include "HeartbeatManager.h"
#include "UsageReportPlugin.h"
#include "UsageReportPopup.h"

#pragma comment(lib, "powrprof.lib")

namespace
{
const wchar_t* kReportVersion = L"2026-11-07.1";
const wchar_t* kHtmlName = L"pc-online-time-report.html";
const wchar_t* kScriptName = L"generate-usage-report.ps1";
const wchar_t* kDisplayLogName = L"display-events.csv";

// GUID_CONSOLE_DISPLAY_STATE：显示器电源状态（0=关，1=开，2=变暗）
const GUID kDisplayStateGuid = { 0x6fe69567, 0x7fcc, 0x4a7f, { 0xa9, 0xa1, 0x9b, 0x3f, 0x0c, 0x0e, 0x4b, 0x5b } };

HINSTANCE ModuleHandle()
{
    static HINSTANCE hModule = nullptr;
    if (hModule == nullptr)
    {
        ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ModuleHandle), &hModule);
    }
    return hModule;
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& name)
{
    if (dir.empty()) return name;
    std::wstring result = dir;
    if (result.back() != L'\\' && result.back() != L'/') result += L'\\';
    result += name;
    return result;
}

bool EnsureDirectory(const std::wstring& path)
{
    if (path.empty()) return false;

    DWORD attr = ::GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos && pos > 0)
    {
        if (!EnsureDirectory(path.substr(0, pos))) return false;
    }

    ::CreateDirectoryW(path.c_str(), nullptr);
    attr = ::GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool WriteResourceToFile(HINSTANCE hInst, UINT id, const std::wstring& path)
{
    HRSRC hRes = ::FindResourceW(hInst, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (hRes == nullptr) return false;

    DWORD size = ::SizeofResource(hInst, hRes);
    HGLOBAL hData = ::LoadResource(hInst, hRes);
    if (hData == nullptr) return false;

    const void* pData = ::LockResource(hData);
    if (pData == nullptr || size == 0) return false;

    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = ::WriteFile(hFile, pData, size, &written, nullptr);
    ::CloseHandle(hFile);
    return ok && written == size;
}

std::wstring ReadTextFile(const std::wstring& path)
{
    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return std::wstring();

    DWORD size = ::GetFileSize(hFile, nullptr);
    std::string buffer(size, 0);
    DWORD read = 0;
    ::ReadFile(hFile, buffer.empty() ? nullptr : &buffer[0], size, &read, nullptr);
    ::CloseHandle(hFile);

    buffer.resize(read);
    std::wstring result;
    if (!buffer.empty())
    {
        int wlen = ::MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(buffer.size()), nullptr, 0);
        if (wlen > 0)
        {
            result.resize(wlen);
            ::MultiByteToWideChar(CP_UTF8, 0, buffer.data(), static_cast<int>(buffer.size()), &result[0], wlen);
        }
    }
    return result;
}

bool WriteTextFile(const std::wstring& path, const std::wstring& text)
{
    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    int len = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    std::string buffer(len, 0);
    if (len > 0)
    {
        ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            &buffer[0], len, nullptr, nullptr);
    }

    DWORD written = 0;
    BOOL ok = ::WriteFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr);
    ::CloseHandle(hFile);
    return ok;
}

std::wstring FindPowerShell()
{
    wchar_t sysDir[MAX_PATH] = { 0 };
    ::GetSystemDirectoryW(sysDir, MAX_PATH);

    std::wstring path = std::wstring(sysDir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;

    path = std::wstring(sysDir) + L"\\powershell.exe";
    if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;

    return L"powershell.exe";
}

// 显示器电源状态变化回调，由系统在状态切换时调用（事件驱动，无需轮询）
ULONG CALLBACK DisplayStateCallback(PVOID context, ULONG type, PVOID setting)
{
    if (type == PBT_POWERSETTINGCHANGE && setting != nullptr)
    {
        DWORD state = *reinterpret_cast<DWORD*>(setting);
        UsageReportPlugin::Instance().OnDisplayStateChanged(state);
    }
    return 0;
}
}

UsageReportPlugin& UsageReportPlugin::Instance()
{
    static UsageReportPlugin instance;
    return instance;
}

UsageReportPlugin::~UsageReportPlugin()
{
    HeartbeatManager::Instance().Shutdown();

    if (m_hDisplayNotify != nullptr)
    {
        ::PowerSettingUnregisterNotification(reinterpret_cast<HPOWERNOTIFY>(m_hDisplayNotify));
        m_hDisplayNotify = nullptr;
    }
}

void UsageReportPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    m_pApp = pApp;
    m_configDir = ResolveConfigDir();
    RegisterDisplayNotification();

    HeartbeatManager::Instance().Initialize(JoinPath(ReportDir(), L"heartbeat.cfg"));

    // 弹窗常驻一个隐藏窗口，左键菜单脚本通过窗口消息唤起
    UsageReportPopup::Instance().Initialize(
        [this]() { return ReportDir(); },
        kHtmlName,
        [this]() { return GenerateReport(); },
        []() { return HeartbeatManager::Instance().BuildInitScript(); },
        [](const std::wstring& text) { HeartbeatManager::Instance().ApplyConfigText(text); });
}

void UsageReportPlugin::RegisterDisplayNotification()
{
    if (m_hDisplayNotify != nullptr) return;

    // 回调参数必须长期有效，因此放在静态存储中
    static DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS params;
    params.Callback = &DisplayStateCallback;
    params.Context = nullptr;

    HPOWERNOTIFY handle = nullptr;
    if (::PowerSettingRegisterNotification(&kDisplayStateGuid, DEVICE_NOTIFY_CALLBACK,
            reinterpret_cast<HANDLE>(&params), &handle) == ERROR_SUCCESS)
    {
        m_hDisplayNotify = handle;
    }
}

void UsageReportPlugin::OnDisplayStateChanged(unsigned long state)
{
    // 2 = 变暗，仍算亮屏
    int normalized = (state == 0) ? 0 : 1;
    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        if (normalized == m_lastDisplayState) return;
        m_lastDisplayState = normalized;
    }
    AppendDisplayEvent(normalized == 0 ? L"off" : L"on");
}

void UsageReportPlugin::AppendDisplayEvent(const wchar_t* state)
{
    std::wstring dir = ReportDir();
    if (dir.empty()) return;
    EnsureDirectory(dir);

    std::wstring path = JoinPath(dir, kDisplayLogName);
    bool exists = ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;

    SYSTEMTIME st;
    ::GetLocalTime(&st);

    wchar_t line[96] = { 0 };
    swprintf_s(line, L"%04d-%02d-%02d %02d:%02d:%02d,%s\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, state);

    std::lock_guard<std::mutex> lock(m_logMutex);
    HANDLE hFile = ::CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    if (!exists)
    {
        const char* header = "time,state\r\n";
        DWORD headerWritten = 0;
        ::WriteFile(hFile, header, static_cast<DWORD>(::strlen(header)), &headerWritten, nullptr);
    }

    int len = ::WideCharToMultiByte(CP_UTF8, 0, line, -1, nullptr, 0, nullptr, nullptr);
    if (len > 1)
    {
        std::string utf8(len, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, line, -1, &utf8[0], len, nullptr, nullptr);
        utf8.resize(len - 1);

        DWORD written = 0;
        ::WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
    ::CloseHandle(hFile);
}

void UsageReportPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    // 主程序传来的配置目录优先于 OnInitialize 时的兜底值
    if (index == EI_CONFIG_DIR && data != nullptr && data[0] != L'\0')
    {
        m_configDir = data;
        HeartbeatManager::Instance().Initialize(JoinPath(ReportDir(), L"heartbeat.cfg"));
    }
}

const wchar_t* UsageReportPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"使用时长报告";
    case TMI_DESCRIPTION: return L"读取 Windows 电源/锁屏事件，实时统计每日电脑使用时长并生成可交互报告";
    case TMI_AUTHOR:      return L"TrafficMonitor Plugins";
    case TMI_COPYRIGHT:   return L"MIT";
    case TMI_VERSION:     return L"1.1.0";
    case TMI_URL:         return L"";
    }
    return L"";
}

const wchar_t* UsageReportPlugin::GetCommandName(int command_index)
{
    if (command_index == 0) return L"查看使用时长报告";
    return nullptr;
}

// 插件列表用的 32x32 图标，直接取自 DLL 内的图标资源
void* UsageReportPlugin::GetPluginIcon()
{
    static HICON icon = nullptr;
    if (icon == nullptr)
    {
        icon = static_cast<HICON>(::LoadImageW(ModuleHandle(),
            MAKEINTRESOURCEW(IDI_PLUGIN_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    }
    return icon;
}

// 右键菜单插件命令使用的小图标
void* UsageReportPlugin::GetCommandIcon(int command_index)
{
    if (command_index != 0) return nullptr;

    static HICON icon = nullptr;
    if (icon == nullptr)
    {
        icon = static_cast<HICON>(::LoadImageW(ModuleHandle(),
            MAKEINTRESOURCEW(IDI_PLUGIN_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    }
    return icon;
}

UsageReportPlugin::OptionReturn UsageReportPlugin::ShowOptionsDialog(void* hParent)
{
    // 某些主程序版本的插件菜单只提供“选项”入口，这里复用同一动作
    OnPluginCommand(0, hParent, nullptr);
    return OR_OPTION_UNCHANGED;
}

void UsageReportPlugin::OnPluginCommand(int command_index, void* hWnd, void* para)
{
    if (command_index != 0) return;

    // 弹窗内部会在后台线程重新生成数据，然后加载页面
    UsageReportPopup::Instance().RequestShow();
}

std::wstring UsageReportPlugin::ResolveConfigDir() const
{
    if (m_pApp != nullptr)
    {
        const wchar_t* dir = m_pApp->GetPluginConfigDir();
        if (dir != nullptr && dir[0] != L'\0') return dir;
    }

    if (!m_configDir.empty()) return m_configDir;

    // 兜底：使用 DLL 所在目录
    wchar_t modulePath[MAX_PATH] = { 0 };
    ::GetModuleFileNameW(ModuleHandle(), modulePath, MAX_PATH);
    std::wstring path = modulePath;
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? std::wstring() : path.substr(0, pos);
}

std::wstring UsageReportPlugin::ReportDir() const
{
    return JoinPath(m_configDir, L"UsageReport");
}

void UsageReportPlugin::EnsureReportFiles()
{
    std::wstring dir = ReportDir();
    if (dir.empty()) return;

    EnsureDirectory(dir);
    EnsureDirectory(JoinPath(dir, L"assets"));
    EnsureDirectory(JoinPath(dir, L"_shared"));
    EnsureDirectory(JoinPath(dir, L"_shared\\js"));

    // 版本一致时无需重复释放
    if (ReadTextFile(JoinPath(dir, L".version")) == kReportVersion) return;

    HINSTANCE hInst = ModuleHandle();
    WriteResourceToFile(hInst, IDR_REPORT_HTML, JoinPath(dir, kHtmlName));
    WriteResourceToFile(hInst, IDR_REPORT_CHARTS, JoinPath(dir, L"assets\\charts.js"));
    WriteResourceToFile(hInst, IDR_REPORT_ECHARTS, JoinPath(dir, L"_shared\\js\\echarts.min.js"));
    WriteResourceToFile(hInst, IDR_REPORT_SCRIPT, JoinPath(dir, kScriptName));
    WriteTextFile(JoinPath(dir, L".version"), kReportVersion);
}

bool UsageReportPlugin::GenerateReport()
{
    EnsureReportFiles();

    std::wstring dir = ReportDir();
    std::wstring script = JoinPath(dir, kScriptName);
    if (::GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    std::wstring psExe = FindPowerShell();
    std::wstring cmdLine = L"\"" + psExe + L"\" -NoProfile -ExecutionPolicy Bypass"
        L" -WindowStyle Hidden -File \"" + script + L"\" -ReportDir \"" + dir + L"\"";

    STARTUPINFOW si = { 0 };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> buffer(cmdLine.begin(), cmdLine.end());
    buffer.push_back(0);

    BOOL started = ::CreateProcessW(psExe.c_str(), buffer.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi);
    if (!started) return false;

    DWORD waitResult = ::WaitForSingleObject(pi.hProcess, 30000);
    DWORD exitCode = 1;
    if (waitResult == WAIT_TIMEOUT)
    {
        ::TerminateProcess(pi.hProcess, 1);
    }
    ::GetExitCodeProcess(pi.hProcess, &exitCode);

    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    if (waitResult == WAIT_TIMEOUT || exitCode != 0) return false;

    std::wstring dataPath = JoinPath(dir, L"assets\\data.js");
    return ::GetFileAttributesW(dataPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &UsageReportPlugin::Instance();
}
