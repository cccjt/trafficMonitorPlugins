#include "stdafx.h"
#include "MenuPanel.h"
#include "IconManager.h"
#include "Logger.h"
#include <Windows.h>
#include <windowsx.h>
#include <string>
#include <wchar.h>

namespace
{
// 避免与 windows.h 的 min/max 宏冲突
int MinInt(int a, int b) { return a < b ? a : b; }
int MaxInt(int a, int b) { return a > b ? a : b; }

// 方案开关:
//   false = 方案 B(自绘标题栏,齿轮和关闭按钮在同一行)
//   true  = 方案 A(系统标题栏,齿轮在关闭按钮下方)
const bool kUseSystemCaption = false;

const wchar_t* const kPanelClassName = L"LeftClickMenuPanelWnd";
const wchar_t* const kPanelTitle = L"快捷菜单";

// 获取窗口 DPI,旧系统上回退到屏幕 DPI
UINT GetWindowDpi(HWND hWnd)
{
    typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
        ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

    if (getDpiForWindow != nullptr && hWnd != nullptr)
    {
        UINT dpi = getDpiForWindow(hWnd);
        if (dpi != 0) return dpi;
    }

    HDC hdc = ::GetDC(hWnd);
    UINT dpi = static_cast<UINT>(::GetDeviceCaps(hdc, LOGPIXELSX));
    ::ReleaseDC(hWnd, hdc);
    return dpi != 0 ? dpi : 96;
}

// 系统是否处于深色主题
bool IsDarkTheme()
{
    DWORD appsUseLightTheme = 1;
    DWORD dataSize = sizeof(appsUseLightTheme);
    HKEY hKey = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        ::RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&appsUseLightTheme), &dataSize);
        ::RegCloseKey(hKey);
    }
    return appsUseLightTheme == 0;
}

// 按 DPI 调整窗口矩形,老系统上回退到非 DPI 版本
BOOL AdjustWindowRectForDpi(RECT* rc, DWORD style, BOOL menu, DWORD exStyle, UINT dpi)
{
    typedef BOOL(WINAPI* AdjustWindowRectExForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static AdjustWindowRectExForDpiFn adjustForDpi = reinterpret_cast<AdjustWindowRectExForDpiFn>(
        ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "AdjustWindowRectExForDpi"));

    if (adjustForDpi != nullptr)
    {
        return adjustForDpi(rc, style, menu, exStyle, dpi);
    }
    return ::AdjustWindowRectEx(rc, style, menu, exStyle);
}

void FillRoundedRect(HDC hdc, const RECT& rc, COLORREF color, int radius)
{
    HBRUSH hBrush = ::CreateSolidBrush(color);
    HGDIOBJ oldBrush = ::SelectObject(hdc, hBrush);
    HGDIOBJ oldPen = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    ::SelectObject(hdc, oldPen);
    ::SelectObject(hdc, oldBrush);
    ::DeleteObject(hBrush);
}

bool IsEmptyMenu(const std::vector<MenuConfigItem>& items)
{
    for (const auto& item : items)
    {
        if (!item.isSeparator) return false;
    }
    return true;
}
}

MenuPanel::MenuPanel() {}
MenuPanel::~MenuPanel()
{
    if (m_font != nullptr) ::DeleteObject(m_font);
    if (m_titleFont != nullptr) ::DeleteObject(m_titleFont);
}

MenuPanel::Result MenuPanel::Show(const std::vector<MenuConfigItem>& items, HWND hOwner,
    int screenX, int screenY, const MenuViewOptions& options)
{
    MenuPanel panel;
    panel.m_items = &items;
    if (!panel.Create(hOwner, screenX, screenY, options))
    {
        Logger::Instance().Warn(L"MenuPanel::Show: 面板创建失败");
        return Result();
    }

    panel.RunMessageLoop();
    return panel.m_result;
}

