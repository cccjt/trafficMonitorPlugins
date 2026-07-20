#include "../pch/stdafx.h"
#include "ScreenshotItem.h"
#include <cstdio>

ScreenshotItem::ScreenshotItem()
{
}

const wchar_t* ScreenshotItem::GetItemValueText() const
{
    int cnt = m_pinCount.load();
    swprintf_s(m_valueBuf, 16, L"%d", cnt);
    return m_valueBuf;
}

int ScreenshotItem::GetItemWidthEx(void* hDC) const
{
    // 简单估算宽度
    HDC dc = static_cast<HDC>(hDC);
    if (!dc) return 30;
    std::wstring text = GetItemValueText();
    SIZE size = { 0 };
    ::GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    return size.cx + 8;
}
