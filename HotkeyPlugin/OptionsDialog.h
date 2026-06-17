#pragma once
#include <afxdialogex.h>
#include "HotkeyConfig.h"
#include <vector>

// 主配置对话框:管理热键列表
class COptionsDialog : public CDialogEx
{
    DECLARE_DYNAMIC(COptionsDialog)

public:
    COptionsDialog(HotkeyConfig& config, CWnd* pParent = nullptr);
    virtual ~COptionsDialog();

    // 对话框 ID
    enum { IDD = IDD_OPTIONS_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    // 消息映射
    afx_msg void OnBnClickedAdd();
    afx_msg void OnBnClickedEdit();
    afx_msg void OnBnClickedDelete();
    afx_msg void OnBnClickedToggle();
    afx_msg void OnNMDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();

    DECLARE_MESSAGE_MAP()

private:
    // 刷新列表显示
    void RefreshList();

    // 获取选中的索引,无选中返回 -1
    int GetSelectedIndex() const;

private:
    HotkeyConfig& m_config;             // 配置引用(由调用方持有)
    std::vector<HotkeyItem> m_items;    // 本地编辑副本

    // 控件
    CListCtrl m_list;
};