bool MenuPanel::Create(HWND hOwner, int screenX, int screenY, const MenuViewOptions& options)
{
    m_hOwner = hOwner;
    m_screenX = screenX;
    m_screenY = screenY;
    m_dpi = GetWindowDpi(hOwner);

    // 显示设置
    m_tileMode = options.tileMode;
    m_tileColumns = options.tileColumns;
    if (m_tileColumns < 2) m_tileColumns = 2;
    if (m_tileColumns > 8) m_tileColumns = 8;
    m_configuredWidth = options.panelWidth;
    if (m_configuredWidth < 160 || m_configuredWidth > 600) m_configuredWidth = 200;

    // 字体
    LOGFONTW lf = { 0 };
    NONCLIENTMETRICSW metrics = { sizeof(NONCLIENTMETRICSW) };
    if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    {
        lf = metrics.lfMenuFont;
    }
    else
    {
        lf.lfHeight = -::MulDiv(10, m_dpi, 72);
        lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
    }
    lf.lfHeight = -::MulDiv(10, m_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    m_font = ::CreateFontIndirectW(&lf);

    LOGFONTW titleLf = lf;
    titleLf.lfWeight = FW_SEMIBOLD;
    m_titleFont = ::CreateFontIndirectW(&titleLf);

    // 主题颜色
    if (IsDarkTheme())
    {
        m_bgColor = RGB(43, 43, 43);
        m_textColor = RGB(243, 243, 243);
        m_disabledColor = RGB(130, 130, 130);
        m_hoverColor = RGB(62, 62, 62);
        m_dividerColor = RGB(80, 80, 80);
        m_buttonColor = RGB(210, 210, 210);
        m_closeHoverColor = RGB(196, 43, 28);
    }
    else
    {
        m_bgColor = ::GetSysColor(COLOR_MENU);
        m_textColor = ::GetSysColor(COLOR_MENUTEXT);
        m_disabledColor = ::GetSysColor(COLOR_GRAYTEXT);
        m_hoverColor = RGB(229, 229, 229);
        m_dividerColor = RGB(205, 205, 205);
        m_buttonColor = RGB(50, 50, 50);
        m_closeHoverColor = RGB(196, 43, 28);
    }

    // 尺寸随 DPI 缩放
    m_rowHeight = ::MulDiv(40, m_dpi, 96);
    m_sepHeight = ::MulDiv(11, m_dpi, 96);
    m_headerHeight = ::MulDiv(42, m_dpi, 96);
    m_gearRowHeight = ::MulDiv(36, m_dpi, 96);
    m_paddingX = ::MulDiv(12, m_dpi, 96);
    m_paddingY = ::MulDiv(6, m_dpi, 96);
    m_iconSize = ::MulDiv(22, m_dpi, 96);
    m_iconGap = ::MulDiv(9, m_dpi, 96);
    m_tileGap = ::MulDiv(5, m_dpi, 96);
    m_tileIconSize = ::MulDiv(32, m_dpi, 96);
    m_tileCellHeight = ::MulDiv(74, m_dpi, 96);

    // 面板宽度来自配置(96 DPI 像素),按 DPI 缩放
    const int minConfiguredWidth = ::MulDiv(160, m_dpi, 96);
    m_width = ::MulDiv(m_configuredWidth, m_dpi, 96);
    if (m_width < minConfiguredWidth) m_width = minConfiguredWidth;

    POINT pt = { screenX, screenY };
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    HMONITOR hMonitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    ::GetMonitorInfoW(hMonitor, &monitorInfo);
    const RECT work = monitorInfo.rcWork;
    const int margin = ::MulDiv(8, m_dpi, 96);

    int workWidth = work.right - work.left - margin * 2;
    m_maxPanelWidth = workWidth > 0 ? workWidth : m_width;
    if (workWidth > 0 && m_width > workWidth)
    {
        m_width = workWidth;
    }

    // 平铺格子尺寸
    UpdateTileMetrics();

    // 内容高度
    m_contentHeight = CalculateContentHeight();

    const bool empty = IsEmptyMenu(*m_items);
    const int emptyViewHeight = kUseSystemCaption
        ? ::MulDiv(8, m_dpi, 96)
        : ::MulDiv(40, m_dpi, 96);

    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    if (kUseSystemCaption)
    {
        style |= WS_CAPTION | WS_SYSMENU;
    }

    // 计算标题栏占用的非客户区高度
    RECT probe = { 0, 0, m_width, 0 };
    AdjustWindowRectForDpi(&probe, style, FALSE, exStyle, m_dpi);
    const int nonClientHeight = probe.bottom - probe.top;

    int maxClientHeight = work.bottom - work.top - margin * 2 - nonClientHeight;
    const int headerHeight = kUseSystemCaption ? m_gearRowHeight : m_headerHeight;
    m_maxBodyHeight = maxClientHeight - headerHeight;
    if (m_maxBodyHeight < 0) m_maxBodyHeight = 0;

    if (empty)
    {
        m_contentHeight = emptyViewHeight;
        m_viewHeight = MaxInt(0, MinInt(emptyViewHeight, m_maxBodyHeight));
        m_contentHeight = m_viewHeight;
    }
    else
    {
        m_viewHeight = MinInt(m_contentHeight, m_maxBodyHeight);
    }
    m_scroll = 0;

    int clientHeight = headerHeight + m_viewHeight;
    m_height = clientHeight;
    m_bodyTop = headerHeight;

    RECT winRect = { 0, 0, m_width, clientHeight };
    AdjustWindowRectForDpi(&winRect, style, FALSE, exStyle, m_dpi);
    int winWidth = winRect.right - winRect.left;
    int winHeight = winRect.bottom - winRect.top;

    int left = screenX + margin;
    int top = screenY + margin;
    if (left + winWidth > work.right) left = screenX - winWidth - margin;
    if (top + winHeight > work.bottom) top = screenY - winHeight - margin;
    if (left < work.left) left = work.left;
    if (top < work.top) top = work.top;
    if (left + winWidth > work.right) left = work.right - winWidth;
    if (top + winHeight > work.bottom) top = work.bottom - winHeight;

    HINSTANCE hInstance = ::GetModuleHandleW(L"LeftClickMenu.dll");
    if (hInstance == nullptr) hInstance = ::GetModuleHandleW(nullptr);

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = &MenuPanel::WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kPanelClassName;
        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            Logger::Instance().Error(L"MenuPanel::Create: 注册面板窗口类失败, 错误码="
                + std::to_wstring(::GetLastError()));
            return false;
        }
        classRegistered = true;
    }

    m_hWnd = ::CreateWindowExW(exStyle, kPanelClassName,
        kUseSystemCaption ? kPanelTitle : L"", style, left, top, winWidth, winHeight,
        hOwner, nullptr, hInstance, this);
    if (m_hWnd == nullptr)
    {
        Logger::Instance().Error(L"MenuPanel::Create: 创建面板失败, 错误码="
            + std::to_wstring(::GetLastError()));
        return false;
    }

    if (!kUseSystemCaption)
    {
        int radius = ::MulDiv(8, m_dpi, 96);
        HRGN hRegion = ::CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, radius, radius);
        if (hRegion != nullptr)
        {
            ::SetWindowRgn(m_hWnd, hRegion, TRUE);
        }
    }

    m_result.tileMode = m_tileMode;

    Logger::Instance().Info(L"MenuPanel: 显示面板, 菜单项="
        + std::to_wstring(m_items->size()) + L", 平铺="
        + (m_tileMode ? L"是" : L"否") + L", 尺寸="
        + std::to_wstring(winWidth) + L"x" + std::to_wstring(winHeight));
    return true;
}

