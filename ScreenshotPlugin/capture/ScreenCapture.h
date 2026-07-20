#pragma once
#include <Windows.h>
#include <gdiplus.h>

namespace ScreenCapture
{
    // 捕获所有显示器组成的虚拟屏,返回 HBITMAP(32位 ARGB)
    // 调用者拥有所有权,需 DeleteObject
    HBITMAP CaptureAllMonitors();

    // 获取虚拟屏上某物理点的颜色(用于放大镜取色)
    COLORREF GetScreenColorAt(int screenX, int screenY);

    // 从完整屏幕 HBITMAP 中按选区裁剪出子图(返回新 HBITMAP,所有权转移)
    HBITMAP CropRegion(HBITMAP hSource, int x, int y, int w, int h);

    // 获取虚拟屏范围
    void GetVirtualScreenRect(int& x, int& y, int& w, int& h);
}
