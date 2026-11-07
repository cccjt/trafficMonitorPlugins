#include <Windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "HeartbeatManager.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

namespace
{
const int kMinIntervalMinutes = 1;
const int kMaxIntervalMinutes = 1440;
const ULONGLONG kCpuSkipThreshold = 50;
const DWORD kCpuSampleMilliseconds = 1000;

std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty()) return std::string();

    int len = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();

    std::string result(len, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
        &result[0], len, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) return std::wstring();

    int len = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (len <= 0) return std::wstring();

    std::wstring result(len, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], len);
    return result;
}

std::wstring Trim(const std::wstring& text)
{
    size_t begin = text.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) return std::wstring();
    size_t end = text.find_last_not_of(L" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::wstring ToLower(const std::wstring& text)
{
    std::wstring result = text;
    for (size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] >= L'A' && result[i] <= L'Z') result[i] += (L'a' - L'A');
    }
    return result;
}

bool EnsureParentDirectory(const std::wstring& path)
{
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos || pos == 0) return true;

    std::wstring dir = path.substr(0, pos);
    DWORD attr = ::GetFileAttributesW(dir.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

    if (!EnsureParentDirectory(dir)) return false;
    ::CreateDirectoryW(dir.c_str(), nullptr);
    return ::GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring ReadTextFile(const std::wstring& path)
{
    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return std::wstring();

    DWORD size = ::GetFileSize(hFile, nullptr);
    std::string buffer(size, 0);
    DWORD read = 0;
    ::ReadFile(hFile, buffer.empty() ? nullptr : &buffer[0], size, &read, nullptr);
    ::CloseHandle(hFile);

    buffer.resize(read);
    return Utf8ToWide(buffer);
}

bool WriteTextFile(const std::wstring& path, const std::wstring& text)
{
    if (!EnsureParentDirectory(path)) return false;

    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    std::string utf8 = WideToUtf8(text);
    DWORD written = 0;
    BOOL ok = ::WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    ::CloseHandle(hFile);
    return ok;
}

std::wstring NormalizeHeaders(const std::wstring& raw)
{
    std::wstring result;
    result.reserve(raw.size() + 2);

    for (size_t i = 0; i < raw.size(); ++i)
    {
        wchar_t ch = raw[i];
        if (ch == L'\r') continue;
        if (ch == L'\n')
        {
            result += L"\r\n";
        }
        else
        {
            result += ch;
        }
    }

    if (result.size() >= 2 && result.compare(result.size() - 2, 2, L"\r\n") == 0)
    {
        return result;
    }
    result += L"\r\n";
    return result;
}

std::wstring NormalizeMethod(const std::wstring& method)
{
    std::wstring upper = Trim(method);
    for (size_t i = 0; i < upper.size(); ++i)
    {
        if (upper[i] >= L'a' && upper[i] <= L'z') upper[i] -= (L'a' - L'A');
    }

    if (upper == L"GET" || upper == L"POST" || upper == L"PUT"
        || upper == L"PATCH" || upper == L"DELETE" || upper == L"HEAD")
    {
        return upper;
    }
    return L"POST";
}

ULONGLONG FileTimeToUInt64(const FILETIME& ft)
{
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
}

HeartbeatManager& HeartbeatManager::Instance()
{
    static HeartbeatManager instance;
    return instance;
}

HeartbeatManager::~HeartbeatManager()
{
    Shutdown();
}

void HeartbeatManager::Initialize(const std::wstring& configPath)
{
    if (configPath.empty()) return;

    bool pathChanged = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pathChanged = !m_configPath.empty() && m_configPath != configPath;
    }
    if (pathChanged) StopThread();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_configPath == configPath && m_wakeEvent != nullptr) return;

        m_configPath = configPath;
        Load();
        if (m_wakeEvent == nullptr)
        {
            m_wakeEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        }
    }

    if (Snapshot().enabled) StartThread();
}

void HeartbeatManager::Shutdown()
{
    StopThread();

    if (m_wakeEvent != nullptr)
    {
        ::CloseHandle(m_wakeEvent);
        m_wakeEvent = nullptr;
    }
}

std::wstring HeartbeatManager::GetConfigText()
{
    HeartbeatConfig config = Snapshot();
    return Serialize(config);
}

std::wstring HeartbeatManager::BuildInitScript()
{
    std::wstring text = GetConfigText();
    std::wstring encoded = EncodeValue(text);
    return L"window.__heartbeatConfigB64=\"" + encoded + L"\";";
}