void MenuPanel::UpdateTileMetrics()
{
    // 平铺模式下面板宽度至少容纳图标和左右留白,不足时自动加宽
    const int minCellWidth = m_tileIconSize + ::MulDiv(16, m_dpi, 96);
    const int neededWidth = minCellWidth * m_tileColumns
        + m_tileGap * (m_tileColumns - 1) + m_paddingX * 2;
    if (m_tileMode && m_width < neededWidth
        && (m_maxPanelWidth <= 0 || neededWidth <= m_maxPanelWidth))
    {
        m_width = neededWidth;
    }

    int contentWidth = m_width - m_paddingX * 2;
    if (contentWidth < 1) contentWidth = 1;

    int totalGap = m_tileGap * (m_tileColumns - 1);
    m_tileCellWidth = (contentWidth - totalGap) / m_tileColumns;
    if (m_tileCellWidth < 1) m_tileCellWidth = 1;
}

int MenuPanel::CalculateContentHeight() const
{
    if (m_tileMode)
    {
        int count = 0;
        if (m_items != nullptr)
        {
            for (const auto& item : *m_items)
            {
                if (!item.isSeparator) ++count;
            }
        }
        int rows = (count + m_tileColumns - 1) / m_tileColumns;
        return m_paddingY * 2 + rows * m_tileCellHeight;
    }

    int height = m_paddingY * 2;
    if (m_items != nullptr)
    {
        for (const auto& item : *m_items)
        {
            height += item.isSeparator ? m_sepHeight : m_rowHeight;
        }
    }
    return height;
}

void MenuPanel::UpdateLayoutAfterModeChange()
{
    if (m_hWnd == nullptr || !::IsWindow(m_hWnd)) return;

    // 恢复配置宽度,平铺模式再按需要自动加宽
    const int minConfiguredWidth = ::MulDiv(160, m_dpi, 96);
    m_width = ::MulDiv(m_configuredWidth, m_dpi, 96);
    if (m_width < minConfiguredWidth) m_width = minConfiguredWidth;
    if (m_maxPanelWidth > 0 && m_width > m_maxPanelWidth) m_width = m_maxPanelWidth;

    UpdateTileMetrics();
    m_contentHeight = CalculateContentHeight();

    const bool empty = IsEmptyMenu(*m_items);
    const int emptyViewHeight = kUseSystemCaption
        ? ::MulDiv(8, m_dpi, 96)
        : ::MulDiv(40, m_dpi, 96);

    if (empty)
    {
        m_viewHeight = MaxInt(0, MinInt(emptyViewHeight, m_maxBodyHeight));
        m_contentHeight = m_viewHeight;
    }
    else
    {
        m_viewHeight = MinInt(m_contentHeight, m_maxBodyHeight);
    }
    m_scroll = 0;

    const int headerHeight = kUseSystemCaption ? m_gearRowHeight : m_headerHeight;
    int clientHeight = headerHeight + m_viewHeight;
    m_height = clientHeight;
    m_bodyTop = headerHeight;

    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    if (kUseSystemCaption)
    {
        style |= WS_CAPTION | WS_SYSMENU;
    }

    RECT winRect = { 0, 0, m_width, clientHeight };
    AdjustWindowRectForDpi(&winRect, style, FALSE, exStyle, m_dpi);
    int winWidth = winRect.right - winRect.left;
    int winHeight = winRect.bottom - winRect.top;

    POINT pt = { m_screenX, m_screenY };
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
    HMONITOR hMonitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    ::GetMonitorInfoW(hMonitor, &monitorInfo);
    const RECT work = monitorInfo.rcWork;
    const int margin = ::MulDiv(8, m_dpi, 96);

    int left = m_screenX + margin;
    int top = m_screenY + margin;
    if (left + winWidth > work.right) left = m_screenX - winWidth - margin;
    if (top + winHeight > work.bottom) top = m_screenY - winHeight - margin;
    if (left < work.left) left = work.left;
    if (top < work.top) top = work.top;
    if (left + winWidth > work.right) left = work.right - winWidth;
    if (top + winHeight > work.bottom) top = work.bottom - winHeight;

    ::SetWindowPos(m_hWnd, nullptr, left, top, winWidth, winHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    if (!kUseSystemCaption)
    {
        int radius = ::MulDiv(8, m_dpi, 96);
        HRGN hRegion = ::CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, radius, radius);
        if (hRegion != nullptr)
        {
            ::SetWindowRgn(m_hWnd, hRegion, TRUE);
        }
    }

    m_result.tileMode = m_tileMode;
    Invalidate();
}

void MenuPanel::ToggleViewMode()
{
    m_tileMode = !m_tileMode;
    m_hoverItem = -1;
    m_selectedItem = -1;
    m_result.viewModeChanged = true;
    m_result.tileMode = m_tileMode;

    Logger::Instance().Info(L"MenuPanel: 切换显示模式为"
        + std::wstring(m_tileMode ? L"平铺" : L"列表"));

    UpdateLayoutAfterModeChange();
}

int MenuPanel::TilePositionToItemIndex(int position) const
{
    if (m_items == nullptr || position < 0) return -1;

    int tilePos = 0;
    for (size_t i = 0; i < m_items->size(); ++i)
    {
        if ((*m_items)[i].isSeparator) continue;
        if (tilePos == position) return static_cast<int>(i);
        ++tilePos;
    }
    return -1;
}

int MenuPanel::ItemIndexToTilePosition(int index) const
{
    if (m_items == nullptr || index < 0 || index >= static_cast<int>(m_items->size()))
        return -1;

    int tilePos = 0;
    for (int i = 0; i < static_cast<int>(m_items->size()); ++i)
    {
        if ((*m_items)[i].isSeparator) continue;
        if (i == index) return tilePos;
        ++tilePos;
    }
    return -1;
}

