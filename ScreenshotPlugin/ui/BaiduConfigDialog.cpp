#include "../pch/stdafx.h"
#include "../res/resource.h"
#include "BaiduConfigDialog.h"
#include "../services/BaiduTranslator.h"
#include <shellapi.h>

IMPLEMENT_DYNAMIC(CBaiduConfigDialog, CDialogEx)

CBaiduConfigDialog::CBaiduConfigDialog(CWnd* pParent)
    : CDialogEx(IDD_BAIDU_CONFIG_DIALOG, pParent)
{
}

CBaiduConfigDialog::~CBaiduConfigDialog()
{
}

void CBaiduConfigDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBaiduConfigDialog, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_APPLY_URL, &CBaiduConfigDialog::OnBtnApplyUrl)
    ON_BN_CLICKED(IDC_BTN_TEST, &CBaiduConfigDialog::OnBtnTest)
END_MESSAGE_MAP()

BOOL CBaiduConfigDialog::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetDlgItemTextW(IDC_EDIT_APPID, m_cfg.appId.c_str());
    SetDlgItemTextW(IDC_EDIT_SECRETKEY, m_cfg.secretKey.c_str());
    SetDlgItemTextW(IDC_EDIT_FROM, m_cfg.fromLang.c_str());
    SetDlgItemTextW(IDC_EDIT_TO, m_cfg.toLang.c_str());

    return TRUE;
}

void CBaiduConfigDialog::OnBtnApplyUrl()
{
    // 打开百度翻译开放平台申请页面
    ::ShellExecuteW(nullptr, L"open",
        L"https://fanyi-api.baidu.com/product/11",
        nullptr, nullptr, SW_SHOWNORMAL);
}

void CBaiduConfigDialog::OnBtnTest()
{
    BaiduConfig cfg;
    CString tmp;
    GetDlgItemTextW(IDC_EDIT_APPID, tmp);
    cfg.appId = tmp.GetString();
    GetDlgItemTextW(IDC_EDIT_SECRETKEY, tmp);
    cfg.secretKey = tmp.GetString();
    GetDlgItemTextW(IDC_EDIT_FROM, tmp);
    cfg.fromLang = tmp.GetString();
    GetDlgItemTextW(IDC_EDIT_TO, tmp);
    cfg.toLang = tmp.GetString();

    if (cfg.appId.empty() || cfg.secretKey.empty())
    {
        AfxMessageBox(L"请先填写 AppID 和 SecretKey", MB_ICONINFORMATION);
        return;
    }

    SetWindowTextW(L"测试中...");

    std::wstring result;
    bool ok = BaiduTranslator::Translate(cfg, L"hello", result);

    SetWindowTextW(L"百度翻译配置");

    if (ok)
    {
        CString msg;
        msg.Format(L"测试成功!\n\nhello → %s", result.c_str());
        AfxMessageBox(msg, MB_ICONINFORMATION);
    }
    else
    {
        AfxMessageBox(L"测试失败,请检查 AppID/SecretKey/网络", MB_ICONERROR);
    }
}

void CBaiduConfigDialog::OnOK()
{
    CString tmp;
    GetDlgItemTextW(IDC_EDIT_APPID, tmp);
    m_cfg.appId = tmp.GetString();
    GetDlgItemTextW(IDC_EDIT_SECRETKEY, tmp);
    m_cfg.secretKey = tmp.GetString();
    GetDlgItemTextW(IDC_EDIT_FROM, tmp);
    m_cfg.fromLang = tmp.GetString();
    GetDlgItemTextW(IDC_EDIT_TO, tmp);
    m_cfg.toLang = tmp.GetString();

    CDialogEx::OnOK();
}
