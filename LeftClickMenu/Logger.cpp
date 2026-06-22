#include "stdafx.h"
#include "Logger.h"
#include <Windows.h>
#include <ctime>
#include <sstream>
#include <iomanip>

Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger() {}
Logger::~Logger() {}

void Logger::SetLogDir(const std::wstring& dir)
{
    m_logDir = dir;
    if (!m_logDir.empty() && m_logDir.back() != L'\\' && m_logDir.back() != L'/')
    {
        m_logDir += L'\\';
    }
    m_logPath = m_logDir + L"LeftClickMenu.log";
}

void Logger::Log(const std::wstring& level, const std::wstring& message)
{
    if (!m_enabled || m_logPath.empty()) return;

    // 获取当前时间
    time_t now = std::time(nullptr);
    struct tm tm;
    localtime_s(&tm, &now);

    std::wostringstream oss;
    oss << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S");
    oss << L" [" << level << L"] " << message << L"\r\n";

    std::wstring line = oss.str();

    // 追加写入文件(线程安全:使用互斥锁)
    static CRITICAL_SECTION cs;
    static bool csInitialized = false;
    if (!csInitialized)
    {
        ::InitializeCriticalSection(&cs);
        csInitialized = true;
    }

    ::EnterCriticalSection(&cs);
    HANDLE hFile = ::CreateFileW(
        m_logPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        ::WriteFile(hFile, line.c_str(),
            static_cast<DWORD>(line.size() * sizeof(wchar_t)),
            &written, nullptr);
        ::CloseHandle(hFile);
    }
    ::LeaveCriticalSection(&cs);
}
