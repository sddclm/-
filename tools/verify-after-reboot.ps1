# ============================================================
#  重启后一键验证：SAC 是否已关闭 + 连点器能否运行 + DSH 能否连上 API
#  用法: powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify-after-reboot.ps1
# ============================================================
$ErrorActionPreference = "Continue"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Write-Host "==================== 重启后环境验证 ====================" -ForegroundColor Cyan

# 1) 智能应用控制
Write-Host ""
Write-Host "[1] 智能应用控制 (Smart App Control)" -ForegroundColor Yellow
$k = "HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy"
$sac = (Get-ItemProperty $k -ErrorAction SilentlyContinue).VerifiedAndReputablePolicyState
Write-Host ("    注册表 VerifiedAndReputablePolicyState = " + $sac + "   (0=已关闭, 1=强制, 2=评估)")
if ($sac -eq 0) { Write-Host "    -> 已关闭" -ForegroundColor Green } else { Write-Host "    -> 仍为开启状态，请检查设置界面" -ForegroundColor Red }

# 2) 连点器 exe 能否运行
Write-Host ""
Write-Host "[2] C++ 版连点器" -ForegroundColor Yellow
$exe = Join-Path $Root "dist\AutoClicker.exe"
if (-not (Test-Path $exe)) {
    Write-Host "    未找到 dist\AutoClicker.exe，请先运行 build.ps1" -ForegroundColor Red
} else {
    try {
        $p = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
        Start-Sleep -Milliseconds 1500
        if (-not $p.HasExited) {
            Write-Host "    -> 可以正常运行（窗口已启动，将自动关闭）" -ForegroundColor Green
            Start-Sleep -Milliseconds 800
            $p.CloseMainWindow() | Out-Null
            Start-Sleep -Milliseconds 600
            if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
        } else {
            Write-Host "    -> 进程立即退出，可能仍被策略拦截" -ForegroundColor Red
        }
    } catch {
        Write-Host ("    -> 仍被拦截: " + $_.Exception.Message) -ForegroundColor Red
    }
}

# 3) 脚本版连点器
Write-Host ""
Write-Host "[3] 脚本版连点器（应急方案，始终可用）" -ForegroundColor Yellow
$bat = Join-Path $Root "script-version\启动连点器.bat"
if (Test-Path $bat) { Write-Host "    -> 可用：双击 script-version\启动连点器.bat" -ForegroundColor Green } else { Write-Host "    -> 未找到" -ForegroundColor Red }

# 4) DSH 连 DeepSeek API
Write-Host ""
Write-Host "[4] DSH 连接 DeepSeek API" -ForegroundColor Yellow
$ca = Join-Path $env:APPDATA "DSH Desktop\kaspersky-ca.pem"
Write-Host ("    卡巴根证书: " + (Test-Path $ca) + "   NODE_EXTRA_CA_CERTS=" + [Environment]::GetEnvironmentVariable("NODE_EXTRA_CA_CERTS", "User"))
$exeDsh = "C:\Program Files\DSH Desktop\DSH Desktop.exe"
if ((-not (Test-Path $ca)) -or (-not (Test-Path $exeDsh))) {
    Write-Host "    缺少证书或 DSH，跳过实测（可运行 tools\fix-dsh-tls.ps1 修复）" -ForegroundColor Yellow
} else {
    $probe = Join-Path $env:TEMP "dsh_probe_verify.js"
    @'
const https = require('https');
const r = https.get('https://api.deepseek.com/v1/models', { headers: { Authorization: 'Bearer sk-probe' }, timeout: 15000 }, res => {
  console.log('RESULT_OK ' + res.statusCode); res.resume(); process.exit(0);
});
r.on('timeout', () => { console.log('RESULT_TIMEOUT'); process.exit(2); });
r.on('error', e => { console.log('RESULT_FAIL ' + (e.code || e.name)); process.exit(1); });
'@ | Set-Content -Path $probe -Encoding ASCII
    $env:ELECTRON_RUN_AS_NODE = "1"
    $env:NODE_EXTRA_CA_CERTS = $ca
    $ok = 0
    for ($i = 1; $i -le 5; $i++) {
        $line = (& $exeDsh $probe 2>&1 | Select-String -Pattern "RESULT_" | ForEach-Object { $_.Line }) -join " "
        if ($line -match "RESULT_OK") { $ok++ } else { Write-Host ("    第 " + $i + " 次: " + $line) -ForegroundColor DarkYellow }
    }
    Remove-Item Env:\ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue
    if ($ok -eq 5) { Write-Host "    -> 5/5 请求成功，DSH 网络正常" -ForegroundColor Green }
    else { Write-Host ("    -> 仅 " + $ok + "/5 成功：请重启 DSH Desktop 后再试；若仍失败运行 tools\fix-dsh-tls.ps1") -ForegroundColor Red }
}

Write-Host ""
Write-Host "==================== 验证结束 ====================" -ForegroundColor Cyan
