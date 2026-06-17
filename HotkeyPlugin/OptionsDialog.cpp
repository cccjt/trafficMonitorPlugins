#include "stdafx.h"
#include "OptionsDialog.h"
#include "HotkeyEditDialog.h"
#include "resource.h"
#include <afxdialogex.h>

IMPLEMENT_DYNAMIC(COptionsDialog, CDialogEx)

COptionsDialog::COptionsDialog(HotkeyConfig& config, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD, pParent)
    , m_config(config)
{
    // 复制一份配置用于编辑
    m_items = config.GetItems();
}

COptionsDialog::~COptionsDialog()
{
}

void COptionsDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_HOTKEYS, m_list);
}

BEGIN_MESSAGE_MAP(COptionsDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_ADD, &COptionsDialog::OnBnClickedAdd)
    ON_BN_CLICKED(IDC_BTN_EDIT, &COptionsDialog::OnBnClickedEdit)
    ON_BN_CLICKED(IDC_BTN_DELETE, &COptionsDialog::OnBnClickedDelete)
    ON_BN_CLICKED(IDC_BTN_TOGGLE, &COptionsDialog::OnBnClickedToggle)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_HOTKEYS, &COptionsDialog::OnNMDblclkList)
    ON_BN_CLICKED(IDOK, &COptionsDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &COptionsDialog::OnBnClickedCancel)
END_MESSAGE_MAP()

BOOL COptionsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置列表扩展样式(整行选中、网格线)
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // 添加列
    m_list.InsertColumn(0, L"热键", LVCFMT_LEFT, 120);
    m_list.InsertColumn(1, L"脚本路径", LVCFMT_LEFT, 280);
    m_list.InsertColumn(2, L"状态", LVCFMT_CENTER, 60);
    m_list.InsertColumn(3, L"描述", LVCFMT_LEFT, 150);

    RefreshList();

    return TRUE;
}

void COptionsDialog::RefreshList()
{
    m_list.DeleteAllItems();

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        const HotkeyConfigItem& item = m_items[i];
        std::wstring hotkey = item.ToHotkeyString();

        int idx = m_list.InsertItem(static_cast<int>(i), hotkey.c_str());
        m_list.SetItemText(idx, 1, item.scriptPath.c_str());
        m_list.SetItemText(idx, 2, item.enabled ? L"启用" : L"禁用");
        m_list.SetItemText(idx, 3, item.description.c_str());
    }
}

int COptionsDialog::GetSelectedIndex() const
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (pos == nullptr) return -1;
    return m_list.GetNextSelectedItem(pos);
}

void COptionsDialog::OnBnClickedAdd()
{
    HotkeyConfigItem newItem;
    newItem.enabled = true;

    CHotkeyEditDialog dlg(newItem, this);
    if (dlg.DoModal() == IDOK)
    {
        m_items.push_back(dlg.GetResult());
        RefreshList();
        // 选中新添加的项
        m_list.SetItemState(static_cast<int>(m_items.size() - 1),
            LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void COptionsDialog::OnBnClickedEdit()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_items.size()))
    {
        MessageBoxW(L"请先选择一个热键项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    CHotkeyEditDialog dlg(m_items[sel], this);
    if (dlg.DoModal() == IDOK)
    {
        m_items[sel] = dlg.GetResult();
        RefreshList();
        m_list.SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
}

void COptionsDialog::OnBnClickedDelete()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_items.size()))
    {
        MessageBoxW(L"请先选择一个热键项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxW(L"确定要删除选中的热键吗?", L"确认",
        MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        m_items.erase(m_items.begin() + sel);
        RefreshList();
    }
}

void COptionsDialog::OnBnClickedToggle()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_items.size()))
    {
        MessageBoxW(L"请先选择一个热键项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_items[sel].enabled = !m_items[sel].enabled;
    RefreshList();
    m_list.SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void COptionsDialog::OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    OnBnClickedEdit();
}

void COptionsDialog::OnBnClickedOk()
{
    // 将编辑结果写回配置
    m_config.GetItems() = m_items;
    CDialogEx::OnOK();
}

void COptionsDialog::OnBnClickedCancel()
{
    CDialogEx::OnCancel();
}
