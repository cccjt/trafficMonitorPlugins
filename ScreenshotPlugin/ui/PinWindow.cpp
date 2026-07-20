#include "../pch/stdafx.h"
#include "PinWindow.h"
#include "../capture/GdiPlusHelper.h"
#include "../core/ScreenshotCore.h"
#include "../res/resource.h"
#include <algorithm>
#include <cmath>

// 右键菜单命令 ID
#define IDM_PIN_COPY       5001
#define IDM_PIN_SAVE       5002
#define IDM_PIN_RESET_SIZE 5003
#define IDM_PIN_RESET_ALPHA 5004
#define IDM_PIN_CLOSE      5005

static HMODULE GetThisModule()
{
    HMODULE hModule = nullptr;
    ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetThisModule),
        &hModule);
    return hModule;
}

PinWindow::PinWindow()
{
}

PinWindow::~PinWindow()
{
    Destroy();
}

bool PinWindow::Create(HBITMAP hBmp, int x, int y)
{
    m_hBitmap = hBmp;

    // 获取位图尺寸
    BITMAP bmp = { 0 };
    if (::GetObjectW(m_hBitmap, sizeof(BITMAP), &bmp) == 0) return false;
    m_imgW = bmp.bmWidth;
    m_imgH = bmp.bmHeight;

    HMODULE hModule = GetThisModule();
    static const wchar_t* CLASS_NAME = L"ScreenshotPinWindow";
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = &PinWindow::WndProc;
        wc.hInstance = hModule;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        ::RegisterClassExW(&wc);
        registered = true;
    }

    int winW, winH;
    CalcWindowSize(m_imgW, m_imgH, winW, winH);

    m_hWnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME, L"PinWindow",
        WS_POPUP,
        x, y, winW, winH,
        nullptr, nullptr, hModule, this);

    if (m_hWnd == nullptr) return false;

    ::ShowWindow(m_hWnd, SW_SHOWNORMAL);

    // 渲染并设置分层窗口
    RenderToWindow();

    return true;
}

void PinWindow::Destroy()
{
    if (m_hWnd != nullptr)
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (m_hBitmap != nullptr)
    {
        ::DeleteObject(m_hBitmap);
        m_hBitmap = nullptr;
    }
}

void PinWindow::Close()
{
    // 先通知 Core 注销自己
    ScreenshotCore::Instance().UnregisterPinWindow(this);
    Destroy();
    delete this;
}

void PinWindow::CalcWindowSize(int imgW, int imgH, int& winW, int& winH) const
{
    winW = static_cast<int>(imgW * m_scale) + 2 * BORDER;
    winH = static_cast<int>(imgH * m_scale) + 2 * BORDER;
}

void PinWindow::UpdateWindowSize()
{
    if (m_hWnd == nullptr) return;
    int winW, winH;
    CalcWindowSize(m_imgW, m_imgH, winW, winH);
    ::SetWindowPos(m_hWnd, nullptr, 0, 0, winW, winH,
        SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
    RenderToWindow();
}

// 四角测试
// 返回值: 1=左上 2=右上 3=左下 4=右下 0=非角
int PinWindow::HitTestCorner(int x, int y) const
{
    RECT rc;
    ::GetClientRect(m_hWnd, &rc);
    const int tol = 12;

    if (x <= rc.left + tol && y <= rc.top + tol) return 1;
    if (x >= rc.right - tol && y <= rc.top + tol) return 2;
    if (x <= rc.left + tol && y >= rc.bottom - tol) return 3;
    if (x >= rc.right - tol && y >= rc.bottom - tol) return 4;
    return 0;
}

LRESULT PinWindow::OnNcHitTest(int x, int y)
{
    // 转换为客户坐标
    POINT pt = { x, y };
    ::ScreenToClient(m_hWnd, &pt);

    // 优先判断四角
    int corner = HitTestCorner(pt.x, pt.y);
    if (corner > 0)
    {
        // 让系统不处理(我们自己实现缩放)
        return HTCLIENT;
    }

    // 非角区域:允许拖动
    return HTCAPTION;
}

LRESULT CALLBACK PinWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    PinWindow* self = reinterpret_cast<PinWindow*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self == nullptr) return ::DefWindowProcW(hWnd, message, wParam, lParam);

    switch (message)
    {
    case WM_PAINT:
        self->OnPaint();
        return 0;
    case WM_LBUTTONDOWN:
        self->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        self->OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        self->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDBLCLK:
        self->OnLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_RBUTTONUP:
        self->OnRButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEWHEEL:
        self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_KEYDOWN:
        self->OnKeyDown(static_cast<UINT>(wParam));
        return 0;
    case WM_ACTIVATE:
        self->OnActivate(LOWORD(wParam));
        return 0;
    case WM_SETFOCUS:
        self->OnSetFocus(reinterpret_cast<HWND>(wParam));
        return 0;
    case WM_KILLFOCUS:
        self->OnKillFocus(reinterpret_cast<HWND>(wParam));
        return 0;
    case WM_NCHITTEST:
        return self->OnNcHitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        // 不要在这里 delete this,因为 Close() 会处理
        break;
    }
    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

