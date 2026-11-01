#include "stdafx.h"
#include "MenuEditDialog.h"
#include "MenuManager.h"
#include "IconManager.h"
#include "resource.h"
#include <afxdialogex.h>
#include <stdlib.h>
#include <cwchar>

IMPLEMENT_DYNAMIC(CMenuEditDialog, CDialogEx)

CMenuEditDialog::CMenuEditDialog(const MenuConfigItem& item, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD, pParent)
    , m_result(item)
{
}

CMenuEditDialog::~CMenuEditDialog()
{
}

void CMenuEditDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_NAME, m_editName);
    DDX_Control(pDX, IDC_EDIT_SCRIPT, m_editScript);
    DDX_Control(pDX, IDC_CHECK_ENABLED, m_chkEnabled);
    DDX_Control(pDX, IDC_CHECK_SEPARATOR, m_chkSeparator);
    DDX_Control(pDX, IDC_COMBO_ICON, m_comboIcon);
    DDX_Control(pDX, IDC_BTN_BROWSE_ICON, m_btnBrowseIcon);
    DDX_Control(pDX, IDC_STATIC_ICON_PREVIEW, m_staticPreview);
    DDX_Control(pDX, IDC_EDIT_ICON_INDEX, m_editIconIndex);
    DDX_Control(pDX, IDC_EDIT_HOTKEY, m_editHotkey);
    DDX_Control(pDX, IDC_BTN_CLEAR_HOTKEY, m_btnClearHotkey);
    DDX_Control(pDX, IDC_CHECK_SHOW_IN_MENU, m_chkShowInMenu);
    DDX_Control(pDX, IDC_BTN_TEST_RUN, m_btnTestRun);
}

BEGIN_MESSAGE_MAP(CMenuEditDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE_ICON, &CMenuEditDialog::OnBnClickedBrowseIcon)
    ON_BN_CLICKED(IDC_CHECK_SEPARATOR, &CMenuEditDialog::OnBnClickedSeparator)
    ON_CBN_SELCHANGE(IDC_COMBO_ICON, &CMenuEditDialog::OnCbnSelchangeIcon)
    ON_BN_CLICKED(IDC_BTN_CLEAR_HOTKEY, &CMenuEditDialog::OnBnClickedClearHotkey)
    ON_BN_CLICKED(IDC_BTN_TEST_RUN, &CMenuEditDialog::OnBnClickedTestRun)
    ON_BN_CLICKED(IDOK, &CMenuEditDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &CMenuEditDialog::OnBnClickedCancel)
END_MESSAGE_MAP()

BOOL CMenuEditDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 填充图标下拉框:第一项为"无",然后是内置图标,最后一项是自定义文件路径
    m_comboIcon.AddString(L"(无)");
    const auto& builtinNames = IconManager::Instance().GetBuiltinNames();
    for (const auto& name : builtinNames)
    {
        m_comboIcon.AddString(name.c_str());
    }

    // 设置初始值
    m_editName.SetWindowTextW(m_result.name.c_str());
    m_editScript.SetWindowTextW(m_result.scriptCode.c_str());
    m_chkEnabled.SetCheck(m_result.enabled ? BST_CHECKED : BST_UNCHECKED);
    m_chkSeparator.SetCheck(m_result.isSeparator ? BST_CHECKED : BST_UNCHECKED);
    m_chkShowInMenu.SetCheck(m_result.showInMenu ? BST_CHECKED : BST_UNCHECKED);
    m_hotkeyModifiers = m_result.hotkeyModifiers;
    m_hotkeyVk = m_result.hotkeyVk;
    UpdateHotkeyDisplay();

    CString iconIndexText;
    iconIndexText.Format(L"%d", m_result.iconIndex);
    m_editIconIndex.SetWindowTextW(iconIndexText);

    // 设置图标选择
    if (m_result.iconSource.empty())
    {
        m_comboIcon.SetCurSel(0);   // (无)
    }
    else
    {
        bool isBuiltin = false;
        for (size_t i = 0; i < builtinNames.size(); ++i)
        {
            if (m_result.iconSource == builtinNames[i])
            {
                m_comboIcon.SetCurSel(static_cast<int>(i + 1));
                isBuiltin = true;
                break;
            }
        }
        if (!isBuiltin)
        {
            // 自定义路径作为下拉框的最后一项
            m_comboIcon.AddString(m_result.iconSource.c_str());
            m_comboIcon.SetCurSel(static_cast<int>(builtinNames.size() + 1));
        }
    }

    UpdateControlStates();
    UpdateIconPreview();

    return TRUE;
}

