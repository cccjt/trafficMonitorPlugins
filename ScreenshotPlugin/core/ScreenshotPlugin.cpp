#include "../pch/stdafx.h"
#include "ScreenshotPlugin.h"
#include "../capture/GdiPlusHelper.h"
#include "../ui/OptionsDialog.h"
#include "ScreenshotItem.h"
#include "ScreenshotCore.h"
#include "../services/OcrEngine.h"
#include "../services/BaiduTranslator.h"

ScreenshotPlugin& ScreenshotPlugin::Instance()
{
    static ScreenshotPlugin instance;
    return instance;
}

ScreenshotPlugin::ScreenshotPlugin()
{
}

ScreenshotPlugin::~ScreenshotPlugin()
{
    m_hotkeyMgr.Destroy();
    GdiPlusHelper::Shutdown();
}

void ScreenshotPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    m_pApp = pApp;

    if (m_initialized) return;
    m_initialized = true;

    // 初始化 GDI+
    GdiPlusHelper::Initialize();

    // 初始化 OCR 引擎(失败不影响其他功能)
    OcrEngine::Initialize();

    // 加载配置
    EnsureConfigPath();
    m_config.Load();

    // 创建显示项
    m_item = std::make_unique<ScreenshotItem>();

    // 初始化并注册热键
    m_hotkeyMgr.Initialize();
    ApplyHotkeys();
}

void ScreenshotPlugin::EnsureConfigPath()
{
    if (m_pApp == nullptr) return;

    std::wstring dir = m_pApp->GetPluginConfigDir();
    if (dir.empty()) return;

    // 确保目录存在
    ::CreateDirectoryW(dir.c_str(), nullptr);

    // 拼接配置文件路径
    std::wstring path = dir;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
        path += L'\\';
    path += L"ScreenshotPlugin.ini";

    m_config.SetConfigPath(path);
}

std::vector<ActionType> ScreenshotPlugin::ApplyHotkeys()
{
    return m_hotkeyMgr.RegisterAll(m_config.GetAllHotkeys());
}

void ScreenshotPlugin::ReloadConfig()
{
    m_config.Load();
    ApplyHotkeys();
}

void ScreenshotPlugin::SaveConfig()
{
    m_config.Save();
}

void ScreenshotPlugin::OnScreenshotReady(ActionType action, HBITMAP hScreenBitmap)
{
    if (hScreenBitmap == nullptr)
    {
        if (m_pApp) m_pApp->ShowNotifyMessage(L"截图失败");
        return;
    }
    ScreenshotCore::Instance().Start(action, hScreenBitmap);
}

void ScreenshotPlugin::OnClipboardAction(ActionType action)
{
    // 处理剪贴板类操作
    if (action == ActionType::ClipboardOcr)
    {
        // 从剪贴板读取图片
        if (!::OpenClipboard(nullptr))
        {
            if (m_pApp) m_pApp->ShowNotifyMessage(L"无法打开剪贴板");
            return;
        }
        HANDLE hData = ::GetClipboardData(CF_DIB);
        if (hData == nullptr)
        {
            ::CloseClipboard();
            if (m_pApp) m_pApp->ShowNotifyMessage(L"剪贴板中没有图片");
            return;
        }

        // 将 DIB 转为 HBITMAP
        BITMAPINFO* pBmi = static_cast<BITMAPINFO*>(::GlobalLock(hData));
        if (pBmi == nullptr)
        {
            ::CloseClipboard();
            return;
        }

        HDC hdc = ::GetDC(nullptr);
        HBITMAP hBmp = ::CreateDIBitmap(hdc, &pBmi->bmiHeader,
            CBM_INIT,
            reinterpret_cast<BYTE*>(pBmi) + pBmi->bmiHeader.biSize,
            pBmi, DIB_RGB_COLORS);
        ::ReleaseDC(nullptr, hdc);

        ::GlobalUnlock(hData);
        ::CloseClipboard();

        if (hBmp)
        {
            std::wstring text;
            std::wstring lang = m_config.GetAutoOcrLang();
            if (OcrEngine::Recognize(hBmp, lang, text) && !text.empty())
            {
                GdiPlusHelper::CopyTextToClipboard(text);
                if (m_pApp) m_pApp->ShowNotifyMessage(L"OCR 已识别并复制到剪贴板");
            }
            else
            {
                if (m_pApp) m_pApp->ShowNotifyMessage(L"OCR 识别失败");
            }
            ::DeleteObject(hBmp);
        }
    }
    else if (action == ActionType::ClipboardTranslate)
    {
        // 从剪贴板读取文字
        if (!::OpenClipboard(nullptr))
        {
            if (m_pApp) m_pApp->ShowNotifyMessage(L"无法打开剪贴板");
            return;
        }
        HANDLE hData = ::GetClipboardData(CF_UNICODETEXT);
        if (hData == nullptr)
        {
            ::CloseClipboard();
            if (m_pApp) m_pApp->ShowNotifyMessage(L"剪贴板中没有文字");
            return;
        }
        wchar_t* pText = static_cast<wchar_t*>(::GlobalLock(hData));
        if (pText == nullptr)
        {
            ::CloseClipboard();
            return;
        }
        std::wstring text = pText;
        ::GlobalUnlock(hData);
        ::CloseClipboard();

        if (text.empty())
        {
            if (m_pApp) m_pApp->ShowNotifyMessage(L"剪贴板为空");
            return;
        }

        std::wstring result;
        if (BaiduTranslator::Translate(m_config.GetBaidu(), text, result))
        {
            ::MessageBoxW(nullptr, result.c_str(), L"翻译结果", MB_OK);
        }
        else
        {
            ::MessageBoxW(nullptr, BaiduTranslator::GetLastError().c_str(),
                L"翻译失败", MB_ICONERROR);
        }
    }
}

IPluginItem* ScreenshotPlugin::GetItem(int index)
{
    if (index == 0 && m_item) return m_item.get();
    return nullptr;
}

ScreenshotPlugin::OptionReturn ScreenshotPlugin::ShowOptionsDialog(void* hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    COptionsDialog dlg(CWnd::FromHandle(static_cast<HWND>(hParent)));
    INT_PTR result = dlg.DoModal();
    if (result == IDOK)
    {
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

const wchar_t* ScreenshotPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"截图工具";
    case TMI_DESCRIPTION: return L"类似 PixPin 的截图/贴图/OCR/翻译插件";
    case TMI_AUTHOR:      return L"TrafficMonitor Plugins";
    case TMI_COPYRIGHT:   return L"MIT";
    case TMI_VERSION:     return L"0.1.0";
    case TMI_URL:         return L"";
    }
    return L"";
}

// 导出函数
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &ScreenshotPlugin::Instance();
}
