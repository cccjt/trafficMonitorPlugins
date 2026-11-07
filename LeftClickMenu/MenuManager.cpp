#include "stdafx.h"
#include "MenuManager.h"
#include "MenuPanel.h"
#include "Logger.h"
#include <sstream>
#include <fstream>
#include <thread>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

MenuManager::MenuManager() {}
MenuManager::~MenuManager() {}

int MenuManager::ShowPopupMenu(const std::vector<MenuConfigItem>& items, HWND hWnd, int x, int y,
    const MenuViewOptions& viewOptions, bool* viewModeChanged, bool* tileMode)
{
    if (viewModeChanged != nullptr) *viewModeChanged = false;
    if (tileMode != nullptr) *tileMode = viewOptions.tileMode;

    Logger::Instance().Info(L"ShowPopupMenu: 显示菜单面板, 屏幕坐标=("
        + std::to_wstring(x) + L"," + std::to_wstring(y) + L"), 菜单项="
        + std::to_wstring(items.size()) + L", 平铺="
        + (viewOptions.tileMode ? L"是" : L"否"));

    MenuPanel::Result result = MenuPanel::Show(items, hWnd, x, y, viewOptions);

    if (viewModeChanged != nullptr) *viewModeChanged = result.viewModeChanged;
    if (tileMode != nullptr) *tileMode = result.tileMode;

    if (result.selectedIndex >= 0 && result.selectedIndex < static_cast<int>(items.size()))
    {
        const MenuConfigItem& selectedItem = items[result.selectedIndex];
        if (selectedItem.enabled && !selectedItem.isSeparator)
        {
            ExecuteAction(selectedItem);
            return result.selectedIndex;
        }
    }

    Logger::Instance().Info(L"ShowPopupMenu: 用户未选择菜单项");
    return -1;
}

bool MenuManager::ExecuteAction(const MenuConfigItem& item)
{
    if (item.scriptCode.empty())
    {
        Logger::Instance().Warn(L"ExecuteAction: 脚本代码为空, 菜单项=[" + item.name + L"]");
        return false;
    }

    Logger::Instance().Info(L"ExecuteAction: 执行菜单项=[" + item.name + L"]");
    return ExecuteScriptFile(item.scriptCode);
}

bool MenuManager::RunScript(const std::wstring& scriptCode)
{
    Logger::Instance().Info(L"RunScript: 手动执行脚本");
    return ExecuteScriptFile(scriptCode);
}

std::wstring MenuManager::FindPowerShellExe() const
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
    Logger::Instance().Warn(L"FindPowerShellExe: 未找到 PowerShell 完整路径, 将依赖 PATH");
    return L"powershell.exe";
}

bool MenuManager::ExecuteScriptFile(const std::wstring& scriptCode)
{
    if (scriptCode.empty())
    {
        Logger::Instance().Warn(L"ExecuteScriptFile: 脚本代码为空");
        return false;
    }

    std::wstring psExe = FindPowerShellExe();

    // 生成临时脚本文件,避免 -EncodedCommand 编码问题
    wchar_t tempPath[MAX_PATH] = { 0 };
    if (::GetTempPathW(MAX_PATH, tempPath) == 0)
    {
        Logger::Instance().Error(L"ExecuteScriptFile: GetTempPathW 失败, 错误码=" + std::to_wstring(::GetLastError()));
        return false;
    }

    std::wstring scriptFile = std::wstring(tempPath) + L"LeftClickMenu_Run_"
        + std::to_wstring(::GetCurrentProcessId()) + L"_"
        + std::to_wstring(::GetTickCount()) + L".ps1";

    {
        // 将脚本代码保存为 UTF-8 with BOM,兼容性最好
        int size = ::WideCharToMultiByte(CP_UTF8, 0, scriptCode.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0)
        {
            Logger::Instance().Error(L"ExecuteScriptFile: WideCharToMultiByte 编码失败, 错误码=" + std::to_wstring(::GetLastError()));
            return false;
        }
        std::string utf8(size, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, scriptCode.c_str(), -1, &utf8[0], size, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();

        std::ofstream fs(scriptFile, std::ios::binary);
        if (!fs.is_open())
        {
            Logger::Instance().Error(L"ExecuteScriptFile: 无法创建临时脚本文件: " + scriptFile);
            return false;
        }
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
        DWORD err = ::GetLastError();
        Logger::Instance().Error(L"ExecuteScriptFile: CreateProcessW 失败, 错误码=" + std::to_wstring(err)
            + L" 命令行=" + cmdLine);
        ::DeleteFileW(scriptFile.c_str());
        return false;
    }

    Logger::Instance().Info(L"ExecuteScriptFile: PowerShell 进程已启动, PID=" + std::to_wstring(pi.dwProcessId));

    // 启动一个后台线程,等进程结束后删除临时脚本文件
    HANDLE hProcess = pi.hProcess;
    HANDLE hThread = pi.hThread;
    std::thread([hProcess, hThread, scriptFile]() {
        ::WaitForSingleObject(hProcess, INFINITE);

        // 获取退出码
        DWORD exitCode = 0;
        ::GetExitCodeProcess(hProcess, &exitCode);
        if (exitCode != 0)
        {
            Logger::Instance().Warn(L"ExecuteScriptFile: PowerShell 进程退出码=" + std::to_wstring(exitCode)
                + L" 脚本文件=" + scriptFile);
        }

        ::CloseHandle(hProcess);
        ::CloseHandle(hThread);
        ::DeleteFileW(scriptFile.c_str());
    }).detach();

    return true;
}
