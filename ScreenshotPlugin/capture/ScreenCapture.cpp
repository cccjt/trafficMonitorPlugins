#include "../pch/stdafx.h"
#include "ScreenCapture.h"

void ScreenCapture::GetVirtualScreenRect(int& x, int& y, int& w, int& h)
{
    x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    w = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    h = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (w <= 0 || h <= 0)
    {
        // 兜底:主屏
        w = ::GetSystemMetrics(SM_CXSCREEN);
        h = ::GetSystemMetrics(SM_CYSCREEN);
        x = 0;
        y = 0;
    }
}

HBITMAP ScreenCapture::CaptureAllMonitors()
{
    int x, y, w, h;
    GetVirtualScreenRect(x, y, w, h);

    HDC hScreen = ::GetDC(nullptr);
    if (!hScreen) return nullptr;

    HDC hMem = ::CreateCompatibleDC(hScreen);
    if (!hMem) { ::ReleaseDC(nullptr, hScreen); return nullptr; }

    HBITMAP hBmp = ::CreateCompatibleBitmap(hScreen, w, h);
    if (!hBmp)
    {
        ::DeleteDC(hMem);
        ::ReleaseDC(nullptr, hScreen);
        return nullptr;
    }

    HGDIOBJ oldObj = ::SelectObject(hMem, hBmp);
    ::BitBlt(hMem, 0, 0, w, h, hScreen, x, y, SRCCOPY);
    ::SelectObject(hMem, oldObj);

    ::DeleteDC(hMem);
    ::ReleaseDC(nullptr, hScreen);

    return hBmp;
}

COLORREF ScreenCapture::GetScreenColorAt(int screenX, int screenY)
{
    HDC hScreen = ::GetDC(nullptr);
    if (!hScreen) return RGB(0, 0, 0);
    COLORREF color = ::GetPixel(hScreen, screenX, screenY);
    ::ReleaseDC(nullptr, hScreen);
    return color;
}

HBITMAP ScreenCapture::CropRegion(HBITMAP hSource, int x, int y, int w, int h)
{
    if (!hSource || w <= 0 || h <= 0) return nullptr;

    HDC hScreen = ::GetDC(nullptr);
    HDC hSrcDC = ::CreateCompatibleDC(hScreen);
    HDC hDstDC = ::CreateCompatibleDC(hScreen);

    HBITMAP hDst = ::CreateCompatibleBitmap(hScreen, w, h);
    if (!hDst)
    {
        ::DeleteDC(hSrcDC);
        ::DeleteDC(hDstDC);
        ::ReleaseDC(nullptr, hScreen);
        return nullptr;
    }

    HGDIOBJ oldSrc = ::SelectObject(hSrcDC, hSource);
    HGDIOBJ oldDst = ::SelectObject(hDstDC, hDst);

    ::BitBlt(hDstDC, 0, 0, w, h, hSrcDC, x, y, SRCCOPY);

    ::SelectObject(hSrcDC, oldSrc);
    ::SelectObject(hDstDC, oldDst);
    ::DeleteDC(hSrcDC);
    ::DeleteDC(hDstDC);
    ::ReleaseDC(nullptr, hScreen);

    return hDst;
}
