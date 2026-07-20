#include "../pch/stdafx.h"
#include "SelectionOverlay.h"
#include "../capture/ScreenCapture.h"
#include "../core/ScreenshotPlugin.h"
#include "SelectionToolbar.h"
#include "../core/ScreenshotCore.h"
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

SelectionOverlay::SelectionOverlay()
{
}

SelectionOverlay::~SelectionOverlay()
{
    Destroy();
}

bool SelectionOverlay::Create(HBITMAP hScreenBitmap, ActionType action)
{
    m_hScreenBitmap = hScreenBitmap;
    m_action = action;

    // 获取屏幕范围
    ScreenCapture::GetVirtualScreenRect(m_screenX, m_screenY, m_screenW, m_screenH);

    HMODULE hModule = GetThisModule();
    static const wchar_t* CLASS_NAME = L"ScreenshotSelectionOverlay";
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = &SelectionOverlay::WndProc;
        wc.hInstance = hModule;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = ::LoadCursor(nullptr, IDC_CROSS);
        ::RegisterClassExW(&wc);
        registered = true;
    }

    m_hWnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"ScreenshotOverlay",
        WS_POPUP,
        m_screenX, m_screenY, m_screenW, m_screenH,
        nullptr, nullptr, hModule, this);

    if (m_hWnd == nullptr) return false;

    ::ShowWindow(m_hWnd, SW_SHOWNORMAL);
    ::UpdateWindow(m_hWnd);
    ::SetForegroundWindow(m_hWnd);
    ::SetFocus(m_hWnd);

    // 创建放大镜(延迟到第一次需要时显示)
    m_magnifier = std::make_unique<MagnifierWnd>();
    m_magnifier->Create();
    m_magnifier->SetSource(m_hScreenBitmap, m_screenX, m_screenY);

    return true;
}

void SelectionOverlay::Destroy()
{
    if (m_magnifier)
    {
        m_magnifier->Destroy();
        m_magnifier.reset();
    }
    if (m_hWnd != nullptr)
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
    if (m_hScreenBitmap != nullptr)
    {
        ::DeleteObject(m_hScreenBitmap);
        m_hScreenBitmap = nullptr;
    }
}

void SelectionOverlay::Close()
{
    Destroy();
}

LRESULT CALLBACK SelectionOverlay::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    SelectionOverlay* self = reinterpret_cast<SelectionOverlay*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self == nullptr) return ::DefWindowProcW(hWnd, message, wParam, lParam);

    switch (message)
    {
    case WM_PAINT:
        self->OnPaint();
        return 0;
    case WM_LBUTTONDOWN:
        self->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        self->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        self->OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDBLCLK:
        self->OnLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_RBUTTONUP:
        self->OnRButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYDOWN:
        self->OnKeyDown(static_cast<UINT>(wParam));
        return 0;
    case WM_ERASEBKGND:
        return 1;   // 避免闪烁
    case WM_DESTROY:
        // 不在这里 Destroy,因为 Destroy() 会调用 DestroyWindow 递归
        break;
    }
    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

void SelectionOverlay::GetSelectionRect(RECT& rc) const
{
    rc = m_rcSel;
}

void SelectionOverlay::NormalizeSelection()
{
    if (m_rcSel.left > m_rcSel.right) std::swap(m_rcSel.left, m_rcSel.right);
    if (m_rcSel.top > m_rcSel.bottom) std::swap(m_rcSel.top, m_rcSel.bottom);
}

bool SelectionOverlay::IsInSelection(int x, int y) const
{
    if (!m_hasSelection) return false;
    return x >= m_rcSel.left && x <= m_rcSel.right && y >= m_rcSel.top && y <= m_rcSel.bottom;
}

// 手柄编号:
//  1    2    3
//  4         5
//  6    7    8
// 1=左上 2=上中 3=右上 4=左中 5=右中 6=左下 7=下中 8=右下
int SelectionOverlay::HitTestHandle(int x, int y) const
{
    if (!m_hasSelection) return 0;
    const int tol = 8;
    int l = m_rcSel.left, r = m_rcSel.right, t = m_rcSel.top, b = m_rcSel.bottom;
    int mx = (l + r) / 2, my = (t + b) / 2;

    if (x >= l - tol && x <= l + tol && y >= t - tol && y <= t + tol) return 1;
    if (x >= mx - tol && x <= mx + tol && y >= t - tol && y <= t + tol) return 2;
    if (x >= r - tol && x <= r + tol && y >= t - tol && y <= t + tol) return 3;
    if (x >= l - tol && x <= l + tol && y >= my - tol && y <= my + tol) return 4;
    if (x >= r - tol && x <= r + tol && y >= my - tol && y <= my + tol) return 5;
    if (x >= l - tol && x <= l + tol && y >= b - tol && y <= b + tol) return 6;
    if (x >= mx - tol && x <= mx + tol && y >= b - tol && y <= b + tol) return 7;
    if (x >= r - tol && x <= r + tol && y >= b - tol && y <= b + tol) return 8;
    return 0;
}

