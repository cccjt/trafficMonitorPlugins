# LeftClickMenu 插件开发计划

## 概述

基于 HotkeyPlugin 的项目结构和 TrafficMonitor 插件接口（`PluginInterface.h`），创建一个新插件 **LeftClickMenu**。用户在任务栏/主窗口的显示项上左键单击，弹出自定义菜单；每个菜单项可设置图标（内置图标库 + 文件加载）和 PowerShell 脚本动作。

## 项目结构

```
LeftClickMenu/
├── PluginInterface.h          # 插件接口（与 HotkeyPlugin 共用，复制）
├── MenuPlugin.h/cpp           # 主插件类，实现 ITMPlugin
├── MenuItem.h/cpp             # 显示项目类，实现 IPluginItem，处理左键弹出菜单
├── MenuConfig.h/cpp           # 配置管理，INI 文件读写
├── MenuManager.h/cpp          # 菜单管理器，构建/显示弹出菜单、执行动作
├── IconManager.h/cpp          # 图标管理，内置图标 + 从文件加载图标
├── OptionsDialog.h/cpp        # 主配置对话框（列表管理菜单项）
├── MenuEditDialog.h/cpp       # 单条菜单项编辑对话框（名称/图标/脚本）
├── resource.h                 # 资源定义
├── LeftClickMenu.rc           # 资源文件（对话框模板 + 内置图标）
├── LeftClickMenu.vcxproj      # VS 项目文件
├── LeftClickMenu.vcxproj.filters
├── dllmain.cpp                # DLL 入口
├── stdafx.h/cpp               # 预编译头
└── res/                       # 内置图标资源目录（.ico 文件）
    ├── script.ico
    ├── folder.ico
    ├── program.ico
    ├── url.ico
    └── settings.ico
```

## 阶段一：MVP（先跑通核心流程）

### Task 1: 创建项目骨架

- 在解决方案 `HotkeyPlugin.sln` 中新增 `LeftClickMenu` 项目
- 创建 `LeftClickMenu.vcxproj`，配置与 HotkeyPlugin 一致（MFC 动态链接 DLL、Unicode、x64/Win32 双平台）
- 复制 `PluginInterface.h`、`stdafx.h/cpp`、`dllmain.cpp` 到新项目目录
- 创建 `resource.h` 和 `.rc` 资源文件

### Task 2: 配置管理 — MenuConfig

**`MenuConfig.h/cpp`** — 菜单项配置结构和 INI 持久化

```cpp
struct MenuConfigItem {
    std::wstring name;          // 菜单项显示名称
    std::wstring iconSource;    // 图标来源：内置名称或文件路径
    int iconIndex;              // 图标索引（文件加载时使用，默认 0）
    std::wstring scriptCode;    // PowerShell 脚本代码（Base64 编码存储）
    bool enabled;               // 是否启用
    bool isSeparator;           // 是否为分隔线
};

class MenuConfig {
    // INI 读写，配置文件: LeftClickMenu.ini
    // [MenuItem_0] / name / iconSource / iconIndex / scriptCode / enabled / isSeparator
};
```

**关键决策**：`scriptCode` 在 INI 中以 Base64 编码存储，避免多行值和引号转义问题。运行时解码后再通过 `powershell.exe -EncodedCommand` 执行。

### Task 3: 菜单管理器 — MenuManager

**`MenuManager.h/cpp`** — 构建弹出菜单并执行动作

- `ShowPopupMenu(HWND hWnd, int x, int y)`：使用 Win32 API 创建弹出菜单
  - `CreatePopupMenu` 创建菜单
  - 遍历配置项，使用 `InsertMenuItem` 添加菜单项（MVP 阶段可先不带图标）
  - 支持分隔线（`MFT_SEPARATOR`）和禁用项（`MFS_GRAYED`）
  - 菜单命令 ID 直接对应配置项索引，便于回查
  - `TrackPopupMenu` 显示菜单并获取用户选择
  - 返回选中的菜单项索引
- `ExecuteAction(const MenuConfigItem& item)`：在后台线程中执行 PowerShell 脚本
  - 将 `scriptCode` 按 UTF-16 LE 编码为 Base64
  - 使用 `CreateProcess` 启动 `powershell.exe -NoProfile -EncodedCommand <base64>`
  - 指定 `CREATE_NO_WINDOW`，避免弹出 PowerShell 黑窗
  - 异步执行，不阻塞 UI

### Task 4: 显示项目 — MenuItem

**`MenuItem.h/cpp`** — 实现 `IPluginItem`，核心是左键弹出菜单

- `GetItemName` -> `"快捷菜单"`
- `GetItemId` -> `"LeftClickMenu"`
- `GetItemLableText` -> `"Menu"`
- `GetItemValueText` -> 菜单项数量或空字符串
- `OnMouseEvent`:
  - `MT_LCLICKED`: 调用 `MenuManager::ShowPopupMenu` 弹出菜单，返回 1（拦截默认行为）
  - `MT_RCLICKED`: 返回 0（使用主程序默认右键菜单）

