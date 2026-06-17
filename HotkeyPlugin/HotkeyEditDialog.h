#pragma once
#include <afxdialogex.h>
#include "resource.h"
#include "HotkeyConfig.h"

// 单条热键编辑对话框
class CHotkeyEditDialog : public CDialogEx
{
    DECLARE_DYNAMIC(CHotkeyEditDialog)

public:
    CHotkeyEditDialog(const HotkeyItem& item, CWnd* pParent = nullptr);
    virtual ~CHotkeyEditDialog();

    // 获取编辑后的结果
    const HotkeyItem& GetResult() const { return m_result; }

    // 对话框 ID
    enum { IDD = IDD_HOTKEY_EDIT_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

    // 消息映射
    afx_msg void OnBnClickedBrowseScript();
    afx_msg void OnBnClickedOk();
    afx_msg void OnBnClickedCancel();

    // 自定义热键输入框的按键处理
    afx_msg void OnHotkeyChange();

    DECLARE_MESSAGE_MAP()

private:
    // 更新热键显示
    void UpdateHotkeyDisplay();

    // 检查输入有效性
    bool ValidateInput();

private:
    HotkeyItem m_result;         // 编辑结果
    UINT m_modifiers;            // 当前修饰键
    UINT m_vk;                   // 当前虚拟键码

    // 控件
    CEdit m_editHotkey;
    CEdit m_editScript;
    CEdit m_editDescription;
    CButton m_chkEnabled;
};
