#include "stdafx.h"
#include "IconManager.h"
#include "Logger.h"
#include "resource.h"
#include <sstream>
#include <shellapi.h>
#include <ShlObj.h>

IconManager& IconManager::Instance()
{
    static IconManager instance;
    return instance;
}

IconManager::IconManager()
{
    // 初始化内置图标名 -> 资源 ID 映射
    m_builtinMap[L"script"] = IDI_ICON_SCRIPT;
    m_builtinMap[L"folder"] = IDI_ICON_FOLDER;
    m_builtinMap[L"program"] = IDI_ICON_PROGRAM;
    m_builtinMap[L"url"] = IDI_ICON_URL;
    m_builtinMap[L"settings"] = IDI_ICON_SETTINGS;

    m_builtinNames.push_back(L"script");
    m_builtinNames.push_back(L"folder");
    m_builtinNames.push_back(L"program");
    m_builtinNames.push_back(L"url");
    m_builtinNames.push_back(L"settings");
}

IconManager::~IconManager()
{
    Cleanup();
}

void IconManager::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;
}

void IconManager::Cleanup()
{
    for (auto& pair : m_iconCache)
    {
        if (pair.second) ::DestroyIcon(pair.second);
    }
    m_iconCache.clear();

    for (auto& pair : m_bitmapCache)
    {
        if (pair.second) ::DeleteObject(pair.second);
    }
    m_bitmapCache.clear();
}

HICON IconManager::GetIcon(const std::wstring& source, int index)
{
    if (source.empty()) return nullptr;

    // 生成缓存 key
    std::wostringstream key;
    key << source << L":" << index;
    std::wstring keyStr = key.str();

    // 查找缓存
    auto it = m_iconCache.find(keyStr);
    if (it != m_iconCache.end())
    {
        return it->second;
    }

    HICON hIcon = nullptr;

    // 判断是内置名称还是文件路径
    auto builtinIt = m_builtinMap.find(source);
    if (builtinIt != m_builtinMap.end())
    {
        hIcon = LoadBuiltinIcon(source);
    }
    else
    {
        hIcon = LoadFileIcon(source, index);
    }

    // 缓存(即使为空也缓存,避免重复尝试)
    m_iconCache[keyStr] = hIcon;
    return hIcon;
}

HBITMAP IconManager::GetBitmap(const std::wstring& source, int index, int size)
{
    if (source.empty()) return nullptr;

    // 生成缓存 key
    std::wostringstream key;
    key << source << L":" << index << L":" << size;
    std::wstring keyStr = key.str();

    // 查找缓存
    auto it = m_bitmapCache.find(keyStr);
    if (it != m_bitmapCache.end())
    {
        return it->second;
    }

    HICON hIcon = GetIcon(source, index);
    if (hIcon == nullptr)
    {
        m_bitmapCache[keyStr] = nullptr;
        return nullptr;
    }

    HBITMAP hBmp = IconToBitmap(hIcon, size);
    m_bitmapCache[keyStr] = hBmp;
    return hBmp;
}

HICON IconManager::LoadBuiltinIcon(const std::wstring& name)
{
    if (m_hInstance == nullptr)
    {
        Logger::Instance().Warn(L"IconManager::LoadBuiltinIcon: 模块句柄未初始化");
        return nullptr;
    }

    auto it = m_builtinMap.find(name);
    if (it == m_builtinMap.end()) return nullptr;

    // 加载 16x16 小图标
    HICON hIcon = (HICON)::LoadImageW(
        m_hInstance,
        MAKEINTRESOURCEW(it->second),
        IMAGE_ICON,
        16, 16,
        LR_DEFAULTCOLOR);

    if (hIcon == nullptr)
    {
        Logger::Instance().Warn(L"IconManager::LoadBuiltinIcon: 无法加载内置图标 [" + name + L"]");
    }
    return hIcon;
}

HICON IconManager::LoadFileIcon(const std::wstring& path, int index)
{
    if (path.empty()) return nullptr;

    // 检查文件是否存在
    DWORD attr = ::GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        Logger::Instance().Warn(L"IconManager::LoadFileIcon: 文件不存在 [" + path + L"]");
        return nullptr;
    }

    // 尝试使用 ExtractIconEx 从文件提取图标
    HICON hIconLarge = nullptr;
    HICON hIconSmall = nullptr;
    int count = ::ExtractIconExW(path.c_str(), index, &hIconLarge, &hIconSmall, 1);
    if (count > 0)
    {
        // 优先使用小图标(16x16)
        if (hIconSmall)
        {
            if (hIconLarge) ::DestroyIcon(hIconLarge);
            return hIconSmall;
        }
        return hIconLarge;
    }

    // ExtractIconEx 失败,尝试使用 SHGetFileInfo 获取关联图标
    SHFILEINFOW sfi = { 0 };
    DWORD_PTR result = ::SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON);
    if (result && sfi.hIcon)
    {
        return sfi.hIcon;
    }

    Logger::Instance().Warn(L"IconManager::LoadFileIcon: 无法从文件提取图标 [" + path + L"]");
    return nullptr;
}

HBITMAP IconManager::IconToBitmap(HICON hIcon, int size)
{
    if (hIcon == nullptr) return nullptr;

    // 获取屏幕 DC
    HDC hScreenDC = ::GetDC(nullptr);
    if (hScreenDC == nullptr) return nullptr;

    // 创建兼容 DC
    HDC hMemDC = ::CreateCompatibleDC(hScreenDC);
    if (hMemDC == nullptr)
    {
        ::ReleaseDC(nullptr, hScreenDC);
        return nullptr;
    }

    // 创建兼容位图
    HBITMAP hBmp = ::CreateCompatibleBitmap(hScreenDC, size, size);
    if (hBmp == nullptr)
    {
        ::DeleteDC(hMemDC);
        ::ReleaseDC(nullptr, hScreenDC);
        return nullptr;
    }

    // 选入位图
    HBITMAP hOldBmp = (HBITMAP)::SelectObject(hMemDC, hBmp);

    // 填充透明背景(使用菜单颜色)
    RECT rc = { 0, 0, size, size };
    ::FillRect(hMemDC, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));

    // 绘制图标
    ::DrawIconEx(hMemDC, 0, 0, hIcon, size, size, 0, nullptr, DI_NORMAL);

    // 恢复
    ::SelectObject(hMemDC, hOldBmp);
    ::DeleteDC(hMemDC);
    ::ReleaseDC(nullptr, hScreenDC);

    return hBmp;
}
