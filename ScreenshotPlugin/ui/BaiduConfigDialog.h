#pragma once
#include "../res/resource.h"
#include "../config/ScreenshotConfig.h"

// 百度翻译配置对话框
class CBaiduConfigDialog : public CDialogEx
{
    DECLARE_DYNAMIC(CBaiduConfigDialog)

public:
    explicit CBaiduConfigDialog(CWnd* pParent = nullptr);
    virtual ~CBaiduConfigDialog();

    enum { IDD = IDD_BAIDU_CONFIG_DIALOG };

    void SetConfig(const BaiduConfig& cfg) { m_cfg = cfg; }
    const BaiduConfig& GetConfig() const { return m_cfg; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    afx_msg void OnBtnApplyUrl();
    afx_msg void OnBtnTest();

    virtual void OnOK() override;

    DECLARE_MESSAGE_MAP()

private:
    BaiduConfig m_cfg;
};
