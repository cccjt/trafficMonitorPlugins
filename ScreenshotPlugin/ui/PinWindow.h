#pragma once
#include <Windows.h>
#include <gdiplus.h>

// 贴图窗口
// - 显示一张截图,置顶浮窗
// - 发光边框:获取焦点=蓝色,未获取焦点=灰色
// - 滚轮调整图片透明度(边框不变)
// - 四角拖动缩放
// - 双击复制并关闭
class PinWindow
{
public:
    PinWindow();
    ~PinWindow();

    // 创建窗口
    //   hBmp: 截图位图(所有权转移给本对象)
    //   x, y: 窗口位置(屏幕坐标)
    bool Create(HBITMAP hBmp, int x, int y);

    // 销毁
    void Destroy();

    HWND GetHwnd() const { return m_hWnd; }

private:
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 消息处理
    void OnPaint();
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMouseMove(int x, int y);
    void OnLButtonDblClk(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnMouseWheel(int delta);
    void OnKeyDown(UINT vk);
    void OnActivate(WORD state);
    void OnSetFocus(HWND hWnd);
    void OnKillFocus(HWND hWnd);
    LRESULT OnNcHitTest(int x, int y);

    // 绘制(用于 UpdateLayeredWindow)
    void RenderToWindow();

    // 判断鼠标是否在四个角
    int HitTestCorner(int x, int y) const;

    // 计算窗口尺寸(根据图片尺寸 + 边框)
    void CalcWindowSize(int imgW, int imgH, int& winW, int& winH) const;

    // 重新计算窗口尺寸(根据 scale)
    void UpdateWindowSize();

    // 显示右键菜单
    void ShowContextMenu(int x, int y);

    // 关闭自己
    void Close();

private:
    HWND m_hWnd = nullptr;
    HBITMAP m_hBitmap = nullptr;        // 截图位图
    int m_imgW = 0;
    int m_imgH = 0;

    float m_scale = 1.0f;               // 缩放比例
    int m_alpha = 255;                  // 图片透明度(64~255)

    bool m_focused = true;              // 是否获取焦点

    // 拖动缩放状态
    bool m_dragging = false;
    int m_dragCorner = 0;               // 拖动的角(1-4)
    POINT m_dragStart = { 0, 0 };
    float m_dragStartScale = 1.0f;
    POINT m_dragStartPos = { 0, 0 };

    // 边框相关
    static constexpr int BORDER = 6;    // 边框宽度(发光区)
};
