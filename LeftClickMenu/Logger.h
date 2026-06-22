#pragma once
#include <string>

// 简单日志工具:写入独立日志文件
class Logger
{
public:
    // 获取单例实例
    static Logger& Instance();

    // 设置日志目录(由主插件类调用)
    void SetLogDir(const std::wstring& dir);

    // 写入日志
    // level: 日志级别(INFO/WARN/ERROR)
    // message: 日志内容
    void Log(const std::wstring& level, const std::wstring& message);

    // 便捷方法
    void Info(const std::wstring& msg) { Log(L"INFO", msg); }
    void Warn(const std::wstring& msg) { Log(L"WARN", msg); }
    void Error(const std::wstring& msg) { Log(L"ERROR", msg); }

    // 启用/禁用日志
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

private:
    Logger();
    ~Logger();

    // 禁止拷贝
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::wstring m_logDir;      // 日志目录
    std::wstring m_logPath;     // 日志文件完整路径
    bool m_enabled = true;      // 是否启用日志
};
