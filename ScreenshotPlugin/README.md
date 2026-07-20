# ScreenshotPlugin

TrafficMonitor 截图工具插件 - 类似 PixPin 的截图/贴图/OCR/翻译功能

## 功能

- **截图**:全屏区域选择,支持多显示器
- **贴图**:截图后钉在屏幕上,带发光边框
  - 焦点时蓝色,失焦时灰色
  - 滚轮调整透明度(边框不透明)
  - 四角拖动自由缩放
  - 双击复制并关闭
- **保存**:截图保存为 PNG/JPEG/BMP
- **OCR**:基于 Windows.Media.Ocr(开发中,目前为占位)
- **翻译**:百度翻译 API
- **二维码识别**:基于 Windows.Media.BarcodeScanner(开发中)
- **放大镜取色**:
  - 鼠标进入选区时显示放大镜
  - 显示 9x9 像素网格 + 坐标 + 颜色值
  - HEX/RGB/CMYK 三种颜色格式(Shift 切换)
  - 按 C 复制颜色值

## 默认热键

| 功能 | 默认热键 | 状态 |
|---|---|---|
| 截图 | Ctrl+Alt+A | 启用 |
| 截图并贴图 | Ctrl+Alt+P | 启用 |
| 截图并 OCR | Ctrl+Alt+O | 启用 |
| 截图并翻译 | Ctrl+Alt+T | 启用 |
| 截图并保存 | Ctrl+Alt+S | 禁用 |
| 截图并识别二维码 | Ctrl+Alt+Q | 禁用 |
| OCR 剪贴板图片 | Ctrl+Alt+Shift+O | 禁用 |
| 翻译剪贴板文字 | Ctrl+Alt+Shift+T | 禁用 |

可在 TrafficMonitor 设置 → 插件 → 截图工具 中修改。

## 选区交互

- **左键拖动**:新建选区
- **选区内左键拖动**:移动选区
- **8 个手柄拖动**:调整选区大小
- **鼠标进入选区**:显示放大镜
- **双击选区**:复制到剪贴板并关闭
- **右键选区**:直接贴图
- **回车**:显示工具条
- **ESC**:取消
- **C**:复制放大镜中心颜色值
- **Shift**:切换颜色格式

## 贴图窗口交互

- **左键拖动**:移动贴图(非四角)
- **四角拖动**:自由缩放
- **滚轮**:调整图片透明度(边框不变)
- **双击**:复制并关闭
- **右键**:菜单(复制/保存/还原大小/还原透明度/关闭)
- **ESC**:关闭
- **点击窗口**:获取焦点(边框变蓝)

## 编译

1. 使用 Visual Studio 2022 打开 `trafficMonitorPlugins.sln`
2. 选择 `Release | x64` 配置
3. 编译生成 `ScreenshotPlugin.dll`
4. 将 DLL 复制到 TrafficMonitor 的 `plugins` 目录
5. 在 TrafficMonitor 设置中启用插件

## 配置文件

位置:`%APPDATA%\TrafficMonitor\plugins\ScreenshotPlugin.ini`

```ini
[General]
ShowToolbar=1
DefaultSaveDir=
AutoOcrLang=zh-Hans-CN

[Hotkey_Capture]
Modifiers=6
VK=65
Enabled=1

[Hotkey_Pin]
Modifiers=6
VK=80
Enabled=1

[Baidu]
AppId=
SecretKey=
FromLang=auto
ToLang=zh
```

## 文件结构

```
ScreenshotPlugin/
├── core/                          # 核心框架
│   ├── PluginInterface.h          # TM 插件接口
│   ├── ScreenshotPlugin.h/.cpp    # ITMPlugin 接口实现
│   ├── ScreenshotCore.h/.cpp      # 截图流程协调器
│   ├── ScreenshotItem.h/.cpp      # TM 显示项(贴图数)
│   └── dllmain.cpp                # DLL 入口
├── config/                        # 配置管理
│   ├── ActionType.h/.cpp          # 功能动作枚举(8 种)
│   ├── ScreenshotConfig.h/.cpp    # INI 配置管理
│   ├── HotkeyManager.h/.cpp       # 全局热键注册+路由
│   └── Md5Helper.h/.cpp           # MD5 计算(BCrypt)
├── capture/                       # 屏幕捕获
│   ├── ScreenCapture.h/.cpp       # 多显示器捕获+裁剪
│   └── GdiPlusHelper.h/.cpp       # GDI+/剪贴板/PNG 工具
├── ui/                            # 界面
│   ├── SelectionOverlay.h/.cpp    # 选区覆盖层(暗化/手柄/标签)
│   ├── MagnifierWnd.h/.cpp        # 放大镜取色窗口
│   ├── SelectionToolbar.h/.cpp    # 横向工具条
│   ├── PinWindow.h/.cpp           # 贴图窗口(发光边框/透明度/缩放)
│   ├── OptionsDialog.h/.cpp       # 主配置对话框
│   ├── HotkeyEditDialog.h/.cpp    # 热键编辑对话框
│   └── BaiduConfigDialog.h/.cpp   # 百度翻译配置对话框
├── services/                      # 功能服务
│   ├── OcrEngine.h/.cpp           # OCR 引擎(占位,待集成 WinRT)
│   ├── QrCodeDecoder.h/.cpp       # 二维码识别(占位)
│   └── BaiduTranslator.h/.cpp     # 百度翻译 API 封装
├── res/                           # 资源
│   ├── resource.h
│   └── ScreenshotPlugin.rc
├── pch/                           # 预编译头
│   ├── stdafx.h
│   └── stdafx.cpp
├── ScreenshotPlugin.vcxproj
├── ScreenshotPlugin.vcxproj.filters
└── README.md
```

## 状态

- ✅ v1 阶段 0-4 完成(骨架/配置/捕获/选区/工具条/贴图)
- ✅ v1 阶段 8-9 完成(百度翻译+对话框)
- ⏳ v1 阶段 5 标注(铅笔/箭头/文字/橡皮擦)待实现
- ⏳ v1 阶段 6 二维码识别待实现
- ⏳ v1 阶段 7 OCR 待实现(C++/WinRT)

## License

MIT
