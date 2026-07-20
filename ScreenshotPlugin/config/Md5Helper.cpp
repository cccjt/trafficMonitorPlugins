#include "../pch/stdafx.h"
#include "Md5Helper.h"
#include <bcrypt.h>
#include <vector>

// 使用 Windows BCrypt API 计算 MD5
// 注意:Win10 1607+ 才支持 MD5 via BCrypt,但绝大多数现代系统都支持

namespace
{
    std::string WstringToUtf8(const std::wstring& wstr)
    {
        if (wstr.empty()) return std::string();
        int len = ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
            static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        std::string out(len, 0);
        ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
            static_cast<int>(wstr.size()), &out[0], len, nullptr, nullptr);
        return out;
    }

    std::wstring BytesToHex(const unsigned char* data, size_t len)
    {
        static const wchar_t hexChars[] = L"0123456789abcdef";
        std::wstring out;
        out.reserve(len * 2);
        for (size_t i = 0; i < len; ++i)
        {
            out.push_back(hexChars[(data[i] >> 4) & 0x0F]);
            out.push_back(hexChars[data[i] & 0x0F]);
        }
        return out;
    }

    std::wstring ComputeMd5(const void* data, size_t len)
    {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        std::vector<unsigned char> hashData;

        NTSTATUS status = ::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0);
        if (status != 0) return L"";

        DWORD hashLen = 0, cbData = 0;
        ::BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PBYTE>(&hashLen), sizeof(hashLen), &cbData, 0);
        hashData.resize(hashLen);

        status = ::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
        if (status != 0)
        {
            ::BCryptCloseAlgorithmProvider(hAlg, 0);
            return L"";
        }

        status = ::BCryptHashData(hHash, const_cast<PBYTE>(static_cast<const unsigned char*>(data)),
            static_cast<ULONG>(len), 0);
        if (status != 0)
        {
            ::BCryptDestroyHash(hHash);
            ::BCryptCloseAlgorithmProvider(hAlg, 0);
            return L"";
        }

        status = ::BCryptFinishHash(hHash, hashData.data(), hashLen, 0);
        ::BCryptDestroyHash(hHash);
        ::BCryptCloseAlgorithmProvider(hAlg, 0);

        if (status != 0) return L"";

        return BytesToHex(hashData.data(), hashLen);
    }
}

std::wstring Md5Helper::Compute(const std::wstring& input)
{
    // 直接对 wchar_t 缓冲计算(用于 API 签名场景)
    return ComputeMd5(input.data(), input.size() * sizeof(wchar_t));
}

std::wstring Md5Helper::ComputeUtf8(const std::wstring& input)
{
    std::string utf8 = WstringToUtf8(input);
    return ComputeMd5(utf8.data(), utf8.size());
}
