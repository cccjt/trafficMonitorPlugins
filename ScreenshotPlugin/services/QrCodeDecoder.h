#pragma once
#include <Windows.h>
#include <string>

// 二维码/条形码识别
class QrCodeDecoder
{
public:
    // 解码图片中的二维码
    //   hBmp:   HBITMAP
    //   result: 输出识别到的内容
    //   返回值: 成功/失败
    static bool Decode(HBITMAP hBmp, std::wstring& result);
};