void MenuPanel::RunMessageLoop()
{
    ::ShowWindow(m_hWnd, SW_SHOW);
    ::SetForegroundWindow(m_hWnd);
    ::SetFocus(m_hWnd);
    ::SetCapture(m_hWnd);

    MSG msg;
    while (m_running)
    {
        int result = ::GetMessageW(&msg, nullptr, 0, 0);
        if (result == 0 || result == -1) break;
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    if (::GetCapture() == m_hWnd)
    {
        ::ReleaseCapture();
    }
    if (m_hWnd != nullptr && ::IsWindow(m_hWnd))
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void MenuPanel::Close()
{
    m_running = false;
    if (::GetCapture() == m_hWnd)
    {
        ::ReleaseCapture();
    }
    if (m_hWnd != nullptr && ::IsWindow(m_hWnd))
    {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

LRESULT CALLBACK MenuPanel::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MenuPanel* self = reinterpret_cast<MenuPanel*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    if (self == nullptr)
    {
        return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg)
    {
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;

    case WM_ACTIVATE:
    {
        WORD state = LOWORD(wParam);
        if (state == WA_INACTIVE)
        {
            if (self->m_activated) self->Close();
        }
        else
        {
            self->m_activated = true;
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps = { 0 };
        HDC hdc = ::BeginPaint(hWnd, &ps);
        self->Paint(hdc);
        ::EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
    {
        POINT pt = { 0, 0 };
        ::GetCursorPos(&pt);
        ::ScreenToClient(hWnd, &pt);
        if (self->PointInToggle(pt) || self->PointInGear(pt) || self->PointInClose(pt))
        {
            ::SetCursor(::LoadCursorW(nullptr, IDC_HAND));
        }
        else
        {
            ::SetCursor(::LoadCursorW(nullptr, IDC_ARROW));
        }
        return TRUE;
    }

    case WM_MOUSEMOVE:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        self->UpdateHover(pt);
        if (!self->m_trackingLeave)
        {
            TRACKMOUSEEVENT tme = { 0 };
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            if (::TrackMouseEvent(&tme)) self->m_trackingLeave = true;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        self->m_trackingLeave = false;
        if (self->m_hoverItem != -1 || self->m_hoverToggle
            || self->m_hoverGear || self->m_hoverClose)
        {
            self->m_hoverItem = -1;
            self->m_hoverToggle = false;
            self->m_hoverGear = false;
            self->m_hoverClose = false;
            self->Invalidate();
        }
        return 0;

    case WM_LBUTTONDOWN:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT clientRect = { 0, 0, 0, 0 };
        ::GetClientRect(hWnd, &clientRect);
        if (!::PtInRect(&clientRect, pt))
        {
            // 点击面板外部,直接关闭
            self->Close();
            return 0;
        }

        self->m_mouseDown = true;
        self->m_mouseDownPoint = pt;
        self->m_pressedToggle = self->PointInToggle(pt);
        self->m_pressedGear = !self->m_pressedToggle && self->PointInGear(pt);
        self->m_pressedClose = !self->m_pressedToggle && !self->m_pressedGear
            && self->PointInClose(pt);
        self->m_pressedItem = (!self->m_pressedToggle && !self->m_pressedGear
            && !self->m_pressedClose) ? self->GetItemAtPoint(pt) : -1;
        self->UpdateHover(pt);
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (!self->m_mouseDown) return 0;
        self->m_mouseDown = false;

        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT clientRect = { 0, 0, 0, 0 };
        ::GetClientRect(hWnd, &clientRect);
        if (!::PtInRect(&clientRect, pt))
        {
            self->m_pressedItem = -1;
            self->m_pressedToggle = false;
            self->m_pressedGear = false;
            self->m_pressedClose = false;
            self->Close();
            return 0;
        }

        self->UpdateHover(pt);
        if (self->m_pressedToggle && self->PointInToggle(pt))
        {
            self->m_pressedToggle = false;
            self->ToggleViewMode();
            return 0;
        }
        if (self->m_pressedGear && self->PointInGear(pt))
        {
            self->m_result.editRequested = true;
            self->m_pressedGear = false;
            self->Close();
            return 0;
        }
        if (self->m_pressedClose && self->PointInClose(pt))
        {
            self->m_pressedClose = false;
            self->Close();
            return 0;
        }
        if (self->m_pressedItem >= 0 && self->GetItemAtPoint(pt) == self->m_pressedItem)
        {
            int item = self->m_pressedItem;
            self->m_pressedItem = -1;
            self->ActivateItem(item);
            return 0;
        }

        self->m_pressedItem = -1;
        self->m_pressedToggle = false;
        self->m_pressedGear = false;
        self->m_pressedClose = false;
        return 0;
    }

    case WM_RBUTTONDOWN:
        self->Close();
        return 0;

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            self->Close();
            return 0;
        }
        // 列表模式上下移动;平铺模式上下移动一行,左右移动一个格子
        int delta = 0;
        if (wParam == VK_UP)
        {
            delta = self->m_tileMode ? -self->m_tileColumns : -1;
        }
        else if (wParam == VK_DOWN)
        {
            delta = self->m_tileMode ? self->m_tileColumns : 1;
        }
        else if (self->m_tileMode && wParam == VK_LEFT)
        {
            delta = -1;
        }
        else if (self->m_tileMode && wParam == VK_RIGHT)
        {
            delta = 1;
        }

        if (delta != 0)
        {
            self->m_hoverItem = -1;
            self->m_hoverToggle = false;
            self->m_hoverGear = false;
            self->m_hoverClose = false;
            self->m_selectedItem = self->FindNextItem(self->m_selectedItem, delta);
            self->EnsureItemVisible(self->m_selectedItem);
            self->Invalidate();
            return 0;
        }
        if (wParam == VK_RETURN)
        {
            if (self->m_selectedItem >= 0)
            {
                self->ActivateItem(self->m_selectedItem);
            }
            else
            {
                int first = self->FindNextItem(-1, 1);
                if (first >= 0) self->ActivateItem(first);
            }
            return 0;
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
        self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_CLOSE:
        self->Close();
        return 0;

    case WM_DESTROY:
        self->m_hWnd = nullptr;
        self->m_running = false;
        return 0;

    case WM_NCDESTROY:
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }

    return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void MenuPanel::Paint(HDC hdc)
{
    RECT clientRect = { 0, 0, 0, 0 };
    ::GetClientRect(m_hWnd, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;

    HDC memDc = ::CreateCompatibleDC(hdc);
    HBITMAP memBitmap = ::CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = ::SelectObject(memDc, memBitmap);

    HBRUSH bgBrush = ::CreateSolidBrush(m_bgColor);
    ::FillRect(memDc, &clientRect, bgBrush);
    ::DeleteObject(bgBrush);
    ::SetBkMode(memDc, TRANSPARENT);

    DrawHeader(memDc);
    DrawItems(memDc);

    ::BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);

    ::SelectObject(memDc, oldBitmap);
    ::DeleteObject(memBitmap);
    ::DeleteDC(memDc);
}

void MenuPanel::DrawHeader(HDC hdc)
{
    int radius = ::MulDiv(4, m_dpi, 96);

    if (kUseSystemCaption)
    {
        // 方案 A:系统标题栏自带关闭按钮,这里只画切换按钮和齿轮
        RECT toggleRect = GetToggleRect();
        RECT gearRect = GetGearRect();
        if (m_hoverToggle)
        {
            FillRoundedRect(hdc, toggleRect, m_hoverColor, radius);
        }
        if (m_tileMode)
        {
            DrawListIcon(hdc, toggleRect, m_buttonColor);
        }
        else
        {
            DrawTileIcon(hdc, toggleRect, m_buttonColor);
        }
        if (m_hoverGear)
        {
            FillRoundedRect(hdc, gearRect, m_hoverColor, radius);
        }
        DrawSlidersIcon(hdc, gearRect, m_buttonColor);
        return;
    }

    // 方案 B:自绘标题栏
    RECT toggleRect = GetToggleRect();
    RECT gearRect = GetGearRect();
    RECT closeRect = GetCloseRect();

    RECT titleRect = { m_paddingX, 0, toggleRect.left - m_iconGap, m_headerHeight };
    if (titleRect.right > titleRect.left)
    {
        HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, m_titleFont));
        ::SetTextColor(hdc, m_textColor);
        ::DrawTextW(hdc, kPanelTitle, -1, &titleRect,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
        ::SelectObject(hdc, oldFont);
    }

    if (m_hoverToggle)
    {
        FillRoundedRect(hdc, toggleRect, m_hoverColor, radius);
    }
    if (m_tileMode)
    {
        DrawListIcon(hdc, toggleRect, m_buttonColor);
    }
    else
    {
        DrawTileIcon(hdc, toggleRect, m_buttonColor);
    }

    if (m_hoverGear)
    {
        FillRoundedRect(hdc, gearRect, m_hoverColor, radius);
    }
    DrawSlidersIcon(hdc, gearRect, m_buttonColor);

    if (m_hoverClose)
    {
        FillRoundedRect(hdc, closeRect, m_closeHoverColor, radius);
        DrawCloseIcon(hdc, closeRect, RGB(255, 255, 255));
    }
    else
    {
        DrawCloseIcon(hdc, closeRect, m_buttonColor);
    }

    HPEN pen = ::CreatePen(PS_SOLID, 1, m_dividerColor);
    HGDIOBJ oldPen = ::SelectObject(hdc, pen);
    ::MoveToEx(hdc, 0, m_headerHeight - 1, nullptr);
    ::LineTo(hdc, m_width, m_headerHeight - 1);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);
}

void MenuPanel::DrawItems(HDC hdc)
{
    if (m_items == nullptr || m_items->empty() || m_viewHeight <= 0)
    {
        return;
    }

    if (m_tileMode)
    {
        DrawTileItems(hdc);
        return;
    }

    int saved = ::SaveDC(hdc);
    ::IntersectClipRect(hdc, 0, m_bodyTop, m_width, m_bodyTop + m_viewHeight);

    int y = m_bodyTop + m_paddingY - m_scroll;
    for (size_t i = 0; i < m_items->size(); ++i)
    {
        const MenuConfigItem& item = (*m_items)[i];
        if (item.isSeparator)
        {
            RECT separatorRect = { m_paddingX, y, m_width - m_paddingX, y + m_sepHeight };
            DrawSeparator(hdc, separatorRect);
            y += m_sepHeight;
            continue;
        }

        int inset = ::MulDiv(4, m_dpi, 96);
        RECT rowRect = { inset, y, m_width - inset, y + m_rowHeight };
        DrawItemRow(hdc, static_cast<int>(i), rowRect,
            static_cast<int>(i) == m_hoverItem,
            static_cast<int>(i) == m_selectedItem);
        y += m_rowHeight;
    }

    ::RestoreDC(hdc, saved);
}

void MenuPanel::DrawItemRow(HDC hdc, int index, const RECT& rowRect, bool hovered, bool selected)
{
    const MenuConfigItem& item = (*m_items)[index];
    if ((hovered || selected) && item.enabled)
    {
        FillRoundedRect(hdc, rowRect, m_hoverColor, ::MulDiv(4, m_dpi, 96));
    }

    bool disabled = !item.enabled;
    RECT iconRect = { rowRect.left + m_paddingX,
        rowRect.top + (m_rowHeight - m_iconSize) / 2,
        rowRect.left + m_paddingX + m_iconSize,
        rowRect.top + (m_rowHeight - m_iconSize) / 2 + m_iconSize };
    DrawItemIcon(hdc, index, iconRect, disabled);

    RECT textRect = { iconRect.right + m_iconGap, rowRect.top,
        rowRect.right - m_paddingX, rowRect.bottom };
    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, m_font));
    ::SetTextColor(hdc, disabled ? m_disabledColor : m_textColor);
    ::DrawTextW(hdc, item.name.c_str(), -1, &textRect,
        DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    ::SelectObject(hdc, oldFont);
}

void MenuPanel::DrawTileItems(HDC hdc)
{
    int saved = ::SaveDC(hdc);
    ::IntersectClipRect(hdc, 0, m_bodyTop, m_width, m_bodyTop + m_viewHeight);

    int tilePos = 0;
    for (size_t i = 0; i < m_items->size(); ++i)
    {
        const MenuConfigItem& item = (*m_items)[i];
        // 平铺模式不显示分隔线
        if (item.isSeparator) continue;

        int row = tilePos / m_tileColumns;
        int column = tilePos % m_tileColumns;
        int x = m_paddingX + column * (m_tileCellWidth + m_tileGap);
        int y = m_bodyTop + m_paddingY - m_scroll + row * m_tileCellHeight;

        RECT cellRect = { x, y, x + m_tileCellWidth, y + m_tileCellHeight };
        DrawTileCell(hdc, static_cast<int>(i), cellRect,
            static_cast<int>(i) == m_hoverItem,
            static_cast<int>(i) == m_selectedItem);

        ++tilePos;
    }

    ::RestoreDC(hdc, saved);
}

void MenuPanel::DrawTileCell(HDC hdc, int index, const RECT& cellRect,
    bool hovered, bool selected)
{
    const MenuConfigItem& item = (*m_items)[index];
    bool disabled = !item.enabled;

    if ((hovered || selected) && item.enabled)
    {
        int inset = ::MulDiv(2, m_dpi, 96);
        RECT hoverRect = { cellRect.left + inset, cellRect.top + inset,
            cellRect.right - inset, cellRect.bottom - inset };
        FillRoundedRect(hdc, hoverRect, m_hoverColor, ::MulDiv(4, m_dpi, 96));
    }

    int cellWidth = cellRect.right - cellRect.left;
    int iconSize = MinInt(m_tileIconSize, cellWidth - ::MulDiv(8, m_dpi, 96));
    if (iconSize < ::MulDiv(8, m_dpi, 96)) iconSize = ::MulDiv(8, m_dpi, 96);

    int iconX = cellRect.left + (cellWidth - iconSize) / 2;
    int iconY = cellRect.top + ::MulDiv(6, m_dpi, 96);
    RECT iconRect = { iconX, iconY, iconX + iconSize, iconY + iconSize };
    DrawItemIcon(hdc, index, iconRect, disabled);

    int pad = ::MulDiv(2, m_dpi, 96);
    RECT textRect = { cellRect.left + pad, iconRect.bottom + pad,
        cellRect.right - pad, cellRect.bottom - ::MulDiv(3, m_dpi, 96) };

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, m_font));
    ::SetTextColor(hdc, disabled ? m_disabledColor : m_textColor);
    ::DrawTextW(hdc, item.name.c_str(), -1, &textRect,
        DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    ::SelectObject(hdc, oldFont);
}

void MenuPanel::DrawSeparator(HDC hdc, const RECT& rc)
{
    int y = (rc.top + rc.bottom) / 2;
    HPEN pen = ::CreatePen(PS_SOLID, 1, m_dividerColor);
    HGDIOBJ oldPen = ::SelectObject(hdc, pen);
    ::MoveToEx(hdc, rc.left, y, nullptr);
    ::LineTo(hdc, rc.right, y);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);
}

void MenuPanel::DrawSlidersIcon(HDC hdc, const RECT& rc, COLORREF color)
{
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int inset = ::MulDiv(5, m_dpi, 96);
    int knobRadius = ::MulDiv(2, m_dpi, 96);
    int x0 = rc.left + inset;
    int x1 = rc.right - inset;
    int y1 = rc.top + height / 4;
    int y2 = rc.top + height / 2;
    int y3 = rc.top + height * 3 / 4;

    HPEN pen = ::CreatePen(PS_SOLID, MaxInt(1, ::MulDiv(1, m_dpi, 96)), color);
    HGDIOBJ oldPen = ::SelectObject(hdc, pen);
    ::MoveToEx(hdc, x0, y1, nullptr);
    ::LineTo(hdc, x1, y1);
    ::MoveToEx(hdc, x0, y2, nullptr);
    ::LineTo(hdc, x1, y2);
    ::MoveToEx(hdc, x0, y3, nullptr);
    ::LineTo(hdc, x1, y3);

    HBRUSH brush = ::CreateSolidBrush(color);
    HGDIOBJ oldBrush = ::SelectObject(hdc, brush);
    int knob1 = rc.left + width * 45 / 100;
    int knob2 = rc.left + width * 70 / 100;
    int knob3 = rc.left + width * 35 / 100;
    ::Ellipse(hdc, knob1 - knobRadius, y1 - knobRadius, knob1 + knobRadius, y1 + knobRadius);
    ::Ellipse(hdc, knob2 - knobRadius, y2 - knobRadius, knob2 + knobRadius, y2 + knobRadius);
    ::Ellipse(hdc, knob3 - knobRadius, y3 - knobRadius, knob3 + knobRadius, y3 + knobRadius);

    ::SelectObject(hdc, oldBrush);
    ::DeleteObject(brush);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);
}