void PinWindow::OnActivate(WORD state)
{
    bool newFocus = (state != WA_INACTIVE);
    if (newFocus != m_focused)
    {
        m_focused = newFocus;
        RenderToWindow();
    }
}

void PinWindow::OnSetFocus(HWND /*hWnd*/)
{
    if (!m_focused)
    {
        m_focused = true;
        RenderToWindow();
    }
}

void PinWindow::OnKillFocus(HWND /*hWnd*/)
{
    if (m_focused)
    {
        m_focused = false;
        RenderToWindow();
    }
}

void PinWindow::OnLButtonDown(int x, int y)
{
    int corner = HitTestCorner(x, y);
    if (corner > 0)
    {
        // 开始拖动缩放
        m_dragging = true;
        m_dragCorner = corner;
        m_dragStart = { x, y };

        // 记录起始窗口位置
        RECT rc;
        ::GetWindowRect(m_hWnd, &rc);
        m_dragStartPos = { rc.left, rc.top };
        m_dragStartScale = m_scale;

        ::SetCapture(m_hWnd);
        ::SetFocus(m_hWnd);
    }
    else
    {
        // 普通点击:获取焦点
        ::SetFocus(m_hWnd);
    }
}

void PinWindow::OnLButtonUp(int /*x*/, int /*y*/)
{
    if (m_dragging)
    {
        m_dragging = false;
        ::ReleaseCapture();
    }
}

void PinWindow::OnMouseMove(int x, int y)
{
    if (m_dragging)
    {
        // 根据角调整 scale
        int dx = x - m_dragStart.x;
        int dy = y - m_dragStart.y;

        // 根据哪个角被拖动,计算合适的 scale 变化
        float newScale = m_dragStartScale;
        // 简化:取 dx + dy 的平均值作为缩放变化
        // 左上角往左上拖动 = 放大
        // 右下角往右下拖动 = 放大
        float delta = 0.0f;
        switch (m_dragCorner)
        {
        case 1: // 左上
            delta = static_cast<float>(-dx - dy) / m_imgW;
            break;
        case 2: // 右上
            delta = static_cast<float>(dx - dy) / m_imgW;
            break;
        case 3: // 左下
            delta = static_cast<float>(-dx + dy) / m_imgW;
            break;
        case 4: // 右下
            delta = static_cast<float>(dx + dy) / m_imgW;
            break;
        }
        newScale = m_dragStartScale + delta * 2.0f;
        newScale = std::max(0.2f, std::min(5.0f, newScale));

        if (std::fabs(newScale - m_scale) > 0.01f)
        {
            // 调整窗口位置以保持非拖动角不动
            // 简化:不调整位置,只调整大小
            m_scale = newScale;
            UpdateWindowSize();
        }
    }
    else
    {
        // 更新光标
        int corner = HitTestCorner(x, y);
        HCURSOR hCur = nullptr;
        switch (corner)
        {
        case 1: case 4: hCur = ::LoadCursor(nullptr, IDC_SIZENWSE); break;
        case 2: case 3: hCur = ::LoadCursor(nullptr, IDC_SIZENESW); break;
        default: hCur = ::LoadCursor(nullptr, IDC_SIZEALL); break;
        }
        if (hCur) ::SetCursor(hCur);
    }
}

void PinWindow::OnLButtonDblClk(int /*x*/, int /*y*/)
{
    // 双击:复制并关闭
    if (m_hBitmap)
    {
        GdiPlusHelper::CopyDibToClipboard(m_hBitmap);
    }
    Close();
}

void PinWindow::OnRButtonUp(int x, int y)
{
    ShowContextMenu(x, y);
}

void PinWindow::OnMouseWheel(int delta)
{
    int newAlpha = m_alpha + (delta > 0 ? 16 : -16);
    newAlpha = std::max(64, std::min(255, newAlpha));
    if (newAlpha != m_alpha)
    {
        m_alpha = newAlpha;
        RenderToWindow();
    }
}

void PinWindow::OnKeyDown(UINT vk)
{
    if (vk == VK_ESCAPE)
    {
        Close();
    }
}

