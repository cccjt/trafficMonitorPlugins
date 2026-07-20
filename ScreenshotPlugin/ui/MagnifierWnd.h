#pragma once
#include <Windows.h>

// 放大镜取色窗口
// 跟随鼠标显示像素放大网格 + 坐标 + 颜色值
class MagnifierWnd
{
public:
    MagnifierWnd();
    ~MagnifierWnd();

    // 创建窗口
    bool Create();

    // 销毁窗口
    void Destroy();

    // 显示/隐藏
    void Show();
    void Hide();

    // 更新显示:传入鼠标在屏幕上的位置(虚拟屏坐标)
    void Update(int screenX, int screenY);

    // 设置底图(用于读取像素颜色)
    void SetSource(HBITMAP hSource, int srcX, int srcY);

    // 处理快捷键
    //   copyColor: 按 C 时调用,复制当前颜色
    void CopyCurrentColor();

    // 切换颜色格式(Shift 键)
    void ToggleColorFormat();

private:
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 绘制
    void OnPaint();

    // 计算窗口位置(跟随鼠标,自动避开屏幕边缘)
    void UpdateWindowPosition(int screenX, int screenY);

    // 获取当前中心像素颜色
    COLORREF GetCenterColor();

    // 获取当前颜色字符串(根据当前格式)
    std::wstring GetCurrentColorString(COLORREF color);

    // 在屏幕坐标系中读取源图某像素颜色
    COLORREF GetSourcePixel(int screenX, int screenY);

private:
    HWND m_hWnd = nullptr;
    HBITMAP m_hSource = nullptr;     // 底图(完整屏幕截图,所有权在外部)
    int m_srcX = 0;                   // 底图在虚拟屏的起始 X
    int m_srcY = 0;                   // 底图在虚拟屏的起始 Y

    int m_curX = 0;                   // 当前鼠标 X(虚拟屏坐标)
    int m_curY = 0;                   // 当前鼠标 Y

    // 颜色格式
    enum class ColorFormat { HEX, RGB, CMYK };
    ColorFormat m_format = ColorFormat::HEX;

    // 窗口尺寸
    static constexpr int WINDOW_W = 130;
    static constexpr int WINDOW_H = 140;
    static constexpr int GRID_SIZE = 9;        // 9x9 像素网格
    static constexpr int GRID_PIXEL = 12;      // 每个像素放大到 12x12
    static constexpr int GRID_TOTAL = GRID_SIZE * GRID_PIXEL;
};
