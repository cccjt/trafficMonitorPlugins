# 示例 PowerShell 脚本
# 此脚本将在热键触发时执行

Write-Host "Hello from HotkeyPlugin!"
Write-Host "Triggered at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

# 在此处添加你的脚本逻辑
# 例如:打开程序、执行系统操作、发送通知等

# 示例:显示一个通知
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.MessageBox]::Show("热键脚本已执行!", "HotkeyPlugin", 0, 64)
