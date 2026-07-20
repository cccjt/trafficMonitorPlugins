#include "../pch/stdafx.h"
#include "MagnifierWnd.h"
#include "../capture/GdiPlusHelper.h"
#include <sstream>

// 获取当前模块句柄
static HMODULE GetThisModule()
{
    HMODULE hModule = nullptr;
    ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetThisModule),
        &hModule);
    return hModule;
}

MagnifierWnd::MagnifierWnd()
{
}

MagnifierWnd::~MagnifierWnd()
{
    Destroy();
}

bool MagnifierWnd::Create()
{
    if (m_hWnd != nullptr) return true;

    HMODULE hModule = GetThisModule();

    static const wchar_t* CLASS_NAME = L"ScreenshotMagnifierWnd";
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = &MagnifierWnd::WndProc;
        wc.hInstance = hModule;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        ::RegisterClassExW(&wc);
        registered = true;
    }

    // 创建为分层窗口(WS_EX_LAYERED),位置先随意
    m_hWnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        CLASS_NAME, L"Magnifier",
        WS_POPUP,
        0, 0, WINDOW_W, WINDOW_H,
        nullptr, nullptr, hModule, this);

    return m_hWnd != nullptr;
}

void MagnifierWnd::Destroy()
{
    if (m_hWnd != nullptr)
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

LRESULT CALLBACK MagnifierWnd::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    MagnifierWnd* self = reinterpret_cast<MagnifierWnd*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self == nullptr) return ::DefWindowProcW(hWnd, message, wParam, lParam);

    if (message == WM_PAINT)
    {
        self->OnPaint();
        return 0;
    }

    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

void MagnifierWnd::Show()
{
    if (m_hWnd != nullptr)
    {
        ::ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
    }
}

void MagnifierWnd::Hide()
{
    if (m_hWnd != nullptr)
    {
        ::ShowWindow(m_hWnd, SW_HIDE);
    }
}

void MagnifierWnd::SetSource(HBITMAP hSource, int srcX, int srcY)
{
    m_hSource = hSource;
    m_srcX = srcX;
    m_srcY = srcY;
}

COLORREF MagnifierWnd::GetSourcePixel(int screenX, int screenY)
{
    if (m_hSource == nullptr) return RGB(0, 0, 0);

    // 计算在底图中的坐标
    int bmpX = screenX - m_srcX;
    int bmpY = screenY - m_srcY;
    if (bmpX < 0 || bmpY < 0) return RGB(0, 0, 0);

    // 获取位图尺寸
    BITMAP bmp = { 0 };
    if (::GetObjectW(m_hSource, sizeof(BITMAP), &bmp) == 0) return RGB(0, 0, 0);
    if (bmpX >= bmp.bmWidth || bmpY >= bmp.bmHeight) return RGB(0, 0, 0);

    // 从位图读取像素
    HDC hSrcDC = ::CreateCompatibleDC(nullptr);
    HGDIOBJ oldObj = ::SelectObject(hSrcDC, m_hSource);
    COLORREF color = ::GetPixel(hSrcDC, bmpX, bmpY);
    ::SelectObject(hSrcDC, oldObj);
    ::DeleteDC(hSrcDC);

    return color;
}

COLORREF MagnifierWnd::GetCenterColor()
{
    return GetSourcePixel(m_curX, m_curY);
}

std::wstring MagnifierWnd::GetCurrentColorString(COLORREF color)
{
    switch (m_format)
    {
    case ColorFormat::HEX: return GdiPlusHelper::ColorToHex(color);
    case ColorFormat::RGB: return GdiPlusHelper::ColorToRgb(color);
    case ColorFormat::CMYK: return GdiPlusHelper::ColorToCmyk(color);
    }
    return L"";
}