void CMenuEditDialog::UpdateControlStates()
{
    // 分隔线勾选时禁用名称、脚本和图标设置
    BOOL isSeparator = (m_chkSeparator.GetCheck() == BST_CHECKED);

    m_editName.EnableWindow(!isSeparator);
    m_editScript.EnableWindow(!isSeparator);
    m_comboIcon.EnableWindow(!isSeparator);
    m_btnBrowseIcon.EnableWindow(!isSeparator);
    m_btnTestRun.EnableWindow(!isSeparator);

    // 没有设置热键的项无法从菜单之外触发,必须显示在菜单中
    BOOL hasHotkey = !isSeparator && m_hotkeyVk != 0;
    m_editHotkey.EnableWindow(!isSeparator);
    m_btnClearHotkey.EnableWindow(hasHotkey);
    m_chkShowInMenu.EnableWindow(hasHotkey);
    if (!hasHotkey)
    {
        m_chkShowInMenu.SetCheck(BST_CHECKED);
    }

    // 只有自定义文件图标才需要填写图标序号
    int sel = m_comboIcon.GetCurSel();
    int builtinCount = static_cast<int>(IconManager::Instance().GetBuiltinNames().size());
    BOOL isCustom = !isSeparator && sel > builtinCount;
    m_editIconIndex.EnableWindow(isCustom);
}

void CMenuEditDialog::UpdateHotkeyDisplay()
{
    m_editHotkey.SetWindowTextW(MenuConfigItem::HotkeyToString(m_hotkeyModifiers, m_hotkeyVk).c_str());
}

BOOL CMenuEditDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN)
    {
        // 焦点在热键输入框时捕获组合键
        CWnd* focus = GetFocus();
        if (focus == &m_editHotkey)
        {
            UINT vk = pMsg->wParam;

            // 忽略单独的修饰键
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT
                || vk == VK_LWIN || vk == VK_RWIN)
            {
                return TRUE;
            }

            // Esc 关闭对话框,回车确认对话框
            if (vk == VK_ESCAPE || vk == VK_RETURN)
            {
                return CDialogEx::PreTranslateMessage(pMsg);
            }

            // 捕获当前修饰键状态
            m_hotkeyModifiers = 0;
            if (::GetKeyState(VK_CONTROL) & 0x8000) m_hotkeyModifiers |= MOD_CONTROL;
            if (::GetKeyState(VK_MENU) & 0x8000) m_hotkeyModifiers |= MOD_ALT;
            if (::GetKeyState(VK_SHIFT) & 0x8000) m_hotkeyModifiers |= MOD_SHIFT;
            if ((::GetKeyState(VK_LWIN) & 0x8000) || (::GetKeyState(VK_RWIN) & 0x8000))
            {
                m_hotkeyModifiers |= MOD_WIN;
            }

            m_hotkeyVk = vk;
            UpdateHotkeyDisplay();
            UpdateControlStates();
            return TRUE;
        }
    }
    return CDialogEx::PreTranslateMessage(pMsg);
}

void CMenuEditDialog::OnBnClickedClearHotkey()
{
    m_hotkeyModifiers = 0;
    m_hotkeyVk = 0;
    UpdateHotkeyDisplay();
    UpdateControlStates();
}

int CMenuEditDialog::GetSelectedIconIndex() const
{
    CString text;
    m_editIconIndex.GetWindowTextW(text);
    text.Trim();
    if (text.IsEmpty()) return 0;

    int value = _wtoi(static_cast<LPCWSTR>(text));
    return value < 0 ? 0 : value;
}

void CMenuEditDialog::UpdateIconPreview()
{
    int sel = m_comboIcon.GetCurSel();
    int builtinCount = static_cast<int>(IconManager::Instance().GetBuiltinNames().size());

    if (sel <= 0 || sel == CB_ERR)
    {
        // 无图标
        m_staticPreview.SetIcon(nullptr);
        return;
    }

    CString iconSource;
    int iconIndex = 0;
    if (sel <= builtinCount)
    {
        iconSource = IconManager::Instance().GetBuiltinNames()[sel - 1].c_str();
    }
    else
    {
        // 自定义路径
        m_comboIcon.GetLBText(sel, iconSource);
        iconIndex = GetSelectedIconIndex();
    }

    if (iconSource.IsEmpty())
    {
        m_staticPreview.SetIcon(nullptr);
        return;
    }

    // 加载图标并显示预览(图标由 IconManager 缓存,不能在这里销毁)
    HICON hIcon = IconManager::Instance().GetIcon(iconSource.GetString(), iconIndex);
    m_staticPreview.SetIcon(hIcon != nullptr ? hIcon : nullptr);
}

