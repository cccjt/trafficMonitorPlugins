#pragma once
#include <string>

// 截图插件的功能动作类型(对应热键槽位)
enum class ActionType
{
    Capture = 0,            // 截图(选区后显示工具条)
    Pin = 1,                // 截图并直接贴图
    Ocr = 2,                // 截图并OCR识别
    Translate = 3,          // 截图并翻译
    Save = 4,               // 截图并直接保存
    QrCode = 5,             // 截图并识别二维码
    ClipboardOcr = 6,       // OCR剪贴板中的图片
    ClipboardTranslate = 7, // 翻译剪贴板中的文字
};

// 内置功能总数
constexpr int ACTION_COUNT = 8;

// 判断是否为需要截图的动作(走选区流程)
inline bool IsScreenshotAction(ActionType a)
{
    return a == ActionType::Capture
        || a == ActionType::Pin
        || a == ActionType::Ocr
        || a == ActionType::Translate
        || a == ActionType::Save
        || a == ActionType::QrCode;
}

// 获取功能的中文名称
inline const wchar_t* GetActionName(ActionType a)
{
    switch (a)
    {
    case ActionType::Capture:            return L"截图";
    case ActionType::Pin:                return L"截图并贴图";
    case ActionType::Ocr:                return L"截图并OCR";
    case ActionType::Translate:          return L"截图并翻译";
    case ActionType::Save:               return L"截图并保存";
    case ActionType::QrCode:             return L"截图并识别二维码";
    case ActionType::ClipboardOcr:       return L"OCR剪贴板图片";
    case ActionType::ClipboardTranslate: return L"翻译剪贴板文字";
    }
    return L"";
}

// 单条热键配置
struct ScreenshotHotkeyItem
{
    UINT modifiers = 0;         // MOD_CONTROL/MOD_ALT/MOD_SHIFT/MOD_WIN 组合
    UINT vk = 0;                // 虚拟键码
    bool enabled = false;       // 是否启用

    bool IsValid() const { return vk != 0; }

    // 转换为可显示的热键字符串,如 "Ctrl+Alt+A"
    std::wstring ToHotkeyString() const;

    // 从字符串解析热键(逆向操作)
    static bool ParseHotkeyString(const std::wstring& str, UINT& modifiers, UINT& vk);
};

// 单条功能热键的默认值
struct DefaultHotkey
{
    UINT modifiers;
    UINT vk;
    bool enabled;
};

// 获取各功能默认热键
const DefaultHotkey& GetDefaultHotkey(ActionType a);
