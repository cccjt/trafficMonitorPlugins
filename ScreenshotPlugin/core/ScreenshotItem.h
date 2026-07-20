#pragma once
#include "PluginInterface.h"
#include <atomic>

// 截图插件显示项:在 TM 中显示当前贴图数量
class ScreenshotItem : public IPluginItem
{
public:
    ScreenshotItem();

    const wchar_t* GetItemName() const override { return L"截图工具"; }
    const wchar_t* GetItemId() const override { return L"screenshot_pin_count"; }
    const wchar_t* GetItemLableText() const override { return L"贴图"; }
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override { return L"0"; }

    bool IsCustomDraw() const override { return false; }
    int GetItemWidth() const override { return 0; }
    int GetItemWidthEx(void* hDC) const override;

    // 增减贴图计数(PinWindow 创建/销毁时调用)
    void Increment() { m_pinCount++; }
    void Decrement() { if (m_pinCount > 0) m_pinCount--; }

    int GetCount() const { return m_pinCount.load(); }

private:
    std::atomic<int> m_pinCount{ 0 };
    mutable wchar_t m_valueBuf[16]{ 0 };
};
