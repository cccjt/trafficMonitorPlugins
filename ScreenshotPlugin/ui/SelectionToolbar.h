#pragma once
#include <Windows.h>
#include <vector>

class SelectionOverlay;

// 选区下方的横向工具条
// 按钮: 贴图 / 保存 / 复制 / OCR / 翻译 / 撤销 / 重做 / 关闭
class SelectionToolbar
{
public:
    SelectionToolbar();
    ~SelectionToolbar();

    // 创建工具条
    bool Create(SelectionOverlay* overlay);

    // 销毁
    void Destroy();

    // 显示/隐藏
    void Show();
    void Hide();

    // 更新位置(根据选区位置自动定位到选区下方或上方)
    void UpdatePosition();

    HWND GetHwnd() const { return m_hWnd; }

private:
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnPaint();
    void OnLButtonDown(int x, int y);
    void OnMouseMove(int x, int y);

    // 按钮定义
    struct Button
    {
        int id;             // 按钮编号(1-N)
        const wchar_t* text;
        const wchar_t* tip;
        RECT rect;          // 绘制区域
        bool hover;
    };

    std::vector<Button> m_buttons;
    int m_hoveredBtn = 0;

    HWND m_hWnd = nullptr;
    SelectionOverlay* m_overlay = nullptr;

    static constexpr int TB_WIDTH = 380;
    static constexpr int TB_HEIGHT = 36;
};
