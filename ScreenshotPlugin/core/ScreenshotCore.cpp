#include "../pch/stdafx.h"
#include "ScreenshotCore.h"
#include "../ui/SelectionOverlay.h"
#include "../ui/SelectionToolbar.h"
#include "../ui/PinWindow.h"
#include "ScreenshotPlugin.h"
#include "ScreenshotItem.h"
#include "../capture/ScreenCapture.h"
#include "../capture/GdiPlusHelper.h"
#include "../services/OcrEngine.h"
#include "../services/BaiduTranslator.h"
#include "../services/QrCodeDecoder.h"

ScreenshotCore& ScreenshotCore::Instance()
{
    static ScreenshotCore instance;
    return instance;
}

ScreenshotCore::ScreenshotCore() {}
ScreenshotCore::~ScreenshotCore() {}

void ScreenshotCore::Start(ActionType action, HBITMAP hBmp)
{
    if (m_currentOverlay)
    {
        // 已有进行中的截图,先关闭
        m_currentOverlay->Close();
        m_currentOverlay.reset();
    }
    if (m_currentToolbar)
    {
        m_currentToolbar->Destroy();
        m_currentToolbar.reset();
    }

    m_currentOverlay = std::make_unique<SelectionOverlay>();
    if (!m_currentOverlay->Create(hBmp, action))
    {
        m_currentOverlay.reset();
        return;
    }

    // 创建工具条(默认隐藏,选区确认后显示)
    m_currentToolbar = std::make_unique<SelectionToolbar>();
    if (m_currentToolbar->Create(m_currentOverlay.get()))
    {
        m_currentOverlay->SetToolbar(m_currentToolbar.get());
    }
}

HBITMAP ScreenshotCore::GetFinalBitmap(SelectionOverlay* overlay)
{
    if (!overlay) return nullptr;
    return overlay->CropSelection(false);
}

void ScreenshotCore::CloseOverlay()
{
    if (m_currentToolbar)
    {
        m_currentToolbar->Destroy();
        m_currentToolbar.reset();
    }
    if (m_currentOverlay)
    {
        m_currentOverlay->Close();
        m_currentOverlay.reset();
    }
}

void ScreenshotCore::DoAction(ActionType action, SelectionOverlay* overlay, ActionType /*sourceAction*/)
{
    if (!overlay) return;

    // 获取最终位图(从屏幕截图裁剪选区)
    HBITMAP hFinal = overlay->CropSelection(false);

    if (hFinal == nullptr)
    {
        CloseOverlay();
        return;
    }

    // 派发动作
    switch (action)
    {
    case ActionType::Capture:
        // 复制并关闭
        GdiPlusHelper::CopyDibToClipboard(hFinal);
        ::DeleteObject(hFinal);
        CloseOverlay();
        break;

    case ActionType::Pin:
    {
        // 创建贴图窗口
        RECT rcSel;
        overlay->GetSelection(rcSel);
        auto pin = new PinWindow();
        if (pin->Create(hFinal, rcSel.left, rcSel.top))
        {
            RegisterPinWindow(pin);
            // hFinal 所有权转移给 PinWindow
        }
        else
        {
            ::DeleteObject(hFinal);
            delete pin;
        }
        CloseOverlay();
        break;
    }

    case ActionType::Save:
    {
        std::wstring path;
        if (GdiPlusHelper::ShowSaveDialog(nullptr, L"screenshot.png", path))
        {
            GdiPlusHelper::SavePng(hFinal, path);
        }
        ::DeleteObject(hFinal);
        CloseOverlay();
        break;
    }

    case ActionType::Ocr:
    {
        std::wstring text;
        std::wstring lang = ScreenshotPlugin::Instance().GetConfig().GetAutoOcrLang();
        bool ok = OcrEngine::Recognize(hFinal, lang, text);
        ::DeleteObject(hFinal);
        if (ok && !text.empty())
        {
            GdiPlusHelper::CopyTextToClipboard(text);
            if (auto app = ScreenshotPlugin::Instance().GetApp())
                app->ShowNotifyMessage(L"OCR 已识别并复制到剪贴板");
        }
        else
        {
            if (auto app = ScreenshotPlugin::Instance().GetApp())
                app->ShowNotifyMessage(L"OCR 识别失败");
        }
        CloseOverlay();
        break;
    }

    case ActionType::Translate:
    {
        std::wstring text;
        std::wstring lang = ScreenshotPlugin::Instance().GetConfig().GetAutoOcrLang();
        bool ok = OcrEngine::Recognize(hFinal, lang, text);
        ::DeleteObject(hFinal);
        if (ok && !text.empty())
        {
            std::wstring result;
            BaiduConfig baidu = ScreenshotPlugin::Instance().GetConfig().GetBaidu();
            if (BaiduTranslator::Translate(baidu, text, result))
            {
                // 弹窗显示
                ::MessageBoxW(nullptr, result.c_str(), L"翻译结果", MB_OK);
            }
            else
            {
                ::MessageBoxW(nullptr, BaiduTranslator::GetLastError().c_str(),
                    L"翻译失败", MB_ICONERROR);
            }
        }
        else
        {
            if (auto app = ScreenshotPlugin::Instance().GetApp())
                app->ShowNotifyMessage(L"OCR 识别失败,无法翻译");
        }
        CloseOverlay();
        break;
    }

    case ActionType::QrCode:
    {
        std::wstring result;
        bool ok = QrCodeDecoder::Decode(hFinal, result);
        ::DeleteObject(hFinal);
        if (ok && !result.empty())
        {
            GdiPlusHelper::CopyTextToClipboard(result);
            ::MessageBoxW(nullptr, result.c_str(), L"二维码内容", MB_OK);
        }
        else
        {
            ::MessageBoxW(nullptr, L"未检测到二维码", L"识别失败", MB_ICONINFORMATION);
        }
        CloseOverlay();
        break;
    }

    default:
        ::DeleteObject(hFinal);
        CloseOverlay();
        break;
    }
}

void ScreenshotCore::RegisterPinWindow(PinWindow* p)
{
    if (!p) return;
    m_pinWindows.push_back(p);

    // 更新显示项
    auto& plugin = ScreenshotPlugin::Instance();
    auto item = dynamic_cast<ScreenshotItem*>(plugin.GetItem(0));
    if (item) item->Increment();
}

void ScreenshotCore::UnregisterPinWindow(PinWindow* p)
{
    if (!p) return;
    auto it = std::find(m_pinWindows.begin(), m_pinWindows.end(), p);
    if (it != m_pinWindows.end())
    {
        m_pinWindows.erase(it);

        auto& plugin = ScreenshotPlugin::Instance();
        auto item = dynamic_cast<ScreenshotItem*>(plugin.GetItem(0));
        if (item) item->Decrement();
    }
}