void HeartbeatManager::ApplyConfigText(const std::wstring& text)
{
    HeartbeatConfig parsed;
    if (!Parse(text, parsed)) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = parsed;
        Save();
        if (m_wakeEvent == nullptr)
        {
            m_wakeEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
        }
    }

    if (parsed.enabled) StartThread();
    if (m_wakeEvent != nullptr) ::SetEvent(m_wakeEvent);
}

void HeartbeatManager::StartThread()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_started) return;

    m_stop = false;
    m_thread = std::thread(&HeartbeatManager::ThreadMain, this);
    m_started = true;
}

void HeartbeatManager::StopThread()
{
    std::thread thread;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_started) return;
        m_stop = true;
        if (m_wakeEvent != nullptr) ::SetEvent(m_wakeEvent);
        thread = std::move(m_thread);
        m_started = false;
    }

    if (thread.joinable()) thread.join();
}

void HeartbeatManager::ThreadMain()
{
    // 后台线程尽量降低调度优先级,避免与前台交互抢 CPU
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    while (!m_stop.load())
    {
        HeartbeatConfig config = Snapshot();
        if (!config.enabled)
        {
            if (::WaitForSingleObject(m_wakeEvent, INFINITE) != WAIT_OBJECT_0) break;
            continue;
        }

        DWORD intervalMs = static_cast<DWORD>(config.intervalMinutes) * 60000u;
        DWORD waitResult = ::WaitForSingleObject(m_wakeEvent, intervalMs);
        if (m_stop.load()) break;
        if (waitResult != WAIT_TIMEOUT) continue;     // 配置变更,按新间隔重新等待

        // 采样期间系统可能被占用,超过阈值则跳过本次
        if (IsSystemCpuBusy()) continue;

        config = Snapshot();
        if (m_stop.load() || !config.enabled) continue;
        SendRequest(config);
    }
}

bool HeartbeatManager::IsSystemCpuBusy() const
{
    FILETIME idleTime = { 0 };
    FILETIME kernelTime = { 0 };
    FILETIME userTime = { 0 };
    if (!::GetSystemTimes(&idleTime, &kernelTime, &userTime)) return false;

    ULONGLONG idle1 = FileTimeToUInt64(idleTime);
    ULONGLONG kernel1 = FileTimeToUInt64(kernelTime);
    ULONGLONG user1 = FileTimeToUInt64(userTime);

    ::Sleep(kCpuSampleMilliseconds);

    if (!::GetSystemTimes(&idleTime, &kernelTime, &userTime)) return false;

    ULONGLONG idle2 = FileTimeToUInt64(idleTime);
    ULONGLONG kernel2 = FileTimeToUInt64(kernelTime);
    ULONGLONG user2 = FileTimeToUInt64(userTime);

    ULONGLONG idleDelta = idle2 - idle1;
    ULONGLONG totalDelta = (kernel2 - kernel1) + (user2 - user1);
    if (totalDelta == 0) return false;

    ULONGLONG busyDelta = totalDelta > idleDelta ? totalDelta - idleDelta : 0;
    return busyDelta * 100 >= totalDelta * kCpuSkipThreshold;
}

