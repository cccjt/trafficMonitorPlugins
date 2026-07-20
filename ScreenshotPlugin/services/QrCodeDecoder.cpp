#include "../pch/stdafx.h"
#include "QrCodeDecoder.h"

// v1 占位实现
// 阶段6 实现:优先使用 Windows.Media.BarcodeScanner (Win10 1903+)
// 或集成 ZBar 开源库

bool QrCodeDecoder::Decode(HBITMAP hBmp, std::wstring& result)
{
    (void)hBmp;
    result.clear();
    return false;
}
