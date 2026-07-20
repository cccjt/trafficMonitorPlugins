#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
#include <algorithm>
#include "../config/ActionType.h"

class SelectionOverlay;
class SelectionToolbar;
class PinWindow;

// 截图核心控制器
// 负责:协调选区覆盖层、工具条、动作派发、贴图窗口管理
class ScreenshotCore
{
public:
    static ScreenshotCore& Instance();

    // 启动一次截图流程
    //   action: 触发动作
    //   hBmp:   完整屏幕截图(所有权转移)
    void Start(ActionType action, HBITMAP hBmp);

    // 执行动作(由工具条按钮/双击/右键调用)
    //   action:    要执行的动作
    //   overlay:   当前选区覆盖层
    //   sourceAction: 触发该次截图的原始动作(可选)
    void DoAction(ActionType action, SelectionOverlay* overlay, ActionType sourceAction);

    // 获取当前选区覆盖层
    SelectionOverlay* GetCurrentOverlay() const { return m_currentOverlay.get(); }

    // 注册一个贴图窗口(创建时调用)
    void RegisterPinWindow(PinWindow* p);
    // 注销一个贴图窗口(关闭时调用)
    void UnregisterPinWindow(PinWindow* p);

    // 获取当前贴图窗口数量
    int GetPinCount() const { return static_cast<int>(m_pinWindows.size()); }

private:
    ScreenshotCore();
    ~ScreenshotCore();

    // 获取最终位图:从屏幕截图裁剪选区 + 合并标注层
    HBITMAP GetFinalBitmap(SelectionOverlay* overlay);

    // 关闭选区覆盖层
    void CloseOverlay();

private:
    std::unique_ptr<SelectionOverlay> m_currentOverlay;
    std::unique_ptr<SelectionToolbar> m_currentToolbar;

    // 当前活跃的贴图窗口列表
    std::vector<PinWindow*> m_pinWindows;
};
