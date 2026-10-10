# ============================================================
#  鼠标连点器 UI 回归测试脚本
#  用法: powershell -NoProfile -ExecutionPolicy Bypass -File tests\ui_test.ps1
#  作用: 启动 exe -> 模拟点击各控件 -> 截图 -> 校验配置 -> 输出结论
#  注意: 测试期间会把鼠标移到安全位置(屏幕右下角), 并点击固定坐标
# ============================================================
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Exe  = Join-Path $Root "dist\AutoClicker.exe"
$Shots = Join-Path $Root "shots"
New-Item -ItemType Directory -Force -Path $Shots | Out-Null

Get-Process AutoClicker -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class T {
   [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
   [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
   [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
   [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
   [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
   [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
   [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
}
"@
function Grab([int]$x,[int]$y,[int]$w,[int]$h,[string]$out) {
   $bmp = New-Object System.Drawing.Bitmap $w, $h
   $g = [System.Drawing.Graphics]::FromImage($bmp)
   $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size $w, $h))
   $g.Dispose(); $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}
function Click([int]$x,[int]$y) {
   [T]::SetCursorPos($x, $y) | Out-Null
   Start-Sleep -Milliseconds 180
   [T]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero)
   Start-Sleep -Milliseconds 70
   [T]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
   Start-Sleep -Milliseconds 220
}

$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds 2
$p.Refresh(); $h = $p.MainWindowHandle
[T]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0043) | Out-Null
Start-Sleep -Milliseconds 600
$rc = New-Object T+RECT
[T]::GetWindowRect($h, [ref]$rc) | Out-Null
$wx = $rc.Left; $wy = $rc.Top; $ww = $rc.Right - $rc.Left; $wh = $rc.Bottom - $rc.Top
Write-Host ("窗口: " + $wx + "," + $wy + " 尺寸 " + $ww + "x" + $wh)

$results = @()

# 1. 初始界面
Grab $wx $wy $ww $wh (Join-Path $Shots "test_01_idle.png")
$results += "01 初始界面截图 OK"

# 2. 预设按钮: 10ms (客户区 133,296)
Click ($wx + 133) ($wy + 296)
Grab $wx $wy $ww $wh (Join-Path $Shots "test_02_preset.png")
$results += "02 预设按钮(10ms)"

# 3. 指定次数 (客户区 255,533)
Click ($wx + 255) ($wy + 533)
Grab $wx $wy $ww $wh (Join-Path $Shots "test_03_repeat.png")
$results += "03 指定次数模式"

# 4. 热键启动 + 自动停止(100次 x 10ms = 1s)
[System.Windows.Forms.SendKeys]::SendWait("{F6}")
Start-Sleep -Milliseconds 2600
Grab $wx $wy $ww $wh (Join-Path $Shots "test_04_autostop.png")
$results += "04 热键启动/自动停止"

# 5. 最小化 / 恢复
Click ($wx + 429) ($wy + 23)
Start-Sleep -Milliseconds 800
$min = [T]::IsIconic($h)
$results += ("05 最小化按钮 -> iconic=" + $min)

# 6. 关闭并校验配置落盘
[T]::PostMessage($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1200
$alive = (Get-Process AutoClicker -ErrorAction SilentlyContinue) -ne $null
$results += ("06 关闭按钮 -> 进程已退出=" + (-not $alive))
$ini = Join-Path $Root "dist\MouseClicker.ini"
if (Test-Path $ini) {
    $results += "07 便携配置已写入 dist\MouseClicker.ini"
    Get-Content $ini | ForEach-Object { Write-Host ("      " + $_) }
} else {
    $results += "07 配置未找到(检查 exe 目录写权限)"
}

Write-Host ""
Write-Host "===== 测试结果 =====" -ForegroundColor Cyan
$results | ForEach-Object { Write-Host ("  " + $_) }
Write-Host ("截图目录: " + $Shots) -ForegroundColor Green
