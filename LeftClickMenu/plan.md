# TrafficMonitor 热键脚本插件 - 开发计划

## 一、需求分析

基于 TrafficMonitor 插件接口规范,需要实现一个插件,功能为:
- 注册全局键盘热键
- 热键触发时执行指定的 PowerShell 脚本
- 提供配置界面管理热键与脚本的映射关系

## 二、技术方案

### 核心技术点
1. **插件接口**: C++ DLL,导出 `TMPluginGetInstance` 函数,实现 `ITMPlugin` 和 `IPluginItem` 接口
2. **全局热键**: 使用 Windows API `RegisterHotKey` + 隐藏的消息窗口接收 `WM_HOTKEY`
3. **脚本执行**: `CreateProcess` 调用 `powershell.exe -ExecutionPolicy Bypass -File "<脚本路径>"`
4. **配置界面**: 使用 MFC 实现选项对话框(与官方插件模板一致)
5. **配置持久化**: INI 文件存储热键映射

### 项目结构
```
HotkeyPlugin/
├── HotkeyPlugin.vcxproj        # MFC DLL 工程
├── PluginInterface.h           # TrafficMonitor 插件接口头文件
├── HotkeyPlugin.h/.cpp         # 主插件类 (实现 ITMPlugin)
├── HotkeyItem.h/.cpp           # 显示项目类 (实现 IPluginItem)
├── HotkeyManager.h/.cpp        # 热键注册与消息处理
├── OptionsDialog.h/.cpp        # 配置对话框
├── HotkeyConfig.h/.cpp         # 配置数据结构与读写
├── HotkeyEditDialog.h/.cpp     # 单条热键编辑子对话框
├── resource.h / HotkeyPlugin.rc # 资源文件
└── dllmain.cpp                 # DLL 入口
```

## 三、分步实施计划

### 步骤 1: 工程搭建
- 创建 MFC DLL 项目(支持 x64/x86)
- 引入 `PluginInterface.h`(从 TrafficMonitor 源码获取)
- 配置输出目录为 `plugins`

### 步骤 2: 配置数据结构 (`HotkeyConfig`)
- 定义结构体 `HotkeyItem`:
  - `UINT modifiers` (MOD_CONTROL/MOD_ALT/MOD_SHIFT/MOD_WIN 组合)
  - `UINT vk` (虚拟键码)
  - `CString scriptPath` (PowerShell 脚本路径)
  - `bool enabled` (是否启用)
  - `CString description` (描述)
- 实现 INI 文件读写: `LoadConfig()` / `SaveConfig()`
- 配置文件路径: 插件 DLL 同目录下 `HotkeyPlugin.ini`

### 步骤 3: 热键管理器 (`HotkeyManager`)
- 创建隐藏的消息窗口(Message-Only Window)用于接收 `WM_HOTKEY`
- `RegisterAll()`: 遍历配置,为每个启用的热键调用 `RegisterHotKey`,分配唯一 ID
- `UnregisterAll()`: 注销所有热键
- `OnHotKey(int id)`: 根据 ID 查找对应脚本,通过 `CreateProcess` 异步执行 PowerShell
- 执行参数: `powershell.exe -ExecutionPolicy Bypass -NoProfile -File "<path>"`
- 记录最后执行的脚本信息(供显示项目使用)

### 步骤 4: 主插件类 (`HotkeyPlugin` 实现 `ITMPlugin`)
- `GetItem(index)`: 返回显示项目对象
- `DataRequired()`: 更新状态文本(如 "热键:3/5" 表示 3 个启用共 5 个)
- `ShowOptionsDialog(hParent)`: 弹出配置对话框,返回 `OR_OPTION_CHANGED`/`OR_OPTION_UNCHANGED`
- `GetInfo(index)`: 返回插件名称、版本、作者等信息
- 构造时:加载配置 → 启动热键管理器
- 析构时:注销热键 → 销毁窗口

### 步骤 5: 显示项目 (`HotkeyItem` 实现 `IPluginItem`)
- `GetItemName()`: "热键脚本"
- `GetItemId()`: "HotkeyScript"
- `GetItemLableText()`: "HK"
- `GetItemValueText()`: 显示启用热键数量,如 "3" 或最后执行脚本名
- `GetItemValueSampleText()`: "00" (用于宽度计算)
- `IsCustomDraw()`: 返回 false(使用主程序默认绘制)
- `OnMouseEvent()`: 左键点击时打开选项对话框

### 步骤 6: 配置对话框 (`OptionsDialog`)
- 主对话框: ListCtrl 显示所有热键映射(热键、脚本路径、状态、描述)
- 按钮: 添加 / 编辑 / 删除 / 启用切换
- 编辑子对话框 (`HotkeyEditDialog`):
  - 热键输入框: 自定义控件捕获按键组合(如 Ctrl+Alt+P)
  - 脚本路径: 编辑框 + 浏览按钮(`CFileDialog`)
  - 描述: 编辑框
  - 启用复选框
- 保存时: 写入 INI → 重新注册热键

### 步骤 7: 构建与测试
- 编译 Release x64 版本
- 用 PluginTester 初步测试加载与界面
- 放入 TrafficMonitor `plugins` 目录实测:
  - 热键注册成功
  - 触发热键能执行脚本
  - 配置能正确保存加载

## 四、关键注意事项

1. **热键冲突**: `RegisterHotKey` 失败需提示用户哪个热键被占用
2. **脚本路径**: 支持相对路径(相对于插件目录)和绝对路径
3. **执行方式**: 异步执行,不阻塞 TrafficMonitor 主线程
4. **错误处理**: 脚本执行失败时通过显示项目提示状态
5. **DPI 兼容**: 显示项目宽度返回 DPI=96 时的值,主程序自动缩放
6. **配置文件编码**: INI 使用 UTF-8 with BOM 或 Unicode,支持中文路径

## 五、参考资源

- 插件开发指南: https://github.com/zhongyang219/TrafficMonitor/wiki/%E6%8F%92%E4%BB%B6%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97
- 官方插件模板: https://github.com/zhongyang219/TrafficMonitorPlugins/tree/main/PluginTemplate
- 插件接口头文件: https://github.com/zhongyang219/TrafficMonitor/blob/master/include/PluginInterface.h
- 示例插件 PluginDemo: https://github.com/zhongyang219/TrafficMonitor/tree/master/PluginDemo
