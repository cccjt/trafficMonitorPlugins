#pragma once
#include <map>
#include "../res/resource.h"
#include "../core/ScreenshotPlugin.h"
#include "../config/ActionType.h"

// 截图插件主配置对话框
// 列表显示 8 个功能及其热键,支持修改/恢复默认/启用切换
class COptionsDialog : public CDialogEx
{
    DECLARE_DYNAMIC(COptionsDialog)

public:
    explicit COptionsDialog(CWnd* pParent = nullptr);
    virtual ~COptionsDialog();

    enum { IDD = IDD_OPTIONS_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    // 点击"修改热键"
    afx_msg void OnBtnEdit();
    // 点击"恢复默认"
    afx_msg void OnBtnReset();
    // 点击"启用/禁用"
    afx_msg void OnBtnToggle();
    // 点击"百度翻译设置"
    afx_msg void OnBtnBaidu();
    // 列表双击
    afx_msg void OnListDblClk(NMHDR* pNMHDR, LRESULT* pResult);
    // 确定
    virtual void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    // 初始化列表控件
    void InitList();
    // 刷新列表显示
    void RefreshList();
    // 获取当前选中的功能(无选中返回 false)
    bool GetSelectedAction(ActionType& out) const;
    // 设置当前选中的功能
    void SetSelectedAction(ActionType a);

    CListCtrl m_list;
    // 临时保存对话框编辑期间的配置(确认时写回 ScreenshotPlugin)
    std::map<ActionType, ScreenshotHotkeyItem> m_hotkeys;
    BaiduConfig m_baidu;
};
