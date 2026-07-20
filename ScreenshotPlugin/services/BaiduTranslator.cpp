#include "../pch/stdafx.h"
#include "BaiduTranslator.h"
#include "../config/Md5Helper.h"
#include <winhttp.h>
#include <sstream>
#include <random>

std::wstring BaiduTranslator::s_lastError;

std::wstring BaiduTranslator::MakeSign(const std::wstring& appId,
    const std::wstring& query, const std::wstring& salt,
    const std::wstring& secretKey)
{
    // 签名公式: md5(appid + query + salt + secretKey)
    std::wstring raw = appId + query + salt + secretKey;
    return Md5Helper::ComputeUtf8(raw);
}

std::wstring BaiduTranslator::UrlEncode(const std::wstring& s)
{
    std::string utf8;
    int len = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        utf8.resize(len);
        ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &utf8[0], len, nullptr, nullptr);
        // 去掉末尾的 \0
        if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();
    }

    std::wstring out;
    static const wchar_t hex[] = L"0123456789ABCDEF";
    for (unsigned char c : utf8)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            out.push_back(static_cast<wchar_t>(c));
        }
        else
        {
            out.push_back(L'%');
            out.push_back(hex[(c >> 4) & 0x0F]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

bool BaiduTranslator::HttpGet(const std::wstring& url, std::string& response)
{
    // 解析 URL
    URL_COMPONENTS uc = { 0 };
    wchar_t hostName[256] = { 0 };
    wchar_t urlPath[1024] = { 0 };
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = hostName;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = urlPath;
    uc.dwUrlPathLength = 1023;

    if (!::WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &uc))
    {
        s_lastError = L"WinHttpCrackUrl 失败";
        return false;
    }

    HINTERNET hSession = ::WinHttpOpen(L"ScreenshotPlugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
        s_lastError = L"WinHttpOpen 失败";
        return false;
    }

    HINTERNET hConnect = ::WinHttpConnect(hSession, hostName, uc.nPort, 0);
    if (!hConnect)
    {
        s_lastError = L"WinHttpConnect 失败";
        ::WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = ::WinHttpOpenRequest(hConnect, L"GET",
        urlPath, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest)
    {
        s_lastError = L"WinHttpOpenRequest 失败";
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
        return false;
    }

    BOOL bResult = ::WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bResult)
    {
        s_lastError = L"WinHttpSendRequest 失败";
        ::WinHttpCloseHandle(hRequest);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
        return false;
    }

    bResult = ::WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResult)
    {
        s_lastError = L"WinHttpReceiveResponse 失败";
        ::WinHttpCloseHandle(hRequest);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
        return false;
    }

    // 读取响应
    DWORD bytesRead = 0;
    char buffer[4096];
    response.clear();
    while (true)
    {
        if (!::WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead))
            break;
        if (bytesRead == 0) break;
        response.append(buffer, bytesRead);
    }

    ::WinHttpCloseHandle(hRequest);
    ::WinHttpCloseHandle(hConnect);
    ::WinHttpCloseHandle(hSession);
    return true;
}

bool BaiduTranslator::Translate(const BaiduConfig& cfg, const std::wstring& query, std::wstring& result)
{
    s_lastError.clear();

    if (cfg.appId.empty() || cfg.secretKey.empty())
    {
        s_lastError = L"未配置百度翻译 AppID/SecretKey";
        return false;
    }
    if (query.empty())
    {
        s_lastError = L"待翻译文本为空";
        return false;
    }

    // 生成随机 salt
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    std::wstring salt = std::to_wstring(dis(gen));

    // 计算签名
    std::wstring sign = MakeSign(cfg.appId, query, salt, cfg.secretKey);

    // 拼接 URL
    std::wostringstream url;
    url << L"https://fanyi-api.baidu.com/api/trans/vip/translate?"
        << L"q=" << UrlEncode(query)
        << L"&from=" << cfg.fromLang
        << L"&to=" << cfg.toLang
        << L"&appid=" << cfg.appId
        << L"&salt=" << salt
        << L"&sign=" << sign;

    std::string response;
    if (!HttpGet(url.str(), response))
    {
        return false;
    }

    // 解析 JSON 响应(简化解析)
    // 成功格式: {"from":"en","to":"zh","trans_result":[{"src":"hello","dst":"你好"}]}
    // 失败格式: {"error_code":"54001","error_msg":"INVALID_SIGN"}
    std::wstring wresponse;
    int len = ::MultiByteToWideChar(CP_UTF8, 0, response.c_str(),
        static_cast<int>(response.size()), nullptr, 0);
    if (len > 0)
    {
        wresponse.resize(len);
        ::MultiByteToWideChar(CP_UTF8, 0, response.c_str(),
            static_cast<int>(response.size()), &wresponse[0], len);
    }

    // 检测错误
    size_t errPos = wresponse.find(L"\"error_code\"");
    if (errPos != std::wstring::npos)
    {
        size_t msgPos = wresponse.find(L"\"error_msg\"");
        std::wstring errCode = wresponse.substr(errPos + 13, 20);
        std::wstring errMsg;
        if (msgPos != std::wstring::npos)
        {
            size_t start = wresponse.find(L'"', msgPos + 11) + 1;
            size_t end = wresponse.find(L'"', start);
            if (start != std::wstring::npos && end != std::wstring::npos)
            {
                errMsg = wresponse.substr(start, end - start);
            }
        }
        s_lastError = L"百度翻译错误: " + errCode + L" " + errMsg;
        return false;
    }

    // 提取 dst 字段
    // 简化:取所有 "dst":"xxx" 的内容拼接
    result.clear();
    size_t pos = 0;
    while (true)
    {
        size_t dstPos = wresponse.find(L"\"dst\":\"", pos);
        if (dstPos == std::wstring::npos) break;
        size_t start = dstPos + 7;
        size_t end = wresponse.find(L'"', start);
        if (end == std::wstring::npos) break;
        if (!result.empty()) result += L'\n';
        result += wresponse.substr(start, end - start);
        pos = end + 1;
    }

    // 处理转义字符 \\n \" 等
    std::wstring unescaped;
    unescaped.reserve(result.size());
    for (size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] == L'\\' && i + 1 < result.size())
        {
            wchar_t next = result[i + 1];
            if (next == L'n') unescaped.push_back(L'\n');
            else if (next == L't') unescaped.push_back(L'\t');
            else if (next == L'"') unescaped.push_back(L'"');
            else if (next == L'\\') unescaped.push_back(L'\\');
            else if (next == L'/') unescaped.push_back(L'/');
            else
            {
                unescaped.push_back(L'\\');
                unescaped.push_back(next);
            }
            ++i;
        }
        else
        {
            unescaped.push_back(result[i]);
        }
    }
    result = unescaped;

    return !result.empty();
}
