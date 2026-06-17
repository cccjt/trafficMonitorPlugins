#pragma once
#include "PluginInterface.h"
#include <string>

class HotkeyPlugin;

// 显示项目类:在任务栏/主窗口显示热键状态
class HotkeyItem : public IPluginItem
{
public:
    HotkeyItem();
    ~HotkeyItem();

    // 设置所属的主插件(用于获取数据)
    void SetOwner(HotkeyPlugin* owner) { m_owner = owner; }

    // IPluginItem 接口实现
    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    bool IsCustomDraw() const override { return false; }
    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

private:
    HotkeyPlugin* m_owner = nullptr;
    mutable std::wstring m_valueText;   // 缓存显示文本
};
