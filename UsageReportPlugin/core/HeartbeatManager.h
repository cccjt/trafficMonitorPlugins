#pragma once
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

// 心跳配置
struct HeartbeatConfig
{
    bool enabled = false;
    std::wstring method = L"POST";
    std::wstring url;
    int intervalMinutes = 5;
    // 设备名:留空时自动取 Windows 计算机名,用户可在心跳设置里覆盖并落盘
    std::wstring device;
    std::wstring headers;
    std::wstring body;
};

// 定时心跳:按配置在后台线程周期性发送 HTTP 请求,不读取响应
// 线程优先级低于普通线程,发送前采样 1 秒系统 CPU,超过 50% 时跳过本次
class HeartbeatManager
{
public:
    static HeartbeatManager& Instance();

    // 读取配置并在开启时启动后台线程
    void Initialize(const std::wstring& configPath);
    // 停止后台线程
    void Shutdown();

    // 供报告页面读取:纯文本配置(headers/body 为 base64)
    std::wstring GetConfigText();
    // 供报告页面注入:把配置以 base64 形式写入 window.__heartbeatConfigB64
    std::wstring BuildInitScript();
    // 供报告页面保存:解析文本配置并落盘,立即生效
    void ApplyConfigText(const std::wstring& text);

private:
    HeartbeatManager() = default;
    ~HeartbeatManager();
    HeartbeatManager(const HeartbeatManager&) = delete;
    HeartbeatManager& operator=(const HeartbeatManager&) = delete;

    void StartThread();
    void StopThread();
    void ThreadMain();
    void SendRequest(const HeartbeatConfig& config);
    bool IsSystemCpuBusy() const;
    void Load();
    void Save();
    HeartbeatConfig Snapshot() const;

    static std::wstring EncodeValue(const std::wstring& value);
    static std::wstring DecodeValue(const std::wstring& value);
    static std::wstring Serialize(const HeartbeatConfig& config);
    static bool Parse(const std::wstring& text, HeartbeatConfig& config);

    mutable std::mutex m_mutex;
    HeartbeatConfig m_config;
    std::wstring m_configPath;
    HANDLE m_wakeEvent = nullptr;
    std::atomic<bool> m_stop{ false };
    bool m_started = false;
    std::thread m_thread;
};
