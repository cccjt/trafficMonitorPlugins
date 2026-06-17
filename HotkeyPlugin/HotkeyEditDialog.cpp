#include "stdafx.h"
#include "HotkeyEditDialog.h"
#include "resource.h"
#include <afxdialogex.h>
#include <Windows.h>

CHotkeyEditDialog::CHotkeyEditDialog(const HotkeyConfigItem& item, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD, pParent)
    , m_result(item)
    , m_modifiers(item.modifiers)
    , m_vk(item.vk)
{
}

CHotkeyEditDialog::~CHotkeyEditDialog()
{
}

void CHotkeyEditDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_HOTKEY, m_editHotkey);
    DDX_Control(pDX, IDC_EDIT_SCRIPT, m_editScript);
    DDX_Control(pDX, IDC_EDIT_DESCRIPTION, m_editDescription);
    DDX_Control(pDX, IDC_CHECK_ENABLED, m_chkEnabled);
}

BEGIN_MESSAGE_MAP(CHotkeyEditDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE, &CHotkeyEditDialog::OnBnClickedBrowseScript)
    ON_BN_CLICKED(IDOK, &CHotkeyEditDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &CHotkeyEditDialog::OnBnClickedCancel)
    ON_EN_CHANGE(IDC_EDIT_HOTKEY, &CHotkeyEditDialog::OnHotkeyChange)
END_MESSAGE_MAP()

BOOL CHotkeyEditDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置初始值
    m_editScript.SetWindowTextW(m_result.scriptPath.c_str());
    m_editDescription.SetWindowTextW(m_result.description.c_str());
    m_chkEnabled.SetCheck(m_result.enabled ? BST_CHECKED : BST_UNCHECKED);

    UpdateHotkeyDisplay();

    return TRUE;
}

void CHotkeyEditDialog::UpdateHotkeyDisplay()
{
    std::wstring str;
    if (m_modifiers & MOD_CONTROL) str += L"Ctrl+";
    if (m_modifiers & MOD_ALT) str += L"Alt+";
    if (m_modifiers & MOD_SHIFT) str += L"Shift+";
    if (m_modifiers & MOD_WIN) str += L"Win+";

    if (m_vk != 0)
    {
        // 获取键名
        wchar_t keyName[64] = { 0 };
        UINT scanCode = MapVirtualKeyW(m_vk, MAPVK_VK_TO_VSC);
        LONG lParam = (scanCode << 16);
        if (GetKeyNameTextW(lParam, keyName, 64) != 0)
        {
            str += keyName;
        }
        else
        {
            str += L"?";
        }
    }

    // 避免触发 OnEnChange 循环
    m_editHotkey.SetWindowTextW(str.c_str());
}

// 自定义热键输入框:拦截按键消息
void CHotkeyEditDialog::OnHotkeyChange()
{
    // 用户直接编辑文本框时不解析,只允许通过按键设置
    // 实际按键捕获在 PreTranslateMessage 中处理
}

void CHotkeyEditDialog::OnBnClickedBrowseScript()
{
    CFileDialog dlg(TRUE, L"ps1", nullptr,
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
        L"PowerShell 脚本 (*.ps1)|*.ps1|所有文件 (*.*)|*.*||", this);

    if (dlg.DoModal() == IDOK)
    {
        m_editScript.SetWindowTextW(dlg.GetPathName());
    }
}

bool CHotkeyEditDialog::ValidateInput()
{
    if (m_vk == 0)
    {
        MessageBoxW(L"请按下热键组合键", L"提示", MB_OK | MB_ICONWARNING);
        return false;
    }

    CString scriptPath;
    m_editScript.GetWindowTextW(scriptPath);
    if (scriptPath.IsEmpty())
    {
        MessageBoxW(L"请输入或选择脚本路径", L"提示", MB_OK | MB_ICONWARNING);
        return false;
    }

    return true;
}

void CHotkeyEditDialog::OnBnClickedOk()
{
    if (!ValidateInput())
    {
        return;
    }

    m_result.modifiers = m_modifiers;
    m_result.vk = m_vk;

    CString tmp;
    m_editScript.GetWindowTextW(tmp);
    m_result.scriptPath = tmp;

    m_editDescription.GetWindowTextW(tmp);
    m_result.description = tmp;

    m_result.enabled = (m_chkEnabled.GetCheck() == BST_CHECKED);

    CDialogEx::OnOK();
}

void CHotkeyEditDialog::OnBnClickedCancel()
{
    CDialogEx::OnCancel();
}

// 拦截热键输入框的按键消息
BOOL CHotkeyEditDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
    {
        // 检查焦点是否在热键输入框
        CWnd* focus = GetFocus();
        if (focus == &m_editHotkey)
        {
            UINT vk = pMsg->wParam;

            // 忽略单独的修饰键
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN)
            {
                return TRUE; // 拦截,不传递
            }

            // 忽略一些不应该作为热键的按键
            if (vk == VK_ESCAPE)
            {
                // ESC 允许关闭对话框
                return CDialogEx::PreTranslateMessage(pMsg);
            }
            if (vk == VK_RETURN)
            {
                // 回车默认点击 OK
                return CDialogEx::PreTranslateMessage(pMsg);
            }

            // 捕获修饰键状态
            m_modifiers = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) m_modifiers |= MOD_CONTROL;
            if (GetKeyState(VK_MENU) & 0x8000) m_modifiers |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) m_modifiers |= MOD_SHIFT;
            if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) m_modifiers |= MOD_WIN;

            m_vk = vk;
            UpdateHotkeyDisplay();
            return TRUE; // 拦截
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}
