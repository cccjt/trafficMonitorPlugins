#include "stdafx.h"
#include "MenuConfig.h"
#include "Logger.h"
#include <Windows.h>
#include <sstream>

MenuConfig::MenuConfig() {}
MenuConfig::~MenuConfig() {}

void MenuConfig::SetConfigPath(const std::wstring& path)
{
    m_configPath = path;
}

std::wstring MenuConfig::ReadString(const wchar_t* section, const wchar_t* key, const wchar_t* defaultVal) const
{
    // 脚本代码可能较长,使用 INI API 允许的最大缓冲区
    const DWORD bufSize = 32768;
    std::vector<wchar_t> buf(bufSize, 0);
    ::GetPrivateProfileStringW(section, key, defaultVal, buf.data(), bufSize, m_configPath.c_str());
    return std::wstring(buf.data());
}

int MenuConfig::ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const
{
    return static_cast<int>(::GetPrivateProfileIntW(section, key, defaultVal, m_configPath.c_str()));
}

bool MenuConfig::WriteString(const wchar_t* section, const wchar_t* key, const wchar_t* value) const
{
    return ::WritePrivateProfileStringW(section, key, value, m_configPath.c_str()) != 0;
}

bool MenuConfig::WriteInt(const wchar_t* section, const wchar_t* key, int value) const
{
    wchar_t buf[32] = { 0 };
    _itow_s(value, buf, 10);
    return ::WritePrivateProfileStringW(section, key, buf, m_configPath.c_str()) != 0;
}

bool MenuConfig::Load()
{
    m_items.clear();

    if (m_configPath.empty())
    {
        Logger::Instance().Warn(L"MenuConfig::Load: 配置文件路径为空");
        return false;
    }

    int count = ReadInt(L"General", L"Count", 0);
    if (count <= 0)
    {
        Logger::Instance().Info(L"MenuConfig::Load: 配置为空或首次加载");
        return true;
    }

    // 限制最大菜单项数量,防止 INI 损坏导致异常
    const int MAX_ITEMS = 500;
    if (count > MAX_ITEMS)
    {
        Logger::Instance().Warn(L"MenuConfig::Load: 菜单项数量超过上限 " + std::to_wstring(MAX_ITEMS) + L", 已截断");
        count = MAX_ITEMS;
    }

    for (int i = 0; i < count; ++i)
    {
        std::wostringstream section;
        section << L"MenuItem_" << i;
        std::wstring sec = section.str();

        MenuConfigItem item;
        item.name = ReadString(sec.c_str(), L"Name", L"");
        item.iconSource = ReadString(sec.c_str(), L"IconSource", L"");
        item.iconIndex = ReadInt(sec.c_str(), L"IconIndex", 0);
        item.scriptCode = ReadString(sec.c_str(), L"ScriptCode", L"");
        item.enabled = ReadInt(sec.c_str(), L"Enabled", 1) != 0;
        item.isSeparator = ReadInt(sec.c_str(), L"IsSeparator", 0) != 0;

        // 跳过完全空的项(可能 INI 被损坏)
        if (item.name.empty() && item.scriptCode.empty() && !item.isSeparator)
        {
            Logger::Instance().Warn(L"MenuConfig::Load: 跳过空的菜单项 #" + std::to_wstring(i));
            continue;
        }

        m_items.push_back(item);
    }

    Logger::Instance().Info(L"MenuConfig::Load: 已加载 " + std::to_wstring(m_items.size()) + L" 个菜单项");
    return true;
}

bool MenuConfig::Save() const
{
    if (m_configPath.empty())
    {
        Logger::Instance().Warn(L"MenuConfig::Save: 配置文件路径为空");
        return false;
    }

    // 先写入数量
    WriteInt(L"General", L"Count", static_cast<int>(m_items.size()));

    // 写入每一项
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        std::wostringstream section;
        section << L"MenuItem_" << i;
        std::wstring sec = section.str();

        const MenuConfigItem& item = m_items[i];
        WriteString(sec.c_str(), L"Name", item.name.c_str());
        WriteString(sec.c_str(), L"IconSource", item.iconSource.c_str());
        WriteInt(sec.c_str(), L"IconIndex", item.iconIndex);
        WriteString(sec.c_str(), L"ScriptCode", item.scriptCode.c_str());
        WriteInt(sec.c_str(), L"Enabled", item.enabled ? 1 : 0);
        WriteInt(sec.c_str(), L"IsSeparator", item.isSeparator ? 1 : 0);
    }

    // 清理多余的旧项(如果新数量比旧数量少)
    for (size_t i = m_items.size(); i < m_items.size() + 50; ++i)
    {
        std::wostringstream section;
        section << L"MenuItem_" << i;
        std::wstring sec = section.str();
        // 尝试删除旧项,忽略失败
        WriteString(sec.c_str(), nullptr, nullptr);
    }

    Logger::Instance().Info(L"MenuConfig::Save: 已保存 " + std::to_wstring(m_items.size()) + L" 个菜单项");
    return true;
}

void MenuConfig::AddItem(const MenuConfigItem& item)
{
    m_items.push_back(item);
}

void MenuConfig::RemoveItem(size_t index)
{
    if (index < m_items.size())
    {
        m_items.erase(m_items.begin() + index);
    }
}

void MenuConfig::UpdateItem(size_t index, const MenuConfigItem& item)
{
    if (index < m_items.size())
    {
        m_items[index] = item;
    }
}

int MenuConfig::GetEnabledCount() const
{
    int count = 0;
    for (const auto& item : m_items)
    {
        if (item.enabled) ++count;
    }
    return count;
}