void MenuPanel::DrawTileIcon(HDC hdc, const RECT& rc, COLORREF color)
{
    // 2x2 方块图标,表示平铺视图
    int inset = ::MulDiv(7, m_dpi, 96);
    int gap = ::MulDiv(2, m_dpi, 96);
    int size = (rc.right - rc.left - inset * 2 - gap) / 2;
    if (size < 1) size = 1;

    int left = rc.left + inset;
    int top = rc.top + (rc.bottom - rc.top - size * 2 - gap) / 2;

    HBRUSH brush = ::CreateSolidBrush(color);
    int xs[2] = { left, left + size + gap };
    int ys[2] = { top, top + size + gap };
    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 2; ++col)
        {
            RECT block = { xs[col], ys[row], xs[col] + size, ys[row] + size };
            ::FillRect(hdc, &block, brush);
        }
    }
    ::DeleteObject(brush);
}

void MenuPanel::DrawListIcon(HDC hdc, const RECT& rc, COLORREF color)
{
    // 三条横线图标,表示列表视图
    int inset = ::MulDiv(7, m_dpi, 96);
    int bullet = ::MulDiv(3, m_dpi, 96);
    int lineHeight = MaxInt(1, ::MulDiv(1, m_dpi, 96));
    int left = rc.left + inset;
    int right = rc.right - inset;
    int width = rc.bottom - rc.top;

    HPEN pen = ::CreatePen(PS_SOLID, lineHeight, color);
    HGDIOBJ oldPen = ::SelectObject(hdc, pen);
    HBRUSH brush = ::CreateSolidBrush(color);
    HGDIOBJ oldBrush = ::SelectObject(hdc, brush);

    for (int i = 0; i < 3; ++i)
    {
        int y = rc.top + width * (i + 1) / 4;
        RECT dot = { left, y - bullet / 2, left + bullet, y - bullet / 2 + bullet };
        ::FillRect(hdc, &dot, brush);
        ::MoveToEx(hdc, left + bullet + ::MulDiv(3, m_dpi, 96), y, nullptr);
        ::LineTo(hdc, right, y);
    }

    ::SelectObject(hdc, oldBrush);
    ::DeleteObject(brush);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);
}