void HeartbeatManager::SendRequest(const HeartbeatConfig& config)
{
    if (config.url.empty()) return;

    std::vector<wchar_t> urlBuffer(config.url.begin(), config.url.end());
    urlBuffer.push_back(0);

    URL_COMPONENTS components = { 0 };
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!::WinHttpCrackUrl(urlBuffer.data(), 0, 0, &components)) return;
    if (components.nScheme != INTERNET_SCHEME_HTTP
        && components.nScheme != INTERNET_SCHEME_HTTPS)
    {
        return;
    }

    HINTERNET session = ::WinHttpOpen(L"UsageReportHeartbeat/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr)
    {
        session = ::WinHttpOpen(L"UsageReportHeartbeat/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (session == nullptr) return;
    ::WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.lpszExtraInfo != nullptr && components.dwExtraInfoLength > 0)
    {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty()) path = L"/";

    HINTERNET connection = ::WinHttpConnect(session, host.c_str(), components.nPort, 0);
    if (connection == nullptr)
    {
        ::WinHttpCloseHandle(session);
        return;
    }

    DWORD requestFlags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    std::wstring method = NormalizeMethod(config.method);
    HINTERNET request = ::WinHttpOpenRequest(connection, method.c_str(), path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);

    if (request != nullptr)
    {
        std::wstring headers = NormalizeHeaders(config.headers);
        if (!config.body.empty() && ToLower(headers).find(L"content-type") == std::wstring::npos)
        {
            headers = L"Content-Type: application/json; charset=utf-8\r\n" + headers;
        }
        if (!headers.empty())
        {
            ::WinHttpAddRequestHeaders(request, headers.c_str(), static_cast<DWORD>(-1),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        std::string body = WideToUtf8(config.body);
        DWORD bodyLength = static_cast<DWORD>(body.size());
        ::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            body.empty() ? nullptr : &body[0], bodyLength, bodyLength, 0);

        // 只发送请求,不等待响应
        ::WinHttpCloseHandle(request);
    }

    ::WinHttpCloseHandle(connection);
    ::WinHttpCloseHandle(session);
}

HeartbeatConfig HeartbeatManager::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void HeartbeatManager::Load()
{
    if (m_configPath.empty()) return;

    std::wstring text = ReadTextFile(m_configPath);
    if (text.empty()) return;

    HeartbeatConfig parsed;
    if (Parse(text, parsed)) m_config = parsed;
}

void HeartbeatManager::Save()
{
    if (m_configPath.empty()) return;
    WriteTextFile(m_configPath, Serialize(m_config));
}

std::wstring HeartbeatManager::Serialize(const HeartbeatConfig& config)
{
    std::wstring text;
    text += L"enabled=";
    text += config.enabled ? L"1" : L"0";
    text += L"\nmethod=" + NormalizeMethod(config.method);
    text += L"\ninterval=" + std::to_wstring(config.intervalMinutes);
    text += L"\nurl=" + config.url;
    text += L"\nheaders=" + EncodeValue(config.headers);
    text += L"\nbody=" + EncodeValue(config.body);
    return text;
}

bool HeartbeatManager::Parse(const std::wstring& text, HeartbeatConfig& config)
{
    if (text.empty()) return false;

    HeartbeatConfig result;
    size_t position = 0;
    while (position < text.size())
    {
        size_t lineEnd = text.find_first_of(L"\r\n", position);
        std::wstring line = text.substr(position,
            (lineEnd == std::wstring::npos) ? std::wstring::npos : lineEnd - position);
        position = (lineEnd == std::wstring::npos) ? text.size() : lineEnd + 1;

        size_t separator = line.find(L'=');
        if (separator == std::wstring::npos) continue;

        std::wstring key = ToLower(Trim(line.substr(0, separator)));
        std::wstring value = line.substr(separator + 1);

        if (key == L"enabled")
        {
            result.enabled = (value == L"1" || ToLower(value) == L"true");
        }
        else if (key == L"method")
        {
            result.method = NormalizeMethod(value);
        }
        else if (key == L"interval")
        {
            int interval = _wtoi(value.c_str());
            if (interval < kMinIntervalMinutes) interval = kMinIntervalMinutes;
            if (interval > kMaxIntervalMinutes) interval = kMaxIntervalMinutes;
            result.intervalMinutes = interval;
        }
        else if (key == L"url")
        {
            result.url = Trim(value);
        }
        else if (key == L"headers")
        {
            result.headers = DecodeValue(Trim(value));
        }
        else if (key == L"body")
        {
            result.body = DecodeValue(Trim(value));
        }
    }

    config = result;
    return true;
}

std::wstring HeartbeatManager::EncodeValue(const std::wstring& value)
{
    std::string utf8 = WideToUtf8(value);
    if (utf8.empty()) return std::wstring();

    DWORD chars = 0;
    if (!::CryptBinaryToStringA(reinterpret_cast<const BYTE*>(utf8.data()),
            static_cast<DWORD>(utf8.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &chars))
    {
        return std::wstring();
    }

    std::string base64(chars, 0);
    if (!::CryptBinaryToStringA(reinterpret_cast<const BYTE*>(utf8.data()),
            static_cast<DWORD>(utf8.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &base64[0], &chars))
    {
        return std::wstring();
    }
    base64.resize(chars);

    return Utf8ToWide(base64);
}

std::wstring HeartbeatManager::DecodeValue(const std::wstring& value)
{
    std::string base64 = WideToUtf8(value);
    if (base64.empty()) return std::wstring();

    DWORD bytes = 0;
    if (!::CryptStringToBinaryA(base64.c_str(), static_cast<DWORD>(base64.size()),
            CRYPT_STRING_BASE64, nullptr, &bytes, nullptr, nullptr))
    {
        return std::wstring();
    }
    if (bytes == 0) return std::wstring();

    std::string decoded(bytes, 0);
    if (!::CryptStringToBinaryA(base64.c_str(), static_cast<DWORD>(base64.size()),
            CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(&decoded[0]), &bytes, nullptr, nullptr))
    {
        return std::wstring();
    }
    decoded.resize(bytes);

    return Utf8ToWide(decoded);
}
