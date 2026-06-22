#include "stdafx.h"
#include "MenuManager.h"
#include "IconManager.h"
#include "Logger.h"
#include <sstream>
#include <fstream>
#include <thread>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

MenuManager::MenuManager() {}
MenuManager::~MenuManager() {}

int MenuManager::ShowPopupMenu(const std::vector<MenuConfigItem>& items, HWND hWnd, int x, int y)
{
    if (items.empty())
    {
        Logger::Instance().Warn(L"ShowPopupMenu: 菜单项列表为空");
        return -1;
    }

    HMENU hMenu = ::CreatePopupMenu();
    if (hMenu == nullptr)
    {
        Logger::Instance().Error(L"ShowPopupMenu: CreatePopupMenu 失败, 错误码=" + std::to_wstring(::GetLastError()));
        return -1;
    }

    // 遍历配置项,添加到菜单
    // 菜单命令 ID 直接对应配置项索引(从 1 开始,0 保留)
    for (size_t i = 0; i < items.size(); ++i)
    {
        const MenuConfigItem& item = items[i];

        if (item.isSeparator)
        {
            ::AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }
        else
        {
            UINT flags = MF_STRING;
            if (!item.enabled) flags |= MF_GRAYED | MF_DISABLED;

            // 命令 ID = 索引 + 1(避免 0)
            UINT cmdId = static_cast<UINT>(i + 1);
            ::AppendMenuW(hMenu, flags, cmdId, item.name.c_str());

            // 设置菜单项图标
            if (!item.iconSource.empty())
            {
                HBITMAP hBmp = IconManager::Instance().GetBitmap(item.iconSource, item.iconIndex, 16);
                if (hBmp != nullptr)
                {
                    MENUITEMINFOW mii = { 0 };
                    mii.cbSize = sizeof(MENUITEMINFOW);
                    mii.fMask = MIIM_BITMAP;
                    mii.hbmpItem = hBmp;
                    ::SetMenuItemInfoW(hMenu, cmdId, FALSE, &mii);
                }
                else
                {
                    Logger::Instance().Warn(L"ShowPopupMenu: 无法加载图标 [" + item.iconSource + L"]");
                }
            }
        }
    }

    // 显示弹出菜单
    // TPM_RETURNCMD: 使 TrackPopupMenu 返回选择的命令 ID
    // TPM_LEFTALIGN | TPM_TOPALIGN: 默认对齐方式
    // TPM_NONOTIFY: 不发送 WM_COMMAND,直接返回命令 ID
    UINT cmd = ::TrackPopupMenu(
        hMenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        x, y, 0, hWnd, nullptr);

    ::DestroyMenu(hMenu);

    if (cmd == 0) return -1;  // 用户未选择

    int selectedIndex = static_cast<int>(cmd) - 1;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size()))
    {
        const MenuConfigItem& selectedItem = items[selectedIndex];
        if (selectedItem.enabled && !selectedItem.isSeparator)
        {
            ExecuteAction(selectedItem);
        }
    }
    return selectedIndex;
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
