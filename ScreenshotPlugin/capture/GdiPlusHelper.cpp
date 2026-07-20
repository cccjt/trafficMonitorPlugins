#include "../pch/stdafx.h"
#include "GdiPlusHelper.h"
#include <shlobj.h>
#include <comdef.h>
#include <mutex>
#include <algorithm>

namespace
{
    ULONG_PTR g_gdiplusToken = 0;
    std::mutex g_gdiplusMutex;
    int g_gdiplusRefCount = 0;
}

namespace GdiPlusHelper
{
    void Initialize()
    {
        std::lock_guard<std::mutex> lock(g_gdiplusMutex);
        if (g_gdiplusRefCount++ == 0)
        {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr);
        }
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_gdiplusMutex);
        if (--g_gdiplusRefCount == 0 && g_gdiplusToken != 0)
        {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            g_gdiplusToken = 0;
        }
    }

    Gdiplus::Bitmap* BitmapFromHBITMAP(HBITMAP hBmp)
    {
        if (!hBmp) return nullptr;
        return new Gdiplus::Bitmap(hBmp, nullptr);
    }

    bool CopyDibToClipboard(HBITMAP hBmp)
    {
        if (!hBmp) return false;
        if (!::OpenClipboard(nullptr)) return false;
        ::EmptyClipboard();

        BITMAP bmp = { 0 };
        if (::GetObjectW(hBmp, sizeof(BITMAP), &bmp) == 0)
        {
            ::CloseClipboard();
            return false;
        }

        DWORD paletteSize = 0;
        BITMAPINFOHEADER bi = { 0 };
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = bmp.bmHeight;  // 正值:bottom-up DIB
        bi.biPlanes = 1;
        bi.biBitCount = static_cast<WORD>(bmp.bmBitsPixel);
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;

        DWORD dwBmpSize = ((bmp.bmWidth * bmp.bmBitsPixel + 31) / 32) * 4 * bmp.bmHeight;
        DWORD totalSize = sizeof(BITMAPINFOHEADER) + paletteSize + dwBmpSize;

        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, totalSize);
        if (!hMem)
        {
            ::CloseClipboard();
            return false;
        }

        LPBYTE pMem = static_cast<LPBYTE>(::GlobalLock(hMem));
        if (!pMem)
        {
            ::GlobalFree(hMem);
            ::CloseClipboard();
            return false;
        }

        memcpy(pMem, &bi, sizeof(BITMAPINFOHEADER));

        // 获取位图位数据
        HDC hdc = ::GetDC(nullptr);
        if (hdc)
        {
            BITMAPINFO bi2 = { 0 };
            bi2.bmiHeader = bi;
            ::GetDIBits(hdc, hBmp, 0, bmp.bmHeight,
                pMem + sizeof(BITMAPINFOHEADER),
                &bi2, DIB_RGB_COLORS);
            ::ReleaseDC(nullptr, hdc);
        }

        ::GlobalUnlock(hMem);

        // SetClipboardData 接管所有权
        ::SetClipboardData(CF_DIB, hMem);
        ::CloseClipboard();
        return true;
    }

    bool CopyTextToClipboard(const std::wstring& text)
    {
        if (!::OpenClipboard(nullptr)) return false;
        ::EmptyClipboard();

        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
        if (!hMem)
        {
            ::CloseClipboard();
            return false;
        }

        LPWSTR pMem = static_cast<LPWSTR>(::GlobalLock(hMem));
        if (!pMem)
        {
            ::GlobalFree(hMem);
            ::CloseClipboard();
            return false;
        }
        wcscpy_s(pMem, text.size() + 1, text.c_str());
        ::GlobalUnlock(hMem);

        ::SetClipboardData(CF_UNICODETEXT, hMem);
        ::CloseClipboard();
        return true;
    }

    bool GetEncoderClsid(const wchar_t* format, CLSID* pClsid)
    {
        UINT num = 0, size = 0;
        Gdiplus::ImageCodecInfo* pCodecInfo = nullptr;

        Gdiplus::GetImageEncodersSize(&num, &size);
        if (size == 0) return false;

        pCodecInfo = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
        if (!pCodecInfo) return false;

        Gdiplus::GetImageEncoders(num, size, pCodecInfo);

        bool found = false;
        for (UINT i = 0; i < num; ++i)
        {
            if (wcscmp(pCodecInfo[i].MimeType, format) == 0)
            {
                *pClsid = pCodecInfo[i].Clsid;
                found = true;
                break;
            }
        }
        free(pCodecInfo);
        return found;
    }

    bool SavePng(HBITMAP hBmp, const std::wstring& path)
    {
        if (!hBmp || path.empty()) return false;

        Gdiplus::Bitmap* bmp = BitmapFromHBITMAP(hBmp);
        if (!bmp) return false;

        CLSID clsid;
        bool ok = false;
        if (GetEncoderClsid(L"image/png", &clsid))
        {
            ok = (bmp->Save(path.c_str(), &clsid, nullptr) == Gdiplus::Ok);
        }
        delete bmp;
        return ok;
    }

    bool ShowSaveDialog(HWND hParent, const std::wstring& defaultName,
        std::wstring& outPath)
    {
        wchar_t szFile[MAX_PATH] = { 0 };
        if (!defaultName.empty())
        {
            wcsncpy_s(szFile, MAX_PATH, defaultName.c_str(), _TRUNCATE);
        }

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = hParent;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"PNG 图片 (*.png)\0*.png\0JPEG 图片 (*.jpg)\0*.jpg\0位图 (*.bmp)\0*.bmp\0所有文件 (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
        ofn.lpstrDefExt = L"png";

        if (::GetSaveFileNameW(&ofn))
        {
            outPath = szFile;
            return true;
        }
        return false;
    }

    std::wstring ColorToHex(COLORREF color)
    {
        wchar_t buf[16] = { 0 };
        swprintf_s(buf, 16, L"#%02x%02x%02x",
            GetRValue(color), GetGValue(color), GetBValue(color));
        return buf;
    }

    std::wstring ColorToRgb(COLORREF color)
    {
        wchar_t buf[32] = { 0 };
        swprintf_s(buf, 32, L"rgb(%d,%d,%d)",
            GetRValue(color), GetGValue(color), GetBValue(color));
        return buf;
    }

    std::wstring ColorToCmyk(COLORREF color)
    {
        double r = GetRValue(color) / 255.0;
        double g = GetGValue(color) / 255.0;
        double b = GetBValue(color) / 255.0;

        double k = 1.0 - std::max({ r, g, b });
        if (k >= 1.0)
        {
            return L"cmyk(0,0,0,100)";
        }
        double c = (1.0 - r - k) / (1.0 - k);
        double m = (1.0 - g - k) / (1.0 - k);
        double y = (1.0 - b - k) / (1.0 - k);

        wchar_t buf[48] = { 0 };
        swprintf_s(buf, 48, L"cmyk(%d,%d,%d,%d)",
            static_cast<int>(c * 100 + 0.5),
            static_cast<int>(m * 100 + 0.5),
            static_cast<int>(y * 100 + 0.5),
            static_cast<int>(k * 100 + 0.5));
        return buf;
    }
}
