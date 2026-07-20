#pragma once
#include <Windows.h>
#include <string>
#include <gdiplus.h>

namespace GdiPlusHelper
{
    // 初始化 GDI+ (进程内多次调用安全)
    void Initialize();
    void Shutdown();

    // HBITMAP -> Gdiplus::Bitmap(所有权共享,不接管 HBITMAP)
    Gdiplus::Bitmap* BitmapFromHBITMAP(HBITMAP hBmp);

    // 复制 DIB 到剪贴板
    bool CopyDibToClipboard(HBITMAP hBmp);

    // 复制文本到剪贴板
    bool CopyTextToClipboard(const std::wstring& text);

    // 保存为 PNG
    bool SavePng(HBITMAP hBmp, const std::wstring& path);

    // 保存对话框,返回用户选择的路径
    bool ShowSaveDialog(HWND hParent, const std::wstring& defaultName,
        std::wstring& outPath);

    // 将 COLORREF 转为 HEX 字符串(#rrggbb)
    std::wstring ColorToHex(COLORREF color);

    // 将 COLORREF 转为 RGB 字符串(rgb(r,g,b))
    std::wstring ColorToRgb(COLORREF color);

    // 将 COLORREF 转为 CMYK 字符串(cmyk(c,m,y,k))
    std::wstring ColorToCmyk(COLORREF color);

    // 获取 CLSID(根据 MIME)
    bool GetEncoderClsid(const wchar_t* format, CLSID* pClsid);
}