void MagnifierWnd::UpdateWindowPosition(int screenX, int screenY)
{
    // 默认显示在鼠标右下方 16px
    int x = screenX + 16;
    int y = screenY + 16;

    // 检测是否超出屏幕
    int screenW = ::GetSystemMetrics(SM_CXSCREEN);
    int screenH = ::GetSystemMetrics(SM_CYSCREEN);
    HMONITOR hMon = ::MonitorFromPoint(POINT{ screenX, screenY }, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (::GetMonitorInfoW(hMon, &mi))
    {
        screenW = mi.rcMonitor.right - mi.rcMonitor.left;
        screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;
        if (x + WINDOW_W > mi.rcMonitor.right)
        {
            x = screenX - WINDOW_W - 16;
        }
        if (y + WINDOW_H > mi.rcMonitor.bottom)
        {
            y = screenY - WINDOW_H - 16;
        }
        // 兜底
        if (x < mi.rcMonitor.left) x = mi.rcMonitor.left;
        if (y < mi.rcMonitor.top)  y = mi.rcMonitor.top;
    }

    ::SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, WINDOW_W, WINDOW_H,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void MagnifierWnd::Update(int screenX, int screenY)
{
    m_curX = screenX;
    m_curY = screenY;
    if (m_hWnd == nullptr) return;

    UpdateWindowPosition(screenX, screenY);

    // 触发重绘
    ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void MagnifierWnd::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = ::BeginPaint(m_hWnd, &ps);
    if (!hdc) return;

    // 双缓冲
    HDC hMemDC = ::CreateCompatibleDC(hdc);
    HBITMAP hMemBmp = ::CreateCompatibleBitmap(hdc, WINDOW_W, WINDOW_H);
    HGDIOBJ oldBmp = ::SelectObject(hMemDC, hMemBmp);

    // 背景:半透明黑色
    RECT rcClient = { 0, 0, WINDOW_W, WINDOW_H };
    HBRUSH hBg = ::CreateSolidBrush(RGB(30, 30, 30));
    ::FillRect(hMemDC, &rcClient, hBg);
    ::DeleteObject(hBg);

    // 绘制像素网格(中心 = 鼠标位置)
    int gridStartX = (WINDOW_W - GRID_TOTAL) / 2;
    int gridStartY = 4;

    if (m_hSource != nullptr)
    {
        BITMAP bmp = { 0 };
        ::GetObjectW(m_hSource, sizeof(BITMAP), &bmp);

        int half = GRID_SIZE / 2;
        for (int dy = -half; dy <= half; ++dy)
        {
            for (int dx = -half; dx <= half; ++dx)
            {
                int screenX = m_curX + dx;
                int screenY = m_curY + dy;
                COLORREF c = GetSourcePixel(screenX, screenY);

                int px = gridStartX + (dx + half) * GRID_PIXEL;
                int py = gridStartY + (dy + half) * GRID_PIXEL;

                HBRUSH hPixel = ::CreateSolidBrush(c);
                RECT rcPixel = { px, py, px + GRID_PIXEL, py + GRID_PIXEL };
                ::FillRect(hMemDC, &rcPixel, hPixel);
                ::DeleteObject(hPixel);
            }
        }
    }

    // 网格边框
    HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ oldPen = ::SelectObject(hMemDC, hPen);
    HBRUSH hNullBrush = static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH));
    HGDIOBJ oldBrush = ::SelectObject(hMemDC, hNullBrush);
    ::Rectangle(hMemDC, gridStartX - 1, gridStartY - 1,
        gridStartX + GRID_TOTAL + 1, gridStartY + GRID_TOTAL + 1);
    ::SelectObject(hMemDC, oldBrush);
    ::SelectObject(hMemDC, oldPen);
    ::DeleteObject(hPen);

    // 中心方块高亮
    int centerPx = gridStartX + (GRID_SIZE / 2) * GRID_PIXEL;
    int centerPy = gridStartY + (GRID_SIZE / 2) * GRID_PIXEL;
    HPEN hCenterPen = ::CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
    HGDIOBJ oldCenterPen = ::SelectObject(hMemDC, hCenterPen);
    ::Rectangle(hMemDC, centerPx, centerPy, centerPx + GRID_PIXEL, centerPy + GRID_PIXEL);
    ::SelectObject(hMemDC, oldCenterPen);
    ::DeleteObject(hCenterPen);

    // 底部文字区:坐标 + 颜色值
    int textY = gridStartY + GRID_TOTAL + 6;
    ::SetBkMode(hMemDC, TRANSPARENT);
    ::SetTextColor(hMemDC, RGB(255, 255, 255));

    // 坐标
    std::wostringstream ossCoord;
    ossCoord << L"(" << m_curX << L"," << m_curY << L")";
    ::TextOutW(hMemDC, 4, textY, ossCoord.str().c_str(),
        static_cast<int>(ossCoord.str().size()));

    // 颜色值
    COLORREF centerColor = GetCenterColor();
    std::wstring colorStr = GetCurrentColorString(centerColor);
    ::TextOutW(hMemDC, 4, textY + 14, colorStr.c_str(),
        static_cast<int>(colorStr.size()));

    // 提示
    ::SetTextColor(hMemDC, RGB(180, 180, 180));
    ::TextOutW(hMemDC, 4, textY + 28, L"C:复制  Shift:切换格式",
        static_cast<int>(wcslen(L"C:复制  Shift:切换格式")));

    // 提交到屏幕
    ::BitBlt(hdc, 0, 0, WINDOW_W, WINDOW_H, hMemDC, 0, 0, SRCCOPY);

    ::SelectObject(hMemDC, oldBmp);
    ::DeleteObject(hMemBmp);
    ::DeleteDC(hMemDC);

    ::EndPaint(m_hWnd, &ps);
}

void MagnifierWnd::CopyCurrentColor()
{
    COLORREF color = GetCenterColor();
    std::wstring str = GetCurrentColorString(color);
    GdiPlusHelper::CopyTextToClipboard(str);
}

void MagnifierWnd::ToggleColorFormat()
{
    switch (m_format)
    {
    case ColorFormat::HEX: m_format = ColorFormat::RGB; break;
    case ColorFormat::RGB: m_format = ColorFormat::CMYK; break;
    case ColorFormat::CMYK: m_format = ColorFormat::HEX; break;
    }
    if (m_hWnd) ::InvalidateRect(m_hWnd, nullptr, FALSE);
}
