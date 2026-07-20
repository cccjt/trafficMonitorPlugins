#include "../pch/stdafx.h"
#include "OcrEngine.h"
#include <atomic>

// v1 占位实现:OCR 引擎暂时返回失败
// 阶段7 用 C++/WinRT 实现 Windows.Media.Ocr 调用

bool OcrEngine::s_initialized = false;

std::atomic<bool> g_initialized{ false };

bool OcrEngine::Initialize()
{
    // TODO: 阶段7 实现 winrt::init_apartment()
    s_initialized = true;
    return true;
}

bool OcrEngine::Recognize(HBITMAP hBmp, const std::wstring& language, std::wstring& result)
{
    // v1 占位:返回失败,提示用户该功能正在开发中
    (void)hBmp;
    (void)language;
    result = L"";
    return false;
}

std::vector<std::wstring> OcrEngine::GetAvailableLanguages()
{
    // v1 占位
    return { L"zh-Hans-CN", L"en-US" };
}