### Task 5: 主插件类 — MenuPlugin

**`MenuPlugin.h/cpp`** — 实现 `ITMPlugin`，整合所有组件

- 单例模式（参考 HotkeyPlugin）
- `OnInitialize`: 加载配置、初始化菜单管理器
- `GetItem`: 返回 MenuItem 实例
- `ShowOptionsDialog`: MVP 阶段可暂不实现，或弹出占位提示
- `GetInfo`: 返回插件名称"快捷菜单插件"等
- `OnExtenedInfo`: 接收 `EI_CONFIG_DIR` 获取配置目录

### Task 6: 编译验证

- 编译项目，修复编译错误
- 验证 x64/Win32 双平台 DLL 输出
- 手动编辑 `LeftClickMenu.ini` 测试菜单弹出和脚本执行

## 阶段二：图标与配置对话框

### Task 7: 图标管理器 — IconManager

**`IconManager.h/cpp`** — 管理内置图标和从外部文件加载的图标

- **内置图标**: 在 `.rc` 中定义图标资源（IDI_ICON_SCRIPT、IDI_ICON_FOLDER 等），通过 `LoadIcon` 加载
- **文件图标**: 调用 `ExtractIconEx` 从 ico/exe/dll 文件中提取图标，并转换为 `HBITMAP`
- 提供统一接口:
  - `HICON GetIcon(const std::wstring& source, int index)`
  - `HBITMAP GetBitmap(const std::wstring& source, int index)`
- 缓存已加载的图标/位图，避免重复加载
- 内置图标名映射表：`"script"` -> `IDI_ICON_SCRIPT`、`"folder"` -> `IDI_ICON_FOLDER` 等
- 插件退出或配置重载时统一销毁缓存资源，避免句柄泄漏

### Task 8: 配置对话框 — OptionsDialog + MenuEditDialog

#### OptionsDialog（主配置对话框）
- `CListCtrl` 列表显示所有菜单项（名称、图标来源、动作摘要）
- 按钮：添加 / 编辑 / 删除 / 上移 / 下移
- 双击打开编辑对话框

#### MenuEditDialog（菜单项编辑对话框）
- 编辑字段：
  - 名称（文本框）
  - 图标选择：下拉框（内置图标列表）+ 浏览按钮（选择 ico/exe/dll 文件）+ 图标预览
  - 分隔线复选框（勾选后名称/脚本灰显）
  - 脚本编辑（多行文本框）
  - 启用复选框
- 确认/取消按钮
- 保存时自动将脚本按 UTF-16 LE 编码为 Base64 写入 INI

### Task 9: 资源文件与内置图标

- 创建 5 个内置图标资源（16x16 + 32x32 的 .ico 文件）：
  - `IDI_ICON_SCRIPT` — 脚本图标
  - `IDI_ICON_FOLDER` — 文件夹图标
  - `IDI_ICON_PROGRAM` — 程序图标
  - `IDI_ICON_URL` — 网址图标
  - `IDI_ICON_SETTINGS` — 设置图标
- 对话框资源模板（`IDD_OPTIONS_DIALOG`、`IDD_MENU_EDIT_DIALOG`）
- 为菜单项添加图标显示（将 `HBITMAP` 设置到 `MIIM_BITMAP`）

## 阶段三：完善与发布

### Task 10: 错误处理与边界测试

- 空菜单、长脚本、含特殊字符脚本、无效图标路径的容错处理
- 输出错误日志到 TrafficMonitor 日志或独立日志文件
- 检查句柄泄漏（图标、位图、进程句柄）

### Task 11: 最终编译验证

- 双平台 Release 编译
- 在 TrafficMonitor 中加载测试
- 确认左键弹窗、脚本执行、配置保存均正常

## 关键设计决策

| 决策 | 方案 |
|------|------|
| 菜单弹出方式 | `OnMouseEvent(MT_LCLICKED)` 返回 1 拦截，用 Win32 `TrackPopupMenu` 实现 |
| 图标支持 | 内置图标（rc 资源）+ 外部文件加载（`ExtractIconEx`），IconManager 统一管理；HICON 需转为 HBITMAP 后再设置到菜单 |
| 动作执行 | PowerShell，`CreateProcess` 异步调用，使用 `-EncodedCommand` + Base64 避免引号和换行问题 |
| 配置存储 | INI 文件，脚本以 Base64 编码存储，与 HotkeyPlugin 使用相同模式 |
| UI 框架 | MFC 对话框，与 HotkeyPlugin 保持一致；MVP 阶段先跳过对话框，手动改 INI 验证 |
| 菜单索引映射 | 菜单命令 ID 直接对应配置项索引，分隔线也占索引但标记为不可选 |
