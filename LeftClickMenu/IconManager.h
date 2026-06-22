#pragma once
#include <Windows.h>
#include <string>
#include <map>
#include <vector>

// 图标管理器:管理内置图标和从外部文件加载的图标
class IconManager
{
public:
    // 获取单例实例
    static IconManager& Instance();

    // 初始化内置图标(从模块资源中加载)
    void Initialize(HINSTANCE hInstance);

    // 获取图标
    // source: 内置名称(script/folder/program/url/settings)或文件路径
    // index: 文件图标索引(默认 0)
    // 返回 HICON,调用方负责 DestroyIcon
    HICON GetIcon(const std::wstring& source, int index = 0);

    // 获取位图(用于菜单显示)
    // source: 内置名称或文件路径
    // index: 文件图标索引
    // size: 位图尺寸(默认 16x16)
    // 返回 HBITMAP,调用方负责 DeleteObject
    HBITMAP GetBitmap(const std::wstring& source, int index = 0, int size = 16);

    // 获取内置图标名称列表
    const std::vector<std::wstring>& GetBuiltinNames() const { return m_builtinNames; }

    // 清理缓存(插件卸载时调用)
    void Cleanup();

private:
    IconManager();
    ~IconManager();

    // 禁止拷贝
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    // 加载内置图标资源
    HICON LoadBuiltinIcon(const std::wstring& name);

    // 从文件加载图标
    HICON LoadFileIcon(const std::wstring& path, int index);

    // HICON 转 HBITMAP
    HBITMAP IconToBitmap(HICON hIcon, int size);

    // 内置图标名 -> 资源 ID 映射
    std::map<std::wstring, UINT> m_builtinMap;

    // 内置图标名称列表(用于下拉框)
    std::vector<std::wstring> m_builtinNames;

    // 图标缓存: key = "source:index"
    std::map<std::wstring, HICON> m_iconCache;

    // 位图缓存: key = "source:index:size"
    std::map<std::wstring, HBITMAP> m_bitmapCache;

    HINSTANCE m_hInstance = nullptr;
};
