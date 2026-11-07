#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "MenuConfig.h"
#include "MenuPanel.h"

// 菜单管理器:构建弹出菜单并执行 PowerShell 脚本
class MenuManager
{
public:
    MenuManager();
    ~MenuManager();

    // 显示自绘菜单面板
    // items: 菜单项配置列表
    // hWnd: 面板的 owner 窗口(任务栏窗口或主窗口)
    // x, y: 弹出位置(屏幕坐标)
    // viewOptions: 面板宽度、列表/平铺模式与每行列数
    // viewModeChanged: 非空时返回用户是否切换了显示模式
    // tileMode: 非空时返回切换后的显示模式
    // 返回选中的菜单项索引,未选择返回 -1
    int ShowPopupMenu(const std::vector<MenuConfigItem>& items, HWND hWnd, int x, int y,
        const MenuViewOptions& viewOptions, bool* viewModeChanged = nullptr,
        bool* tileMode = nullptr);

    // 执行 PowerShell 脚本(异步)
    // item: 菜单项配置
    bool ExecuteAction(const MenuConfigItem& item);

    // 直接执行一段脚本代码(供配置对话框的"试运行"使用)
    bool RunScript(const std::wstring& scriptCode);

private:
    // 查找可用的 PowerShell 可执行文件
    std::wstring FindPowerShellExe() const;

    // 将脚本代码写入临时 .ps1 文件并执行
    bool ExecuteScriptFile(const std::wstring& scriptCode);
};
