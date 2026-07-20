#pragma once
#include <Windows.h>
#include <memory>
#include "../config/ActionType.h"
#include "MagnifierWnd.h"

// 前置声明
class ScreenshotCore;
class SelectionToolbar;

// 选区覆盖窗口
// - 全屏置顶分层窗口
// - 显示完整屏幕截图 + 暗化遮罩
// - 橡皮筋选区 + 8 调整手柄 + 坐标标签
// - 鼠标进入选区时显示放大镜
class SelectionOverlay
{
public:
    SelectionOverlay();
    ~SelectionOverlay();

    // 创建窗口
    //   hScreenBitmap: 完整屏幕截图(所有权转移给本对象)
    //   action:        触发该次截图的动作类型
    bool Create(HBITMAP hScreenBitmap, ActionType action);

    // 销毁
    void Destroy();

    // 关联工具条
    void SetToolbar(SelectionToolbar* toolbar) { m_toolbar = toolbar; }

    HWND GetHwnd() const { return m_hWnd; }

    // 获取当前选区(屏幕坐标)
    void GetSelection(RECT& out) const { out = m_rcSel; }

    // 设置选区(屏幕坐标)
    void SetSelection(const RECT& rc);

    // 从屏幕截图中裁剪出当前选区(返回新 HBITMAP,所有权转移)
    //   includeAnnotations: 是否合并标注层(v1 暂未实现标注,参数保留)
    HBITMAP CropSelection(bool includeAnnotations = true);

    // 外部调用:关闭覆盖层
    void Close();

private:
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // 消息处理
    void OnPaint();
    void OnLButtonDown(int x, int y);
    void OnMouseMove(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnLButtonDblClk(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnKeyDown(UINT vk);

    // 判断命中的手柄(返回 0 表示无,1-8 对应 8 个手柄)
    int HitTestHandle(int x, int y) const;

    // 判断点是否在选区内
    bool IsInSelection(int x, int y) const;

    // 更新放大镜
    void UpdateMagnifier(int x, int y);

    // 调整选区为正数宽高
    void NormalizeSelection();

    // 显示/隐藏工具条
    void ShowToolbarIfNeeded();

    // 获取选区相对屏幕的坐标
    void GetSelectionRect(RECT& rc) const;

private:
    HWND m_hWnd = nullptr;
    HBITMAP m_hScreenBitmap = nullptr;   // 完整屏幕截图
    int m_screenX = 0;                    // 屏幕截图的起始 X
    int m_screenY = 0;
    int m_screenW = 0;
    int m_screenH = 0;

    ActionType m_action = ActionType::Capture;

    // 选区(屏幕坐标)
    RECT m_rcSel = { 0, 0, 0, 0 };
    bool m_hasSelection = false;

    // 拖动状态
    enum class DragMode
    {
        None,
        NewSelection,    // 拖出新选区
        MoveSelection,   // 拖动整个选区
        ResizeHandle     // 拖动手柄调整大小
    };
    DragMode m_dragMode = DragMode::None;
    int m_resizeHandle = 0;       // 当前调整的手柄编号(1-8)
    POINT m_dragStart = { 0, 0 }; // 拖动起始点
    RECT m_dragOrigSel = { 0, 0, 0, 0 };   // 拖动前的选区

    // 放大镜
    std::unique_ptr<MagnifierWnd> m_magnifier;
    bool m_magnifierVisible = false;

    // 工具条
    SelectionToolbar* m_toolbar = nullptr;

    // 是否已确认选区(进入动作流程)
    bool m_confirmed = false;
};
