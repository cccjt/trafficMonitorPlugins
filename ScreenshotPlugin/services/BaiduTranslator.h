#pragma once
#include <string>
#include "../config/ScreenshotConfig.h"

// 百度翻译 API 封装
// API 文档: https://fanyi-api.baidu.com/doc/21
class BaiduTranslator
{
public:
    // 翻译文本
    //   cfg:    百度 API 配置(AppID/SecretKey/From/To)
    //   query:  待翻译的文本
    //   result: 输出翻译结果
    //   返回值: 成功/失败
    static bool Translate(const BaiduConfig& cfg, const std::wstring& query, std::wstring& result);

    // 获取上一次错误信息
    static std::wstring GetLastError() { return s_lastError; }

private:
    static std::wstring s_lastError;

    // 生成签名:md5(appid + query + salt + secretKey)
    static std::wstring MakeSign(const std::wstring& appId,
        const std::wstring& query, const std::wstring& salt,
        const std::wstring& secretKey);

    // URL 编码
    static std::wstring UrlEncode(const std::wstring& s);

    // 发送 HTTP GET 请求
    static bool HttpGet(const std::wstring& url, std::string& response);
};
