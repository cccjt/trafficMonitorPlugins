#include "stdafx.h"
#include "MenuItem.h"
#include "MenuPlugin.h"
#include "MenuConfig.h"
#include <Windows.h>

MenuItem::MenuItem() {}
MenuItem::~MenuItem() {}

const wchar_t* MenuItem::GetItemName() const
{
    return L"快捷菜单";
}

const wchar_t* MenuItem::GetItemId() const
{
    return L"LeftClickMenu";
}

const wchar_t* MenuItem::GetItemLableText() const
{
    return L"Menu";
}

const wchar_t* MenuItem::GetItemValueText() const
{
    if (m_owner != nullptr)
    {
        m_valueText = m_owner->GetDisplayText();
    }
    else
    {
        m_valueText = L"0";
    }
    return m_valueText.c_str();
}

const wchar_t* MenuItem::GetItemValueSampleText() const
{
    return L"00";
}

int MenuItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    // 左键点击:弹出菜单
    if (type == MT_LCLICKED && m_owner != nullptr)
    {
        // 检查菜单是否为空
        const auto& items = m_owner->GetConfig().GetItems();
        if (items.empty())
        {
            ::MessageBoxW(
                static_cast<HWND>(hWnd),
                L"菜单项为空,请先在插件设置中添加菜单项。",
                L"快捷菜单",
                MB_OK | MB_ICONINFORMATION);
            return 1;   // 拦截默认行为
        }

        m_owner->ShowMenuFromItem(hWnd, x, y);
        return 1;   // 拦截默认行为
    }
    // 右键点击:不拦截,使用主程序默认右键菜单
    return 0;
}
