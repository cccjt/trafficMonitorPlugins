#include "stdafx.h"
#include "MenuEditDialog.h"
#include "IconManager.h"
#include "resource.h"
#include <afxdialogex.h>

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
}

BEGIN_MESSAGE_MAP(CMenuEditDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE_ICON, &CMenuEditDialog::OnBnClickedBrowseIcon)
    ON_BN_CLICKED(IDC_CHECK_SEPARATOR, &CMenuEditDialog::OnBnClickedSeparator)
    ON_CBN_SELCHANGE(IDC_COMBO_ICON, &CMenuEditDialog::OnCbnSelchangeIcon)
    ON_BN_CLICKED(IDOK, &CMenuEditDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &CMenuEditDialog::OnBnClickedCancel)
END_MESSAGE_MAP()

BOOL CMenuEditDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 填充图标下拉框:第一项为"无",然后是内置图标,最后是"(自定义路径)"
    m_comboIcon.AddString(L"(无)");
    const auto& builtinNames = IconManager::Instance().GetBuiltinNames();
    for (const auto& name : builtinNames)
    {
        m_comboIcon.AddString(name.c_str());
    }
    m_comboIcon.AddString(L"(自定义路径)");

    // 设置初始值
    m_editName.SetWindowTextW(m_result.name.c_str());
    m_editScript.SetWindowTextW(m_result.scriptCode.c_str());
    m_chkEnabled.SetCheck(m_result.enabled ? BST_CHECKED : BST_UNCHECKED);
    m_chkSeparator.SetCheck(m_result.isSeparator ? BST_CHECKED : BST_UNCHECKED);

    // 设置图标选择
    if (m_result.iconSource.empty())
    {
        m_comboIcon.SetCurSel(0);   // (无)
    }
    else
    {
        // 检查是否为内置名称
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
            // 自定义路径,显示在编辑框中
            m_comboIcon.SetCurSel(static_cast<int>(builtinNames.size() + 1));
            m_comboIcon.SetWindowTextW(m_result.iconSource.c_str());
        }
    }

    UpdateControlStates();
    UpdateIconPreview();

    return TRUE;
}

void CMenuEditDialog::UpdateControlStates()
{
    // 分隔线勾选时禁用名称和脚本编辑
    BOOL isSeparator = (m_chkSeparator.GetCheck() == BST_CHECKED);
    m_editName.EnableWindow(!isSeparator);
    m_editScript.EnableWindow(!isSeparator);
    m_comboIcon.EnableWindow(!isSeparator);
    m_btnBrowseIcon.EnableWindow(!isSeparator);
}

void CMenuEditDialog::UpdateIconPreview()
{
    // 获取当前图标源
    CString iconSource;
    int sel = m_comboIcon.GetCurSel();

    const auto& builtinNames = IconManager::Instance().GetBuiltinNames();
    if (sel == 0 || sel == CB_ERR)
    {
        // 无图标
        m_staticPreview.SetIcon(nullptr);
        return;
    }
    else if (sel <= static_cast<int>(builtinNames.size()))
    {
        iconSource = builtinNames[sel - 1].c_str();
    }
    else
    {
        // 自定义路径
        m_comboIcon.GetWindowTextW(iconSource);
    }

    if (iconSource.IsEmpty())
    {
        m_staticPreview.SetIcon(nullptr);
        return;
    }

    // 加载图标并显示预览
    HICON hIcon = IconManager::Instance().GetIcon(iconSource.GetString(), m_result.iconIndex);
    if (hIcon)
    {
        m_staticPreview.SetIcon(hIcon);
    }
    else
    {
        m_staticPreview.SetIcon(nullptr);
    }
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
        // 选择"(自定义路径)"项并显示路径
        const auto& builtinNames = IconManager::Instance().GetBuiltinNames();
        m_comboIcon.SetCurSel(static_cast<int>(builtinNames.size() + 1));
        m_comboIcon.SetWindowTextW(path);
        m_result.iconSource = path.GetString();
        UpdateIconPreview();
    }
}

void CMenuEditDialog::OnBnClickedSeparator()
{
    UpdateControlStates();
}

void CMenuEditDialog::OnCbnSelchangeIcon()
{
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
    }
    return true;
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

        // 图标源
        int sel = m_comboIcon.GetCurSel();
        const auto& builtinNames = IconManager::Instance().GetBuiltinNames();
        if (sel == 0 || sel == CB_ERR)
        {
            m_result.iconSource.clear();
        }
        else if (sel <= static_cast<int>(builtinNames.size()))
        {
            m_result.iconSource = builtinNames[sel - 1];
        }
        else
        {
            // 自定义路径
            CString path;
            m_comboIcon.GetWindowTextW(path);
            m_result.iconSource = path.GetString();
        }
    }
    else
    {
        // 分隔线清空其他字段
        m_result.name.clear();
        m_result.scriptCode.clear();
        m_result.iconSource.clear();
    }

    CDialogEx::OnOK();
}

void CMenuEditDialog::OnBnClickedCancel()
{
    CDialogEx::OnCancel();
}
