#include "../pch/stdafx.h"
#include "../res/resource.h"
#include "OptionsDialog.h"
#include "HotkeyEditDialog.h"
#include "BaiduConfigDialog.h"
#include <vector>

IMPLEMENT_DYNAMIC(COptionsDialog, CDialogEx)

COptionsDialog::COptionsDialog(CWnd* pParent)
    : CDialogEx(IDD_OPTIONS_DIALOG, pParent)
{
    // 复制当前配置
    auto& cfg = ScreenshotPlugin::Instance().GetConfig();
    m_hotkeys = cfg.GetAllHotkeys();
    m_baidu = cfg.GetBaidu();
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
    ON_BN_CLICKED(IDC_BTN_EDIT, &COptionsDialog::OnBtnEdit)
    ON_BN_CLICKED(IDC_BTN_RESET, &COptionsDialog::OnBtnReset)
    ON_BN_CLICKED(IDC_BTN_TOGGLE, &COptionsDialog::OnBtnToggle)
    ON_BN_CLICKED(IDC_BTN_BAIDU, &COptionsDialog::OnBtnBaidu)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_HOTKEYS, &COptionsDialog::OnListDblClk)
END_MESSAGE_MAP()

BOOL COptionsDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    InitList();
    RefreshList();
    return TRUE;
}

void COptionsDialog::InitList()
{
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    m_list.InsertColumn(0, L"功能", LVCFMT_LEFT, 160);
    m_list.InsertColumn(1, L"热键", LVCFMT_LEFT, 160);
    m_list.InsertColumn(2, L"状态", LVCFMT_CENTER, 70);
    m_list.InsertColumn(3, L"默认值", LVCFMT_LEFT, 120);
}

void COptionsDialog::RefreshList()
{
    m_list.DeleteAllItems();

    static const ActionType allActions[ACTION_COUNT] = {
        ActionType::Capture, ActionType::Pin, ActionType::Ocr, ActionType::Translate,
        ActionType::Save, ActionType::QrCode,
        ActionType::ClipboardOcr, ActionType::ClipboardTranslate
    };

    for (int i = 0; i < ACTION_COUNT; ++i)
    {
        ActionType a = allActions[i];
        const ScreenshotHotkeyItem& item = m_hotkeys[a];
        const DefaultHotkey& def = GetDefaultHotkey(a);

        ScreenshotHotkeyItem defItem;
        defItem.modifiers = def.modifiers;
        defItem.vk = def.vk;
        defItem.enabled = def.enabled;

        CString name = GetActionName(a);
        CString hotkey = item.ToHotkeyString().c_str();
        CString status = item.enabled ? L"启用" : L"禁用";
        CString defStr = defItem.ToHotkeyString().c_str();

        m_list.InsertItem(i, name);
        m_list.SetItemText(i, 1, hotkey);
        m_list.SetItemText(i, 2, status);
        m_list.SetItemText(i, 3, defStr);
        m_list.SetItemData(i, static_cast<DWORD_PTR>(i));
    }
}

bool COptionsDialog::GetSelectedAction(ActionType& out) const
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (pos == nullptr) return false;
    int idx = m_list.GetNextSelectedItem(pos);
    if (idx < 0 || idx >= ACTION_COUNT) return false;
    out = static_cast<ActionType>(idx);
    return true;
}

void COptionsDialog::SetSelectedAction(ActionType a)
{
    int idx = static_cast<int>(a);
    m_list.SetItemState(idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void COptionsDialog::OnBtnEdit()
{
    ActionType a;
    if (!GetSelectedAction(a))
    {
        AfxMessageBox(L"请先选择一个功能", MB_ICONINFORMATION);
        return;
    }

    CHotkeyEditDialog dlg(this);
    dlg.SetAction(a);
    dlg.SetHotkey(m_hotkeys[a]);
    if (dlg.DoModal() == IDOK)
    {
        m_hotkeys[a] = dlg.GetHotkey();
        RefreshList();
    }
}

void COptionsDialog::OnBtnReset()
{
    ActionType a;
    if (!GetSelectedAction(a))
    {
        AfxMessageBox(L"请先选择一个功能", MB_ICONINFORMATION);
        return;
    }

    const DefaultHotkey& def = GetDefaultHotkey(a);
    ScreenshotHotkeyItem item;
    item.modifiers = def.modifiers;
    item.vk = def.vk;
    item.enabled = def.enabled;
    m_hotkeys[a] = item;
    RefreshList();
}

void COptionsDialog::OnBtnToggle()
{
    ActionType a;
    if (!GetSelectedAction(a))
    {
        AfxMessageBox(L"请先选择一个功能", MB_ICONINFORMATION);
        return;
    }
    m_hotkeys[a].enabled = !m_hotkeys[a].enabled;
    RefreshList();
}

void COptionsDialog::OnBtnBaidu()
{
    CBaiduConfigDialog dlg(this);
    dlg.SetConfig(m_baidu);
    if (dlg.DoModal() == IDOK)
    {
        m_baidu = dlg.GetConfig();
    }
}

void COptionsDialog::OnListDblClk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    OnBtnEdit();
    *pResult = 0;
}

void COptionsDialog::OnOK()
{
    // 保存配置到插件
    auto& cfg = ScreenshotPlugin::Instance().GetConfig();
    for (const auto& kv : m_hotkeys)
    {
        cfg.SetHotkey(kv.first, kv.second);
    }
    cfg.SetBaidu(m_baidu);
    ScreenshotPlugin::Instance().SaveConfig();
    ScreenshotPlugin::Instance().ApplyHotkeys();

    CDialogEx::OnOK();
}