void MenuPanel::DrawCloseIcon(HDC hdc, const RECT& rc, COLORREF color)
{
    int inset = ::MulDiv(7, m_dpi, 96);
    HPEN pen = ::CreatePen(PS_SOLID, MaxInt(1, ::MulDiv(1, m_dpi, 96)), color);
    HGDIOBJ oldPen = ::SelectObject(hdc, pen);
    ::MoveToEx(hdc, rc.left + inset, rc.top + inset, nullptr);
    ::LineTo(hdc, rc.right - inset, rc.bottom - inset);
    ::MoveToEx(hdc, rc.right - inset, rc.top + inset, nullptr);
    ::LineTo(hdc, rc.left + inset, rc.bottom - inset);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(pen);
}

void MenuPanel::DrawItemIcon(HDC hdc, int index, const RECT& rc, bool disabled)
{
    const MenuConfigItem& item = (*m_items)[index];
    HICON hIcon = IconManager::Instance().GetIcon(item.iconSource, item.iconIndex);
    if (hIcon == nullptr) return;

    if (disabled)
    {
        ::DrawStateW(hdc, nullptr, nullptr, reinterpret_cast<LPARAM>(hIcon), 0,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            DST_ICON | DSS_DISABLED);
    }
    else
    {
        ::DrawIconEx(hdc, rc.left, rc.top, hIcon, rc.right - rc.left, rc.bottom - rc.top,
            0, nullptr, DI_NORMAL);
    }
}

