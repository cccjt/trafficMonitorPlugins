#include "stdafx.h"
#include "OptionsDialog.h"
#include "MenuEditDialog.h"
#include "resource.h"
#include <afxdialogex.h>

IMPLEMENT_DYNAMIC(COptionsDialog, CDialogEx)

COptionsDialog::COptionsDialog(MenuConfig& config, CWnd* pParent /*=nullptr*/)
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
    DDX_Control(pDX, IDC_LIST_MENU_ITEMS, m_list);
}

BEGIN_MESSAGE_MAP(COptionsDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_ADD, &COptionsDialog::OnBnClickedAdd)
    ON_BN_CLICKED(IDC_BTN_EDIT, &COptionsDialog::OnBnClickedEdit)
    ON_BN_CLICKED(IDC_BTN_DELETE, &COptionsDialog::OnBnClickedDelete)
    ON_BN_CLICKED(IDC_BTN_UP, &COptionsDialog::OnBnClickedUp)
    ON_BN_CLICKED(IDC_BTN_DOWN, &COptionsDialog::OnBnClickedDown)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_MENU_ITEMS, &COptionsDialog::OnNMDblclkList)
    ON_BN_CLICKED(IDOK, &COptionsDialog::OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, &COptionsDialog::OnBnClickedCancel)
END_MESSAGE_MAP()

BOOL COptionsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 设置列表扩展样式(整行选中、网格线)
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // 添加列
    m_list.InsertColumn(0, L"名称", LVCFMT_LEFT, 150);
    m_list.InsertColumn(1, L"图标", LVCFMT_LEFT, 100);
    m_list.InsertColumn(2, L"脚本预览", LVCFMT_LEFT, 250);
    m_list.InsertColumn(3, L"状态", LVCFMT_CENTER, 60);

    RefreshList();

    return TRUE;
}

void COptionsDialog::RefreshList()
{
    m_list.DeleteAllItems();

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        const MenuConfigItem& item = m_items[i];

        CString name;
        CString iconSrc;
        CString scriptPreview;
        CString status;

        if (item.isSeparator)
        {
            name = L"--- 分隔线 ---";
            iconSrc = L"";
            scriptPreview = L"";
            status = L"";
        }
        else
        {
            name = item.name.c_str();
            iconSrc = item.iconSource.empty() ? L"(无)" : item.iconSource.c_str();

            // 脚本预览:只显示第一行并截断
            scriptPreview = item.scriptCode.c_str();
            int cr = scriptPreview.FindOneOf(L"\r\n");
            if (cr >= 0) scriptPreview = scriptPreview.Left(cr);
            if (scriptPreview.GetLength() > 60)
                scriptPreview = scriptPreview.Left(60) + L"...";

            status = item.enabled ? L"启用" : L"禁用";
        }

        int idx = m_list.InsertItem(static_cast<int>(i), name);
        m_list.SetItemText(idx, 1, iconSrc);
        m_list.SetItemText(idx, 2, scriptPreview);
        m_list.SetItemText(idx, 3, status);
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
    MenuConfigItem newItem;
    newItem.enabled = true;

    CMenuEditDialog dlg(newItem, this);
    if (dlg.DoModal() == IDOK)
    {
        m_items.push_back(dlg.GetResult());
        RefreshList();
        // 选中新添加的项
        int newIndex = static_cast<int>(m_items.size() - 1);
        m_list.SetItemState(newIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.EnsureVisible(newIndex, FALSE);
    }
}

void COptionsDialog::OnBnClickedEdit()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_items.size()))
    {
        MessageBoxW(L"请先选择一个菜单项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    CMenuEditDialog dlg(m_items[sel], this);
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
        MessageBoxW(L"请先选择一个菜单项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (MessageBoxW(L"确定要删除选中的菜单项吗?", L"确认",
        MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        m_items.erase(m_items.begin() + sel);
        RefreshList();
    }
}

void COptionsDialog::OnBnClickedUp()
{
    int sel = GetSelectedIndex();
    if (sel <= 0 || sel >= static_cast<int>(m_items.size()))
    {
        MessageBoxW(L"请先选择一个可上移的菜单项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::swap(m_items[sel], m_items[sel - 1]);
    RefreshList();
    m_list.SetItemState(sel - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void COptionsDialog::OnBnClickedDown()
{
    int sel = GetSelectedIndex();
    if (sel < 0 || sel >= static_cast<int>(m_items.size()) - 1)
    {
        MessageBoxW(L"请先选择一个可下移的菜单项", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::swap(m_items[sel], m_items[sel + 1]);
    RefreshList();
    m_list.SetItemState(sel + 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
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
    m_config.Save();
    CDialogEx::OnOK();
}

void COptionsDialog::OnBnClickedCancel()
{
    CDialogEx::OnCancel();
}
