#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// OCR 引擎封装(基于 Windows.Media.Ocr)
// 仅支持 Win10+,需要安装对应语言包
class OcrEngine
{
public:
    // 初始化 OCR 引擎
    static bool Initialize();

    // 识别图片中的文字
    //   hBmp:     HBITMAP(支持 24/32 位)
    //   language: BCP-47 语言标签,如 "zh-Hans-CN" 或 "en-US"
    //   result:   输出识别结果
    //   返回值:   成功/失败
    static bool Recognize(HBITMAP hBmp, const std::wstring& language, std::wstring& result);

    // 获取已安装的 OCR 语言列表
    static std::vector<std::wstring> GetAvailableLanguages();

    // 是否已初始化
    static bool IsInitialized() { return s_initialized; }

private:
    static bool s_initialized;
};