void CMenuEditDialog::OnBnClickedBrowseIcon()
{
    // 打开文件选择对话框
    CFileDialog dlg(TRUE, nullptr, nullptr,
        OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
        L"图标文件 (*.ico;*.exe;*.dll)|*.ico;*.exe;*.dll|所有文件 (*.*)|*.*||", this);

    if (dlg.DoModal() == IDOK)
    {
        CString path = dlg.GetPathName();
        int builtinCount = static_cast<int>(IconManager::Instance().GetBuiltinNames().size());

        // 自定义路径固定放在下拉框最后一项,替换掉旧的自定义项
        while (m_comboIcon.GetCount() > builtinCount + 1)
        {
            m_comboIcon.DeleteString(builtinCount + 1);
        }
        m_comboIcon.AddString(path);
        m_comboIcon.SetCurSel(builtinCount + 1);

        UpdateControlStates();
        UpdateIconPreview();
    }
}

void CMenuEditDialog::OnBnClickedSeparator()
{
    UpdateControlStates();
}

void CMenuEditDialog::OnCbnSelchangeIcon()
{
    UpdateControlStates();
    UpdateIconPreview();
}

bool CMenuEditDialog::ValidateInput()
{
    // 分隔线不需要验证名称和脚本
    BOOL isSeparator = (m_chkSeparator.GetCheck() == BST_CHECKED);
    if (!isSeparator)
    {
        CString name;
        m_editName.GetWindowTextW(name);
        if (name.IsEmpty())
        {
            MessageBoxW(L"请输入菜单项名称", L"提示", MB_OK | MB_ICONWARNING);
            return false;
        }

        CString script;
        m_editScript.GetWindowTextW(script);
        if (script.IsEmpty())
        {
            MessageBoxW(L"请输入 PowerShell 脚本代码", L"提示", MB_OK | MB_ICONWARNING);
            return false;
        }

        // 自定义文件图标时检查序号格式
        int sel = m_comboIcon.GetCurSel();
        int builtinCount = static_cast<int>(IconManager::Instance().GetBuiltinNames().size());
        if (sel > builtinCount)
        {
            CString indexText;
            m_editIconIndex.GetWindowTextW(indexText);
            indexText.Trim();
            if (!indexText.IsEmpty())
            {
                for (int i = 0; i < indexText.GetLength(); ++i)
                {
                    if (indexText[i] < L'0' || indexText[i] > L'9')
                    {
                        MessageBoxW(L"图标序号必须是非负整数", L"提示", MB_OK | MB_ICONWARNING);
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void CMenuEditDialog::OnBnClickedTestRun()
{
    CString script;
    m_editScript.GetWindowTextW(script);
    if (script.IsEmpty())
    {
        MessageBoxW(L"请先输入 PowerShell 脚本代码", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    MenuManager manager;
    if (!manager.RunScript(script.GetString()))
    {
        MessageBoxW(L"脚本启动失败,请查看插件日志", L"试运行", MB_OK | MB_ICONWARNING);
    }
}

void CMenuEditDialog::OnBnClickedOk()
{
    if (!ValidateInput())
    {
        return;
    }

    // 分隔线
    m_result.isSeparator = (m_chkSeparator.GetCheck() == BST_CHECKED);
    m_result.enabled = (m_chkEnabled.GetCheck() == BST_CHECKED);

    if (!m_result.isSeparator)
    {
        CString tmp;
        m_editName.GetWindowTextW(tmp);
        m_result.name = tmp;

        m_editScript.GetWindowTextW(tmp);
        m_result.scriptCode = tmp;

        // 热键:未设置时清空修饰键,并强制显示在左键菜单中
        if (m_hotkeyVk != 0)
        {
            m_result.hotkeyModifiers = m_hotkeyModifiers;
            m_result.hotkeyVk = m_hotkeyVk;
            m_result.showInMenu = (m_chkShowInMenu.GetCheck() == BST_CHECKED);
        }
        else
        {
            m_result.hotkeyModifiers = 0;
            m_result.hotkeyVk = 0;
            m_result.showInMenu = true;
        }

        // 图标来源
        int sel = m_comboIcon.GetCurSel();
        int builtinCount = static_cast<int>(IconManager::Instance().GetBuiltinNames().size());
        if (sel <= 0 || sel == CB_ERR)
        {
            m_result.iconSource.clear();
            m_result.iconIndex = 0;
        }
        else if (sel <= builtinCount)
        {
            m_result.iconSource = IconManager::Instance().GetBuiltinNames()[sel - 1];
            m_result.iconIndex = 0;
        }
        else
        {
            CString path;
            m_comboIcon.GetLBText(sel, path);
            m_result.iconSource = path.GetString();
            m_result.iconIndex = GetSelectedIconIndex();
        }
    }
    else
    {
        // 分隔线清空其他字段
        m_result.name.clear();
        m_result.scriptCode.clear();
        m_result.iconSource.clear();
        m_result.iconIndex = 0;
        m_result.hotkeyModifiers = 0;
        m_result.hotkeyVk = 0;
        m_result.showInMenu = true;
    }

    CDialogEx::OnOK();
}

void CMenuEditDialog::OnBnClickedCancel()
{
    CDialogEx::OnCancel();
}
