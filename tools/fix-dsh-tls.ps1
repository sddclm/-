# ============================================================
#  修复 DSH Desktop 因卡巴斯基 HTTPS 扫描导致的 TLS 证书错误
#  现象: "DeepSeek API request to https://api.deepseek.com failed"
#        Node 报 SELF_SIGNED_CERT_IN_CHAIN（间歇性失败）
#  原理: 卡巴斯基"加密连接扫描"对 HTTPS 做中间人替换，签发证书链的根是
#        卡巴自签名根证书。Python/.NET 读系统证书库所以正常，
#        而 Electron/Node 只信任内置 CA 列表 -> 需要额外信任该根证书。
#  用法: powershell -ExecutionPolicy Bypass -File tools\fix-dsh-tls.ps1
# ============================================================
param([string]$DshDataDir = "$env:APPDATA\DSH Desktop")

$ErrorActionPreference = "Stop"

$certs = @()
$certs += Get-ChildItem Cert:\LocalMachine\Root -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match "(?i)kaspersky" }
$certs += Get-ChildItem Cert:\LocalMachine\CA   -ErrorAction SilentlyContinue | Where-Object { $_.Subject -match "(?i)kaspersky" }

if ($certs.Count -eq 0) {
    Write-Host "未找到卡巴斯基证书：本机可能未安装卡巴斯基，或已关闭『加密连接扫描』。" -ForegroundColor Yellow
    Write-Host "若 DSH 仍连不上，请检查其它 TLS 拦截类软件（企业防火墙/根证书缺失）。" -ForegroundColor Yellow
    exit 0
}

if (-not (Test-Path $DshDataDir)) { New-Item -ItemType Directory -Force -Path $DshDataDir | Out-Null }
$out = Join-Path $DshDataDir "kaspersky-ca.pem"

$sb = New-Object System.Text.StringBuilder
foreach ($c in $certs) {
    Write-Host ("导出证书: " + $c.Subject)
    $b = $c.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
    $b64 = [Convert]::ToBase64String($b, [Base64FormattingOptions]::InsertLineBreaks)
    [void]$sb.AppendLine("-----BEGIN CERTIFICATE-----")
    [void]$sb.AppendLine($b64)
    [void]$sb.AppendLine("-----END CERTIFICATE-----")
}
[IO.File]::WriteAllText($out, $sb.ToString(), (New-Object System.Text.UTF8Encoding $false))
Write-Host ("已写入: " + $out) -ForegroundColor Green

# Node/Electron 读取的额外 CA 证书（路径必须为纯 ASCII，含中文的路径会加载失败）
[Environment]::SetEnvironmentVariable("NODE_EXTRA_CA_CERTS", $out, "User")
Write-Host ("已设置用户环境变量 NODE_EXTRA_CA_CERTS = " + $out) -ForegroundColor Green

# 立即生效验证（用 DSH 自带的 Electron 以 Node 模式发起一次真实请求）
$exe = "C:\Program Files\DSH Desktop\DSH Desktop.exe"
$probe = Join-Path $env:TEMP "dsh_tls_probe.js"
@'
const https = require('https');
const req = https.get('https://api.deepseek.com/v1/models', { headers: { Authorization: 'Bearer sk-probe' }, timeout: 15000 }, r => {
  console.log('TLS_OK status=' + r.statusCode); r.resume(); process.exit(0);
});
req.on('timeout', () => { console.log('TLS_TIMEOUT'); process.exit(2); });
req.on('error', e => { console.log('TLS_FAIL ' + (e.code || e.name)); process.exit(1); });
'@ | Set-Content -Path $probe -Encoding ASCII

$old = $env:ELECTRON_RUN_AS_NODE
$oldCa = $env:NODE_EXTRA_CA_CERTS
$env:ELECTRON_RUN_AS_NODE = "1"
$env:NODE_EXTRA_CA_CERTS = $out
$ok = 0
for ($i = 0; $i -lt 5; $i++) {
    $line = (& $exe $probe 2>&1 | Select-String -Pattern "TLS_OK|TLS_FAIL|TLS_TIMEOUT" | ForEach-Object { $_.Line }) -join " "
    if ($line -match "TLS_OK") { $ok++ } else { Write-Host ("  第 " + ($i + 1) + " 次: " + $line) -ForegroundColor DarkYellow }
}
$env:ELECTRON_RUN_AS_NODE = $old
$env:NODE_EXTRA_CA_CERTS = $oldCa

Write-Host ("验证: 5 次请求中成功 " + $ok + " 次") -ForegroundColor Cyan
if ($ok -ge 5) {
    Write-Host "修复成功！请完全退出并重新启动 DSH Desktop 使环境变量生效。" -ForegroundColor Green
} else {
    Write-Host "仍有失败：建议在卡巴斯基中关闭『加密连接扫描』(设置-安全-加密连接扫描)，或把 DSH Desktop.exe 加入信任程序。" -ForegroundColor Red
}
