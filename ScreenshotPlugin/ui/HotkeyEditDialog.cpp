#include "../pch/stdafx.h"
#include "../res/resource.h"
#include "HotkeyEditDialog.h"

IMPLEMENT_DYNAMIC(CHotkeyEditDialog, CDialogEx)

CHotkeyEditDialog::CHotkeyEditDialog(CWnd* pParent)
    : CDialogEx(IDD_HOTKEY_EDIT_DIALOG, pParent)
{
}

CHotkeyEditDialog::~CHotkeyEditDialog()
{
}

void CHotkeyEditDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CHotkeyEditDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_CLEAR, &CHotkeyEditDialog::OnBtnClear)
END_MESSAGE_MAP()

void CHotkeyEditDialog::SetAction(ActionType a)
{
    m_action = a;
}

void CHotkeyEditDialog::SetHotkey(const ScreenshotHotkeyItem& item)
{
    m_initial = item;
    m_result = item;
}

BOOL CHotkeyEditDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 显示功能名称
    CString funcName = GetActionName(m_action);
    SetDlgItemTextW(IDC_STATIC_FUNC, funcName);

    // 显示初始热键
    UpdateHotkeyDisplay();

    // 设置复选框
    CheckDlgButton(IDC_CHECK_ENABLED, m_result.enabled ? BST_CHECKED : BST_UNCHECKED);

    return TRUE;
}

void CHotkeyEditDialog::UpdateHotkeyDisplay()
{
    CString s = m_result.ToHotkeyString().c_str();
    SetDlgItemTextW(IDC_EDIT_HOTKEY, s);
}

void CHotkeyEditDialog::OnBtnClear()
{
    m_result.vk = 0;
    m_result.modifiers = 0;
    m_captured = true;
    UpdateHotkeyDisplay();
}

bool CHotkeyEditDialog::CaptureKeyEvent(UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    // 只处理 KEYDOWN / SYSKEYDOWN
    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN)
        return false;

    // 忽略单独的修饰键,但记录状态
    UINT vk = static_cast<UINT>(wParam);
    if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN)
        return false;   // 让 PreTranslate 继续走

    // ESC 取消捕获
    if (vk == VK_ESCAPE)
    {
        return false;
    }

    // 必须有至少一个修饰键
    UINT modifiers = 0;
    if (::GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (::GetKeyState(VK_MENU) & 0x8000)    modifiers |= MOD_ALT;
    if (::GetKeyState(VK_SHIFT) & 0x8000)   modifiers |= MOD_SHIFT;
    if ((::GetKeyState(VK_LWIN) & 0x8000) || (::GetKeyState(VK_RWIN) & 0x8000))
        modifiers |= MOD_WIN;

    if (modifiers == 0)
    {
        // 也允许 F1-F12 不带修饰键
        if (vk < VK_F1 || vk > VK_F12)
        {
            return false;
        }
    }

    m_result.modifiers = modifiers;
    m_result.vk = vk;
    m_captured = true;
    UpdateHotkeyDisplay();
    return true;    // 消费此消息
}

BOOL CHotkeyEditDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
    {
        if (CaptureKeyEvent(pMsg->message, pMsg->wParam, pMsg->lParam))
        {
            return TRUE;
        }
    }

    // 拦截回车和 ESC,避免默认按钮处理(用户可能正在捕获)
    if (pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_RETURN)
        {
            // 没有 vk 时不允许 OK
            if (m_result.vk == 0)
            {
                return TRUE;
            }
            // 否则走默认 OnOK
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

void CHotkeyEditDialog::OnOK()
{
    // 保存"启用"复选框状态
    m_result.enabled = (IsDlgButtonChecked(IDC_CHECK_ENABLED) == BST_CHECKED);

    // 如果没有设置热键,自动禁用
    if (m_result.vk == 0)
    {
        m_result.enabled = false;
    }

    CDialogEx::OnOK();
}
