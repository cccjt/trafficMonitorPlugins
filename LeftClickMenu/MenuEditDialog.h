#pragma once
#include <afxdialogex.h>
#include "resource.h"
#include "MenuConfig.h"

// 单条菜单项编辑对话框
class CMenuEditDialog : public CDialogEx
{
    DECLARE_DYNAMIC(CMenuEditDialog)

public:
    CMenuEditDialog(const MenuConfigItem& item, CWnd* pParent = nullptr);
    virtual ~CMenuEditDialog();

    // 获取编辑后的结果
    const MenuConfigItem& GetResult() const { return m_result; }

    // 对话框 ID
    enum { IDD = IDD_MENU_EDIT_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    // 消息映射
    afx_msg void OnBnClickedBrowseIcon();
    afx_msg void OnBnClickedSeparator();
    afx_msg void OnCbnSelchangeIcon();
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();

    DECLARE_MESSAGE_MAP()

private:
    // 更新图标预览
    void UpdateIconPreview();

    // 更新控件启用状态(分隔线勾选时禁用名称/脚本)
    void UpdateControlStates();

    // 检查输入有效性
    bool ValidateInput();

private:
    MenuConfigItem m_result;     // 编辑结果

    // 控件
    CEdit m_editName;
    CEdit m_editScript;
    CButton m_chkEnabled;
    CButton m_chkSeparator;
    CComboBox m_comboIcon;
    CButton m_btnBrowseIcon;
    CStatic m_staticPreview;
};
