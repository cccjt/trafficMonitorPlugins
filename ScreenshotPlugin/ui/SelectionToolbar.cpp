#include "../pch/stdafx.h"
#include "SelectionToolbar.h"
#include "SelectionOverlay.h"
#include "../core/ScreenshotCore.h"
#include "../config/ActionType.h"

// 按钮编号(对应动作)
#define BTN_PIN       1
#define BTN_SAVE      2
#define BTN_COPY      3
#define BTN_OCR       4
#define BTN_TRANSLATE 5
#define BTN_CLOSE     6

static HMODULE GetThisModule()
{
    HMODULE hModule = nullptr;
    ::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GetThisModule),
        &hModule);
    return hModule;
}

SelectionToolbar::SelectionToolbar()
{
    m_buttons = {
        { BTN_PIN,       L"贴图",   L"贴到屏幕上", {0,0,0,0}, false },
        { BTN_SAVE,      L"保存",   L"保存为文件", {0,0,0,0}, false },
        { BTN_COPY,      L"复制",   L"复制到剪贴板", {0,0,0,0}, false },
        { BTN_OCR,       L"OCR",    L"识别文字", {0,0,0,0}, false },
        { BTN_TRANSLATE, L"翻译",   L"翻译文字", {0,0,0,0}, false },
        { BTN_CLOSE,     L"✕",     L"关闭", {0,0,0,0}, false },
    };
}

SelectionToolbar::~SelectionToolbar()
{
    Destroy();
}

bool SelectionToolbar::Create(SelectionOverlay* overlay)
{
    m_overlay = overlay;
    HMODULE hModule = GetThisModule();

    static const wchar_t* CLASS_NAME = L"ScreenshotSelectionToolbar";
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = &SelectionToolbar::WndProc;
        wc.hInstance = hModule;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        ::RegisterClassExW(&wc);
        registered = true;
    }

    m_hWnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"SelectionToolbar",
        WS_POPUP,
        0, 0, TB_WIDTH, TB_HEIGHT,
        nullptr, nullptr, hModule, this);

    if (m_hWnd == nullptr) return false;

    return true;
}