void SelectionOverlay::OnLButtonDown(int x, int y)
{
    // 转换为客户坐标(已经是客户坐标,因为我们的窗口=屏幕)
    // 屏幕坐标 = 客户坐标(窗口覆盖整个虚拟屏,从 m_screenX 开始)
    int sx = x + m_screenX;
    int sy = y + m_screenY;

    // 1. 优先判断手柄
    int handle = HitTestHandle(sx, sy);
    if (handle > 0)
    {
        m_dragMode = DragMode::ResizeHandle;
        m_resizeHandle = handle;
        m_dragStart = { sx, sy };
        m_dragOrigSel = m_rcSel;
        ::SetCapture(m_hWnd);
        return;
    }

    // 2. 判断是否在选区内
    if (IsInSelection(sx, sy))
    {
        m_dragMode = DragMode::MoveSelection;
        m_dragStart = { sx, sy };
        m_dragOrigSel = m_rcSel;
        ::SetCapture(m_hWnd);
        return;
    }

    // 3. 开始新选区
    m_dragMode = DragMode::NewSelection;
    m_rcSel.left = sx;
    m_rcSel.top = sy;
    m_rcSel.right = sx;
    m_rcSel.bottom = sy;
    m_dragStart = { sx, sy };
    ::SetCapture(m_hWnd);
    m_hasSelection = false;  // 在移动前还不算"有效选区"
}