RECT MenuPanel::GetToggleRect() const
{
    int buttonSize = ::MulDiv(28, m_dpi, 96);
    int buttonGap = ::MulDiv(4, m_dpi, 96);
    RECT gearRect = GetGearRect();

    RECT rc = { 0, 0, 0, 0 };
    rc.left = gearRect.left - buttonGap - buttonSize;
    rc.top = gearRect.top;
    rc.right = rc.left + buttonSize;
    rc.bottom = rc.top + buttonSize;
    return rc;
}

RECT MenuPanel::GetGearRect() const
{
    int buttonSize = ::MulDiv(28, m_dpi, 96);
    int padRight = ::MulDiv(8, m_dpi, 96);
    RECT rc = { 0, 0, 0, 0 };
    if (kUseSystemCaption)
    {
        rc.left = m_width - padRight - buttonSize;
        rc.top = ::MulDiv(4, m_dpi, 96);
    }
    else
    {
        int buttonGap = ::MulDiv(4, m_dpi, 96);
        rc.left = m_width - padRight - buttonSize * 2 - buttonGap;
        rc.top = (m_headerHeight - buttonSize) / 2;
    }
    rc.right = rc.left + buttonSize;
    rc.bottom = rc.top + buttonSize;
    return rc;
}

RECT MenuPanel::GetCloseRect() const
{
    RECT rc = { 0, 0, 0, 0 };
    if (kUseSystemCaption) return rc;

    int buttonSize = ::MulDiv(28, m_dpi, 96);
    int padRight = ::MulDiv(8, m_dpi, 96);
    rc.left = m_width - padRight - buttonSize;
    rc.top = (m_headerHeight - buttonSize) / 2;
    rc.right = rc.left + buttonSize;
    rc.bottom = rc.top + buttonSize;
    return rc;
}

bool MenuPanel::PointInToggle(POINT pt) const
{
    RECT rc = GetToggleRect();
    return ::PtInRect(&rc, pt) != FALSE;
}