void SelectionToolbar::Destroy()
{
    if (m_hWnd != nullptr)
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void SelectionToolbar::Show()
{
    if (m_hWnd == nullptr) return;
    UpdatePosition();
    ::ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
}

void SelectionToolbar::Hide()
{
    if (m_hWnd != nullptr)
    {
        ::ShowWindow(m_hWnd, SW_HIDE);
    }
}

void SelectionToolbar::UpdatePosition()
{
    if (m_hWnd == nullptr || m_overlay == nullptr) return;

    RECT rcSel;
    m_overlay->GetSelection(rcSel);

    int x = rcSel.left;
    int y = rcSel.bottom + 4;

    // 检测是否超出屏幕
    HMONITOR hMon = ::MonitorFromRect(&rcSel, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    if (::GetMonitorInfoW(hMon, &mi))
    {
        if (y + TB_HEIGHT > mi.rcMonitor.bottom)
        {
            // 移到选区上方
            y = rcSel.top - TB_HEIGHT - 4;
        }
        if (x + TB_WIDTH > mi.rcMonitor.right)
        {
            x = mi.rcMonitor.right - TB_WIDTH;
        }
        if (x < mi.rcMonitor.left) x = mi.rcMonitor.left;
        if (y < mi.rcMonitor.top) y = mi.rcMonitor.top;
    }

    ::SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, TB_WIDTH, TB_HEIGHT,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

LRESULT CALLBACK SelectionToolbar::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_CREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    SelectionToolbar* self = reinterpret_cast<SelectionToolbar*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
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
    case WM_MOUSELEAVE:
        self->m_hoveredBtn = 0;
        for (auto& b : self->m_buttons) b.hover = false;
        ::InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

void SelectionToolbar::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = ::BeginPaint(m_hWnd, &ps);
    if (!hdc) return;

    // 双缓冲
    HDC hMemDC = ::CreateCompatibleDC(hdc);
    HBITMAP hMemBmp = ::CreateCompatibleBitmap(hdc, TB_WIDTH, TB_HEIGHT);
    HGDIOBJ oldBmp = ::SelectObject(hMemDC, hMemBmp);

    // 背景:白色 + 圆角
    RECT rcClient = { 0, 0, TB_WIDTH, TB_HEIGHT };
    HBRUSH hBg = ::CreateSolidBrush(RGB(255, 255, 255));
    ::FillRect(hMemDC, &rcClient, hBg);
    ::DeleteObject(hBg);

    // 边框
    HPEN hPen = ::CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HGDIOBJ oldPen = ::SelectObject(hMemDC, hPen);
    HBRUSH hNullBrush = static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH));
    HGDIOBJ oldBrush = ::SelectObject(hMemDC, hNullBrush);
    ::Rectangle(hMemDC, 0, 0, TB_WIDTH, TB_HEIGHT);
    ::SelectObject(hMemDC, oldBrush);
    ::SelectObject(hMemDC, oldPen);
    ::DeleteObject(hPen);

    // 计算按钮位置
    int btnW = 60;
    int btnH = TB_HEIGHT - 4;
    int gap = 0;
    int startX = 2;
    int startY = 2;

    for (size_t i = 0; i < m_buttons.size(); ++i)
    {
        Button& b = m_buttons[i];
        b.rect = {
            startX + static_cast<int>(i) * (btnW + gap),
            startY,
            startX + static_cast<int>(i) * (btnW + gap) + btnW,
            startY + btnH
        };

        // 悬停效果
        if (b.hover)
        {
            HBRUSH hHover = ::CreateSolidBrush(RGB(230, 240, 255));
            ::FillRect(hMemDC, &b.rect, hHover);
            ::DeleteObject(hHover);
        }

        // 文字
        ::SetBkMode(hMemDC, TRANSPARENT);
        ::SetTextColor(hMemDC, RGB(50, 50, 50));
        SIZE size = { 0 };
        ::GetTextExtentPoint32W(hMemDC, b.text, static_cast<int>(wcslen(b.text)), &size);
        int tx = b.rect.left + (btnW - size.cx) / 2;
        int ty = b.rect.top + (btnH - size.cy) / 2;
        ::TextOutW(hMemDC, tx, ty, b.text, static_cast<int>(wcslen(b.text)));
    }

    ::BitBlt(hdc, 0, 0, TB_WIDTH, TB_HEIGHT, hMemDC, 0, 0, SRCCOPY);

    ::SelectObject(hMemDC, oldBmp);
    ::DeleteObject(hMemBmp);
    ::DeleteDC(hMemDC);

    ::EndPaint(m_hWnd, &ps);
}

void SelectionToolbar::OnLButtonDown(int x, int y)
{
    // 找到点击的按钮
    for (const auto& b : m_buttons)
    {
        if (x >= b.rect.left && x <= b.rect.right &&
            y >= b.rect.top && y <= b.rect.bottom)
        {
            // 执行对应动作
            ActionType action = ActionType::Capture;
            switch (b.id)
            {
            case BTN_PIN:       action = ActionType::Pin; break;
            case BTN_SAVE:      action = ActionType::Save; break;
            case BTN_COPY:      action = ActionType::Capture; break;  // Capture = 复制并关闭
            case BTN_OCR:       action = ActionType::Ocr; break;
            case BTN_TRANSLATE: action = ActionType::Translate; break;
            case BTN_CLOSE:
                // 关闭:销毁整个截图流程
                if (m_overlay) m_overlay->Close();
                return;
            }
            ScreenshotCore::Instance().DoAction(action, m_overlay, ActionType::Capture);
            return;
        }
    }
}

void SelectionToolbar::OnMouseMove(int x, int y)
{
    // 跟踪鼠标离开
    TRACKMOUSEEVENT tme = { 0 };
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    ::TrackMouseEvent(&tme);

    // 找到当前悬停的按钮
    int newHover = 0;
    for (auto& b : m_buttons)
    {
        bool isHover = (x >= b.rect.left && x <= b.rect.right &&
            y >= b.rect.top && y <= b.rect.bottom);
        if (isHover)
        {
            newHover = b.id;
            b.hover = true;
        }
        else
        {
            b.hover = false;
        }
    }
    if (newHover != m_hoveredBtn)
    {
        m_hoveredBtn = newHover;
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}