void SelectionOverlay::OnMouseMove(int x, int y)
{
    int sx = x + m_screenX;
    int sy = y + m_screenY;

    if (m_dragMode == DragMode::NewSelection)
    {
        m_rcSel.right = sx;
        m_rcSel.bottom = sy;
        NormalizeSelection();
        if (m_rcSel.right - m_rcSel.left > 4 || m_rcSel.bottom - m_rcSel.top > 4)
        {
            m_hasSelection = true;
        }
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
    else if (m_dragMode == DragMode::MoveSelection)
    {
        int dx = sx - m_dragStart.x;
        int dy = sy - m_dragStart.y;
        int w = m_dragOrigSel.right - m_dragOrigSel.left;
        int h = m_dragOrigSel.bottom - m_dragOrigSel.top;
        m_rcSel.left = m_dragOrigSel.left + dx;
        m_rcSel.top = m_dragOrigSel.top + dy;
        m_rcSel.right = m_rcSel.left + w;
        m_rcSel.bottom = m_rcSel.top + h;
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
    else if (m_dragMode == DragMode::ResizeHandle)
    {
        int dx = sx - m_dragStart.x;
        int dy = sy - m_dragStart.y;
        RECT rc = m_dragOrigSel;
        switch (m_resizeHandle)
        {
        case 1: rc.left += dx; rc.top += dy; break;
        case 2: rc.top += dy; break;
        case 3: rc.right += dx; rc.top += dy; break;
        case 4: rc.left += dx; break;
        case 5: rc.right += dx; break;
        case 6: rc.left += dx; rc.bottom += dy; break;
        case 7: rc.bottom += dy; break;
        case 8: rc.right += dx; rc.bottom += dy; break;
        }
        // 防止翻转
        if (rc.left < rc.right && rc.top < rc.bottom)
        {
            m_rcSel = rc;
            ::InvalidateRect(m_hWnd, nullptr, FALSE);
        }
    }
    else
    {
        // 没有拖动:更新光标和放大镜
        int handle = HitTestHandle(sx, sy);
        HCURSOR hCur = nullptr;
        if (handle > 0)
        {
            switch (handle)
            {
            case 1: case 8: hCur = ::LoadCursor(nullptr, IDC_SIZENWSE); break;
            case 3: case 6: hCur = ::LoadCursor(nullptr, IDC_SIZENESW); break;
            case 2: case 7: hCur = ::LoadCursor(nullptr, IDC_SIZENS); break;
            case 4: case 5: hCur = ::LoadCursor(nullptr, IDC_SIZEWE); break;
            }
        }
        else if (IsInSelection(sx, sy))
        {
            hCur = ::LoadCursor(nullptr, IDC_SIZEALL);
        }
        else
        {
            hCur = ::LoadCursor(nullptr, IDC_CROSS);
        }
        if (hCur) ::SetCursor(hCur);
    }

    // 放大镜
    if (m_dragMode == DragMode::None)
    {
        UpdateMagnifier(sx, sy);
    }
}

void SelectionOverlay::OnLButtonUp(int x, int y)
{
    if (m_dragMode != DragMode::None)
    {
        ::ReleaseCapture();
        m_dragMode = DragMode::None;
        NormalizeSelection();
        if (m_rcSel.right - m_rcSel.left < 4 || m_rcSel.bottom - m_rcSel.top < 4)
        {
            // 太小,视为无效
            m_hasSelection = false;
        }
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void SelectionOverlay::OnLButtonDblClk(int x, int y)
{
    // 双击选区:复制并关闭
    int sx = x + m_screenX;
    int sy = y + m_screenY;
    if (IsInSelection(sx, sy))
    {
        // 执行复制动作并关闭覆盖层
        ScreenshotCore::Instance().DoAction(ActionType::Capture, this, m_action);
    }
}

void SelectionOverlay::OnRButtonUp(int x, int y)
{
    int sx = x + m_screenX;
    int sy = y + m_screenY;
    if (IsInSelection(sx, sy))
    {
        // 右键:直接贴图
        ScreenshotCore::Instance().DoAction(ActionType::Pin, this, m_action);
    }
}

void SelectionOverlay::OnKeyDown(UINT vk)
{
    if (vk == VK_ESCAPE)
    {
        Close();
        return;
    }
    if (vk == VK_RETURN)
    {
        if (m_hasSelection)
        {
            // 显示工具条
            ShowToolbarIfNeeded();
        }
        return;
    }
    if (vk == 'C')
    {
        // 复制当前放大镜中心颜色
        if (m_magnifier) m_magnifier->CopyCurrentColor();
        return;
    }
    if (vk == VK_SHIFT)
    {
        // 切换颜色格式
        if (m_magnifier) m_magnifier->ToggleColorFormat();
        return;
    }
}

void SelectionOverlay::UpdateMagnifier(int x, int y)
{
    if (!m_magnifier) return;
    if (IsInSelection(x, y))
    {
        if (!m_magnifierVisible)
        {
            m_magnifier->Show();
            m_magnifierVisible = true;
        }
        m_magnifier->Update(x, y);
    }
    else
    {
        if (m_magnifierVisible)
        {
            m_magnifier->Hide();
            m_magnifierVisible = false;
        }
    }
}

void SelectionOverlay::ShowToolbarIfNeeded()
{
    // 阶段4 实现
    if (m_toolbar)
    {
        m_toolbar->Show();
    }
}

void SelectionOverlay::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = ::BeginPaint(m_hWnd, &ps);
    if (!hdc) return;

    // 双缓冲
    HDC hMemDC = ::CreateCompatibleDC(hdc);
    HBITMAP hMemBmp = ::CreateCompatibleBitmap(hdc, m_screenW, m_screenH);
    HGDIOBJ oldBmp = ::SelectObject(hMemDC, hMemBmp);

    // 1. 绘制完整屏幕截图
    if (m_hScreenBitmap != nullptr)
    {
        HDC hSrcDC = ::CreateCompatibleDC(hdc);
        HGDIOBJ oldSrc = ::SelectObject(hSrcDC, m_hScreenBitmap);
        ::BitBlt(hMemDC, 0, 0, m_screenW, m_screenH, hSrcDC, 0, 0, SRCCOPY);
        ::SelectObject(hSrcDC, oldSrc);
        ::DeleteDC(hSrcDC);
    }

    // 准备 GDI+ 用于半透明绘制
    Gdiplus::Graphics graphics(hMemDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // 选区在客户区中的本地坐标
    RECT rcSelLocal = {
        m_rcSel.left - m_screenX, m_rcSel.top - m_screenY,
        m_rcSel.right - m_screenX, m_rcSel.bottom - m_screenY
    };

    // 2. 选区外暗化遮罩
    if (m_hasSelection)
    {
        Gdiplus::SolidBrush darkBrush(Gdiplus::Color(160, 0, 0, 0));

        // 上方
        graphics.FillRectangle(&darkBrush, 0, 0, m_screenW, rcSelLocal.top);
        // 下方
        graphics.FillRectangle(&darkBrush,
            0, rcSelLocal.bottom,
            m_screenW, m_screenH - rcSelLocal.bottom);
        // 左方
        graphics.FillRectangle(&darkBrush,
            0, rcSelLocal.top,
            rcSelLocal.left, rcSelLocal.bottom - rcSelLocal.top);
        // 右方
        graphics.FillRectangle(&darkBrush,
            rcSelLocal.right, rcSelLocal.top,
            m_screenW - rcSelLocal.right, rcSelLocal.bottom - rcSelLocal.top);

        // 3. 选区边框(蓝色)
        Gdiplus::Pen bluePen(Gdiplus::Color(255, 0, 120, 212), 1.0f);
        graphics.DrawRectangle(&bluePen,
            rcSelLocal.left, rcSelLocal.top,
            rcSelLocal.right - rcSelLocal.left - 1,
            rcSelLocal.bottom - rcSelLocal.top - 1);

        // 4. 8 个手柄
        Gdiplus::SolidBrush handleBrush(Gdiplus::Color(255, 0, 120, 212));
        auto drawHandle = [&](int cx, int cy) {
            graphics.FillRectangle(&handleBrush, cx - 4, cy - 4, 8, 8);
            };
        int l = rcSelLocal.left, r = rcSelLocal.right, t = rcSelLocal.top, b = rcSelLocal.bottom;
        int mx = (l + r) / 2, my = (t + b) / 2;
        drawHandle(l, t); drawHandle(mx, t); drawHandle(r, t);
        drawHandle(l, my);                  drawHandle(r, my);
        drawHandle(l, b); drawHandle(mx, b); drawHandle(r, b);

        // 5. 左上角坐标/尺寸标签
        std::wostringstream oss;
        int sw = rcSelLocal.right - rcSelLocal.left;
        int sh = rcSelLocal.bottom - rcSelLocal.top;
        oss << m_rcSel.left << L"," << m_rcSel.top << L"  " << sw << L" × " << sh << L" px";

        std::wstring label = oss.str();
        Gdiplus::Font font(L"Consolas", 10);
        Gdiplus::RectF textRect;
        graphics.MeasureString(label.c_str(), static_cast<INT>(label.size()),
            &font, Gdiplus::PointF(0, 0), &textRect);

        int labelX = rcSelLocal.left;
        int labelY = rcSelLocal.top - static_cast<int>(textRect.Height) - 6;
        if (labelY < 0) labelY = rcSelLocal.top + 4;

        Gdiplus::SolidBrush labelBg(Gdiplus::Color(200, 0, 0, 0));
        graphics.FillRectangle(&labelBg,
            static_cast<INT>(labelX), static_cast<INT>(labelY),
            static_cast<INT>(textRect.Width + 10), static_cast<INT>(textRect.Height + 4));

        Gdiplus::SolidBrush labelFg(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::PointF labelPf(labelX + 5, labelY + 2);
        graphics.DrawString(label.c_str(), static_cast<INT>(label.size()),
            &font, labelPf, &labelFg);
    }

    // 提交到屏幕
    ::BitBlt(hdc, 0, 0, m_screenW, m_screenH, hMemDC, 0, 0, SRCCOPY);

    ::SelectObject(hMemDC, oldBmp);
    ::DeleteObject(hMemBmp);
    ::DeleteDC(hMemDC);

    ::EndPaint(m_hWnd, &ps);
}

void SelectionOverlay::SetSelection(const RECT& rc)
{
    m_rcSel = rc;
    m_hasSelection = (rc.right > rc.left && rc.bottom > rc.top);
    if (m_hWnd) ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

HBITMAP SelectionOverlay::CropSelection(bool /*includeAnnotations*/)
{
    if (!m_hasSelection || m_hScreenBitmap == nullptr) return nullptr;

    // 选区在屏幕坐标系,需要转换为底图坐标
    int x = m_rcSel.left - m_screenX;
    int y = m_rcSel.top - m_screenY;
    int w = m_rcSel.right - m_rcSel.left;
    int h = m_rcSel.bottom - m_rcSel.top;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }

    return ScreenCapture::CropRegion(m_hScreenBitmap, x, y, w, h);
}
