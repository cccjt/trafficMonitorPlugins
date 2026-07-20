#pragma once
#include <string>
#include <cstdint>

// MD5 计算工具
class Md5Helper
{
public:
    // 计算字符串的 MD5,返回 32 字符小写 HEX
    static std::wstring Compute(const std::wstring& input);

    // 计算字符串的 MD5(UTF-8 编码),返回 32 字符小写 HEX
    static std::wstring ComputeUtf8(const std::wstring& input);
};
