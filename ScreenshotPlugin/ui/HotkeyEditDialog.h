#pragma once
#include "../config/ActionType.h"

// 热键编辑对话框
// 用户在编辑框中按下热键组合,自动捕获
class CHotkeyEditDialog : public CDialogEx
{
    DECLARE_DYNAMIC(CHotkeyEditDialog)

public:
    explicit CHotkeyEditDialog(CWnd* pParent = nullptr);
    virtual ~CHotkeyEditDialog();

    enum { IDD = IDD_HOTKEY_EDIT_DIALOG };

    // 设置要编辑的功能(显示在顶部)
    void SetAction(ActionType a);
    // 设置初始热键
    void SetHotkey(const ScreenshotHotkeyItem& item);
    // 获取编辑后的热键
    const ScreenshotHotkeyItem& GetHotkey() const { return m_result; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    // 拦截键盘消息以捕获热键
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

    // 清除热键
    afx_msg void OnBtnClear();

    virtual void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    void UpdateHotkeyDisplay();
    bool CaptureKeyEvent(UINT message, WPARAM wParam, LPARAM lParam);

    ActionType m_action = ActionType::Capture;
    ScreenshotHotkeyItem m_initial;
    ScreenshotHotkeyItem m_result;

    bool m_captured = false;
};