void PinWindow::ShowContextMenu(int x, int y)
{
    HMENU hMenu = ::LoadMenuW(GetThisModule(), MAKEINTRESOURCEW(IDR_PIN_MENU));
    if (!hMenu) return;
    HMENU hSubMenu = ::GetSubMenu(hMenu, 0);
    if (!hSubMenu) { ::DestroyMenu(hMenu); return; }

    // 转换到屏幕坐标
    POINT pt = { x, y };
    ::ClientToScreen(m_hWnd, &pt);

    // 必须设置窗口为前景,否则菜单不消失
    ::SetForegroundWindow(m_hWnd);

    int cmd = ::TrackPopupMenu(hSubMenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
        pt.x, pt.y, 0, m_hWnd, nullptr);

    ::DestroyMenu(hMenu);

    switch (cmd)
    {
    case IDM_PIN_COPY:
        if (m_hBitmap) GdiPlusHelper::CopyDibToClipboard(m_hBitmap);
        break;
    case IDM_PIN_SAVE:
    {
        std::wstring path;
        if (GdiPlusHelper::ShowSaveDialog(m_hWnd, L"pin.png", path))
        {
            GdiPlusHelper::SavePng(m_hBitmap, path);
        }
        break;
    }
    case IDM_PIN_RESET_SIZE:
        m_scale = 1.0f;
        UpdateWindowSize();
        break;
    case IDM_PIN_RESET_ALPHA:
        m_alpha = 255;
        RenderToWindow();
        break;
    case IDM_PIN_CLOSE:
        Close();
        break;
    }
}

void PinWindow::OnPaint()
{
    // 分层窗口由 UpdateLayeredWindow 直接绘制,WM_PAINT 一般不会被触发
    // 这里仅验证客户区,不重复调用 RenderToWindow 避免无限重绘
    PAINTSTRUCT ps;
    ::BeginPaint(m_hWnd, &ps);
    ::EndPaint(m_hWnd, &ps);
}

void PinWindow::RenderToWindow()
{
    if (m_hWnd == nullptr || m_hBitmap == nullptr) return;

    RECT rc;
    ::GetClientRect(m_hWnd, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW <= 0 || winH <= 0) return;

    // 创建 32bit ARGB 内存位图
    HDC hScreen = ::GetDC(nullptr);
    HDC hMemDC = ::CreateCompatibleDC(hScreen);
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hMemBmp = ::CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    ::ReleaseDC(nullptr, hScreen);
    if (!hMemBmp) { ::DeleteDC(hMemDC); return; }

    HGDIOBJ oldBmp = ::SelectObject(hMemDC, hMemBmp);

    // 使用 GDI+ 在内存 DC 上绘制
    Gdiplus::Graphics graphics(hMemDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));  // 完全透明

    // 1. 绘制发光边框
    // 颜色根据焦点状态
    Gdiplus::Color borderColor = m_focused
        ? Gdiplus::Color(255, 0, 120, 212)    // 蓝色
        : Gdiplus::Color(255, 154, 154, 154);  // 灰色

    // 外层光环(6层渐变)
    for (int i = BORDER; i > 0; --i)
    {
        BYTE a = static_cast<BYTE>(m_focused
            ? (40 + (BORDER - i) * 30)
            : (20 + (BORDER - i) * 20));
        Gdiplus::Pen pen(Gdiplus::Color(a, borderColor.GetR(), borderColor.GetG(), borderColor.GetB()),
            static_cast<float>(i * 2));
        graphics.DrawRectangle(&pen,
            static_cast<float>(BORDER - i),
            static_cast<float>(BORDER - i),
            static_cast<float>(winW - 2 * (BORDER - i) - 1),
            static_cast<float>(winH - 2 * (BORDER - i) - 1));
    }

    // 内层实线边框
    Gdiplus::Pen innerPen(borderColor, 2.0f);
    graphics.DrawRectangle(&innerPen,
        static_cast<float>(BORDER - 1),
        static_cast<float>(BORDER - 1),
        static_cast<float>(winW - 2 * BORDER + 1),
        static_cast<float>(winH - 2 * BORDER + 1));

    // 2. 绘制图片(应用透明度)
    Gdiplus::Bitmap srcBitmap(m_hBitmap, nullptr);
    // 缩放到适应窗口
    int scaledW = static_cast<int>(m_imgW * m_scale);
    int scaledH = static_cast<int>(m_imgH * m_scale);

    // 创建带 alpha 的图片
    Gdiplus::ImageAttributes attr;
    Gdiplus::ColorMatrix cm = { 0 };
    memset(&cm, 0, sizeof(cm));
    cm.m[0][0] = 1.0f;
    cm.m[1][1] = 1.0f;
    cm.m[2][2] = 1.0f;
    cm.m[3][3] = m_alpha / 255.0f;
    cm.m[4][4] = 1.0f;
    attr.SetColorMatrix(&cm, Gdiplus::ColorMatrixFlagsDefault,
        Gdiplus::ColorAdjustTypeBitmap);

    Gdiplus::Rect destRect(BORDER, BORDER, scaledW, scaledH);
    graphics.DrawImage(&srcBitmap, destRect,
        0, 0, m_imgW, m_imgH, Gdiplus::UnitPixel, &attr);

    // 3. 提交到 UpdateLayeredWindow
    POINT ptSrc = { 0, 0 };
    SIZE size = { winW, winH };
    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    ::UpdateLayeredWindow(m_hWnd, nullptr, nullptr, &size,
        hMemDC, &ptSrc, RGB(0, 0, 0), &blend, ULW_ALPHA);

    ::SelectObject(hMemDC, oldBmp);
    ::DeleteObject(hMemBmp);
    ::DeleteDC(hMemDC);
}
