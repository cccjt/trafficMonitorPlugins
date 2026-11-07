#pragma once
#include <Windows.h>
#include <vector>
#include "MenuConfig.h"

// 面板显示设置(由配置传入)
struct MenuViewOptions
{
    int panelWidth = 200;       // 96 DPI 下的面板宽度像素
    bool tileMode = false;      // 是否使用平铺显示
    int tileColumns = 4;        // 平铺模式每行列数(2~8)
};

// 自绘菜单面板:替代系统 TrackPopupMenu
// 支持标题栏(显示模式切换 + 关闭按钮)、菜单项图标、悬停高亮、键盘选择、点击外部关闭
class MenuPanel
{
public:
    struct Result
    {
        int selectedIndex = -1;         // 选中的菜单项索引,未选择为 -1
        bool viewModeChanged = false;   // 是否点击了列表/平铺切换按钮
        bool tileMode = false;          // 切换后的显示模式
    };

    // items: 菜单项配置
    // hOwner: 面板的 owner 窗口(任务栏窗口)
    // screenX/screenY: 弹出位置(屏幕坐标)
    // options: 面板宽度、显示模式与每行列数
    static Result Show(const std::vector<MenuConfigItem>& items, HWND hOwner,
        int screenX, int screenY, const MenuViewOptions& options);

private:
    MenuPanel();
    ~MenuPanel();

    MenuPanel(const MenuPanel&) = delete;
    MenuPanel& operator=(const MenuPanel&) = delete;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    bool Create(HWND hOwner, int screenX, int screenY, const MenuViewOptions& options);
    void RunMessageLoop();
    void Close();

    // 绘制
    void Paint(HDC hdc);
    void DrawHeader(HDC hdc);
    void DrawItems(HDC hdc);
    void DrawTileItems(HDC hdc);
    void DrawItemRow(HDC hdc, int index, const RECT& rowRect, bool hovered, bool selected);
    void DrawTileCell(HDC hdc, int index, const RECT& cellRect, bool hovered, bool selected);
    void DrawSeparator(HDC hdc, const RECT& rc);
    void DrawTileIcon(HDC hdc, const RECT& rc, COLORREF color);
    void DrawListIcon(HDC hdc, const RECT& rc, COLORREF color);
    void DrawCloseIcon(HDC hdc, const RECT& rc, COLORREF color);
    void DrawItemIcon(HDC hdc, int index, const RECT& rc, bool disabled);

    // 布局与命中
    RECT GetToggleRect() const;
    RECT GetCloseRect() const;
    int GetItemAtPoint(POINT pt) const;
    bool PointInToggle(POINT pt) const;
    bool PointInClose(POINT pt) const;
    int FindNextItem(int from, int delta) const;
    void EnsureItemVisible(int index);
    void UpdateHover(POINT pt);
    void Invalidate() const;
    int MaxScroll() const;

    // 平铺布局辅助
    void UpdateTileMetrics();
    int CalculateContentHeight() const;
    void UpdateLayoutAfterModeChange();
    void ToggleViewMode();
    int TilePositionToItemIndex(int position) const;
    int ItemIndexToTilePosition(int index) const;

    void OnMouseWheel(int delta);
    void ActivateItem(int index);
    bool IsItemSelectable(int index) const;

private:
    const std::vector<MenuConfigItem>* m_items = nullptr;
    HWND m_hOwner = nullptr;
    HWND m_hWnd = nullptr;
    bool m_running = true;
    bool m_activated = false;
    bool m_mouseDown = false;
    POINT m_mouseDownPoint = { 0, 0 };
    int m_pressedItem = -1;
    bool m_pressedToggle = false;
    bool m_pressedClose = false;

    Result m_result;

    int m_hoverItem = -1;
    bool m_hoverToggle = false;
    bool m_hoverClose = false;
    bool m_trackingLeave = false;
    int m_selectedItem = -1;

    UINT m_dpi = 96;
    int m_width = 0;
    int m_height = 0;
    int m_headerHeight = 0;
    int m_toolbarHeight = 0;
    int m_rowHeight = 0;
    int m_sepHeight = 0;
    int m_paddingX = 0;
    int m_paddingY = 0;
    int m_iconSize = 0;
    int m_iconGap = 0;
    int m_bodyTop = 0;
    int m_viewHeight = 0;
    int m_contentHeight = 0;
    int m_scroll = 0;
    int m_screenX = 0;
    int m_screenY = 0;
    int m_maxBodyHeight = 0;
    int m_maxPanelWidth = 0;

    bool m_tileMode = false;
    int m_tileColumns = 4;
    int m_configuredWidth = 200;
    int m_tileGap = 0;
    int m_tileIconSize = 0;
    int m_tileCellWidth = 0;
    int m_tileCellHeight = 0;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;

    COLORREF m_bgColor = 0;
    COLORREF m_textColor = 0;
    COLORREF m_disabledColor = 0;
    COLORREF m_hoverColor = 0;
    COLORREF m_dividerColor = 0;
    COLORREF m_buttonColor = 0;
    COLORREF m_closeHoverColor = 0;
};