bool MenuPanel::PointInGear(POINT pt) const
{
    RECT rc = GetGearRect();
    return ::PtInRect(&rc, pt) != FALSE;
}

bool MenuPanel::PointInClose(POINT pt) const
{
    RECT rc = GetCloseRect();
    return rc.right > rc.left && ::PtInRect(&rc, pt) != FALSE;
}

int MenuPanel::GetItemAtPoint(POINT pt) const
{
    if (m_items == nullptr) return -1;
    if (pt.y < m_bodyTop || pt.y >= m_bodyTop + m_viewHeight) return -1;
    if (pt.x < 0 || pt.x >= m_width) return -1;

    if (m_tileMode)
    {
        int relY = pt.y - (m_bodyTop + m_paddingY - m_scroll);
        if (relY < 0) return -1;

        int row = relY / m_tileCellHeight;
        int relX = pt.x - m_paddingX;
        if (relX < 0) return -1;

        int step = m_tileCellWidth + m_tileGap;
        int column = relX / step;
        if (column < 0 || column >= m_tileColumns) return -1;
        if (relX % step >= m_tileCellWidth) return -1;

        return TilePositionToItemIndex(row * m_tileColumns + column);
    }

    int y = m_bodyTop + m_paddingY - m_scroll;
    for (size_t i = 0; i < m_items->size(); ++i)
    {
        const MenuConfigItem& item = (*m_items)[i];
        if (item.isSeparator)
        {
            y += m_sepHeight;
            continue;
        }
        if (pt.y >= y && pt.y < y + m_rowHeight) return static_cast<int>(i);
        y += m_rowHeight;
    }
    return -1;
}

bool MenuPanel::IsItemSelectable(int index) const
{
    if (m_items == nullptr) return false;
    if (index < 0 || index >= static_cast<int>(m_items->size())) return false;
    const MenuConfigItem& item = (*m_items)[index];
    return !item.isSeparator && item.enabled;
}

int MenuPanel::FindNextItem(int from, int delta) const
{
    if (m_items == nullptr) return -1;

    // 只在可选中的项之间移动,平铺模式下分隔线已被跳过
    std::vector<int> selectable;
    for (size_t i = 0; i < m_items->size(); ++i)
    {
        if (IsItemSelectable(static_cast<int>(i))) selectable.push_back(static_cast<int>(i));
    }
    if (selectable.empty()) return -1;

    int position = -1;
    if (from >= 0)
    {
        for (size_t i = 0; i < selectable.size(); ++i)
        {
            if (selectable[i] == from)
            {
                position = static_cast<int>(i);
                break;
            }
        }
    }

    if (position < 0)
    {
        return delta >= 0 ? selectable.front() : selectable.back();
    }

    int count = static_cast<int>(selectable.size());
    int next = position + delta;
    next %= count;
    if (next < 0) next += count;
    return selectable[next];
}

void MenuPanel::EnsureItemVisible(int index)
{
    if (index < 0 || m_items == nullptr) return;

    if (m_tileMode)
    {
        int position = ItemIndexToTilePosition(index);
        if (position < 0) return;

        int top = m_paddingY + (position / m_tileColumns) * m_tileCellHeight;
        int bottom = top + m_tileCellHeight;

        int maxScroll = MaxScroll();
        if (top < m_scroll)
        {
            m_scroll = top;
        }
        else if (bottom > m_scroll + m_viewHeight)
        {
            m_scroll = bottom - m_viewHeight;
        }
        if (m_scroll < 0) m_scroll = 0;
        if (m_scroll > maxScroll) m_scroll = maxScroll;
        return;
    }

    int top = m_paddingY;
    for (int i = 0; i < index; ++i)
    {
        top += (*m_items)[i].isSeparator ? m_sepHeight : m_rowHeight;
    }
    int bottom = top + m_rowHeight;

    int maxScroll = MaxScroll();
    if (top < m_scroll)
    {
        m_scroll = top;
    }
    else if (bottom > m_scroll + m_viewHeight)
    {
        m_scroll = bottom - m_viewHeight;
    }
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > maxScroll) m_scroll = maxScroll;
}

void MenuPanel::UpdateHover(POINT pt)
{
    bool toggle = PointInToggle(pt);
    bool gear = !toggle && PointInGear(pt);
    bool close = !toggle && !gear && PointInClose(pt);
    int item = (toggle || gear || close) ? -1 : GetItemAtPoint(pt);

    if (toggle != m_hoverToggle || gear != m_hoverGear
        || close != m_hoverClose || item != m_hoverItem)
    {
        m_hoverToggle = toggle;
        m_hoverGear = gear;
        m_hoverClose = close;
        m_hoverItem = item;
        if (item >= 0)
        {
            m_selectedItem = -1;    // 鼠标悬停时取消键盘选中高亮
        }
        Invalidate();
    }
}

void MenuPanel::Invalidate() const
{
    if (m_hWnd != nullptr && ::IsWindow(m_hWnd))
    {
        ::InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

int MenuPanel::MaxScroll() const
{
    if (m_items == nullptr) return 0;
    return MaxInt(0, m_contentHeight - m_viewHeight);
}

void MenuPanel::OnMouseWheel(int delta)
{
    int maxScroll = MaxScroll();
    if (maxScroll <= 0) return;

    int step = m_tileMode ? m_tileCellHeight * 2 : m_rowHeight * 3;
    if (delta > 0)
    {
        m_scroll -= step;
    }
    else
    {
        m_scroll += step;
    }
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > maxScroll) m_scroll = maxScroll;
    Invalidate();
}

void MenuPanel::ActivateItem(int index)
{
    if (!IsItemSelectable(index)) return;
    m_result.selectedIndex = index;
    Close();
}
