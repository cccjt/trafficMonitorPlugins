#include "stdafx.h"
#include "HotkeyItem.h"
#include "HotkeyPlugin.h"

HotkeyItem::HotkeyItem() {}
HotkeyItem::~HotkeyItem() {}

const wchar_t* HotkeyItem::GetItemName() const
{
    return L"热键脚本";
}

const wchar_t* HotkeyItem::GetItemId() const
{
    return L"HotkeyScript";
}

const wchar_t* HotkeyItem::GetItemLableText() const
{
    return L"HK";
}

const wchar_t* HotkeyItem::GetItemValueText() const
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

const wchar_t* HotkeyItem::GetItemValueSampleText() const
{
    return L"00";
}

int HotkeyItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    // 左键点击:打开选项对话框
    if (type == MT_LCLICKED && m_owner != nullptr)
    {
        m_owner->ShowOptionsFromItem(hWnd);
        return 1;
    }
    return 0;
}
