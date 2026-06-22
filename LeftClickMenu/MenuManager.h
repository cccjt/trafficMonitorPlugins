#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "MenuConfig.h"

// 菜单管理器:构建弹出菜单并执行 PowerShell 脚本
class MenuManager
{
public:
    MenuManager();
    ~MenuManager();

    // 显示弹出菜单
    // items: 菜单项配置列表
    // hWnd: 产生鼠标事件的窗口句柄
    // x, y: 菜单显示位置(屏幕坐标)
    // 返回选中的菜单项索引,未选择返回 -1
    int ShowPopupMenu(const std::vector<MenuConfigItem>& items, HWND hWnd, int x, int y);

    // 执行 PowerShell 脚本(异步)
    // item: 菜单项配置
    bool ExecuteAction(const MenuConfigItem& item);

private:
    // 查找可用的 PowerShell 可执行文件
    std::wstring FindPowerShellExe() const;

    // 将脚本代码写入临时 .ps1 文件并执行
    bool ExecuteScriptFile(const std::wstring& scriptCode);
};
