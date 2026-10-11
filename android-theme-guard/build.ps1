param(
  [string]$Manifest = "AndroidManifest.xml",
  [string]$VersionName = "1.0",
  [int]$VersionCode = 1,
  [string]$ApkName = "ThemeGuard.apk"
)
$ErrorActionPreference = "Continue"
$root  = Split-Path -Parent $MyInvocation.MyCommand.Path
$tools = "D:\ag\tools"                                   # JDK + build-tools + platform
if (-not (Test-Path (Join-Path $tools "jdk17"))) {
  $tools = Join-Path (Split-Path -Parent $root) "tools"
}

if (-not (Test-Path "X:\")) { subst X: (Split-Path -Parent $root) | Out-Null }
$proj = "X:\ThemeGuard"
$out  = Join-Path $proj "build"

function FindHome([string]$dir, [string]$marker) {
  if (Test-Path (Join-Path $dir $marker)) { return $dir }
  $hit = Get-ChildItem $dir -Directory -ErrorAction SilentlyContinue |
         Where-Object { Test-Path (Join-Path $_.FullName $marker) } | Select-Object -First 1
  if ($hit) { return $hit.FullName }
  throw "not found: $marker under $dir"
}

$jdkHome    = FindHome (Join-Path $tools "jdk17") "bin\javac.exe"
$btHome     = FindHome (Join-Path $tools "bt")    "aapt2.exe"
$pfHome     = FindHome (Join-Path $tools "pf")    "android.jar"
$javac      = Join-Path $jdkHome "bin\javac.exe"
$keytool    = Join-Path $jdkHome "bin\keytool.exe"
$aapt2      = Join-Path $btHome  "aapt2.exe"
$d8         = Join-Path $btHome  "d8.bat"
$zipalign   = Join-Path $btHome  "zipalign.exe"
$apksigner  = Join-Path $btHome  "apksigner.bat"
$androidJar = Join-Path $pfHome  "android.jar"

$env:JAVA_HOME = $jdkHome
$env:JAVA_TOOL_OPTIONS = "-Duser.language=en -Duser.country=US"
$env:PATH = (Join-Path $jdkHome "bin") + ";" + $env:PATH

if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $out "classes"), (Join-Path $out "dex") -Force | Out-Null
Write-Output ("proj=" + $proj + " manifest=" + $Manifest + " v" + $VersionName)

# 0) compile resources
$resZip = $null
if (Test-Path (Join-Path $proj "res")) {
  & $aapt2 compile --dir (Join-Path $proj "res") -o (Join-Path $out "res.zip")
  if ($LASTEXITCODE -ne 0) { throw "aapt2 compile failed ($LASTEXITCODE)" }
  $resZip = Join-Path $out "res.zip"
  Write-Output "aapt2 compile OK"
}

# 1) link manifest + resources + assets
$aapt2Args = @("link", "-o", (Join-Path $out "base.apk"), "-I", $androidJar,
  "--manifest", (Join-Path $proj $Manifest), "-A", (Join-Path $proj "assets"),
  "--min-sdk-version", "26", "--target-sdk-version", "34",
  "--version-code", "$VersionCode", "--version-name", "$VersionName")
if ($resZip) { $aapt2Args += $resZip }
& $aapt2 @aapt2Args
if ($LASTEXITCODE -ne 0) { throw "aapt2 link failed ($LASTEXITCODE)" }
Write-Output "aapt2 link OK"

# 2) javac
$srcs = @(Get-ChildItem (Join-Path $proj "src") -Recurse -Filter *.java | ForEach-Object { $_.FullName })
& $javac -encoding UTF-8 -source 11 -target 11 -nowarn -classpath $androidJar -d (Join-Path $out "classes") $srcs
if ($LASTEXITCODE -ne 0) { throw "javac failed ($LASTEXITCODE)" }
Write-Output "javac OK"

# 3) d8
$classes = @(Get-ChildItem (Join-Path $out "classes") -Recurse -Filter *.class | ForEach-Object { $_.FullName })
& $d8 --release --min-api 26 --lib $androidJar --output (Join-Path $out "dex") $classes
if ($LASTEXITCODE -ne 0) { throw "d8 failed ($LASTEXITCODE)" }
Write-Output "d8 OK"

# 4) inject classes.dex
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open((Join-Path $out "base.apk"), 'Update')
$entry = $zip.CreateEntry("classes.dex", [System.IO.Compression.CompressionLevel]::Optimal)
$es = $entry.Open()
$bytes = [System.IO.File]::ReadAllBytes((Join-Path $out "dex\classes.dex"))
$es.Write($bytes, 0, $bytes.Length)
$es.Close()
$zip.Dispose()
Write-Output "dex injected"

# 5) align + sign
& $zipalign -f 4 (Join-Path $out "base.apk") (Join-Path $out "aligned.apk")
if ($LASTEXITCODE -ne 0) { throw "zipalign failed" }
$ks = Join-Path $proj "debug.jks"
if (-not (Test-Path $ks)) {
  & $keytool -genkeypair -keystore $ks -storepass android -keypass android -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=ThemeGuard,O=DSH,C=CN"
  if ($LASTEXITCODE -ne 0) { throw "keytool failed" }
}
& $apksigner sign --ks $ks --ks-pass pass:android --key-pass pass:android --out (Join-Path $out $ApkName) (Join-Path $out "aligned.apk")
if ($LASTEXITCODE -ne 0) { throw "apksigner failed" }
& $apksigner verify --print-certs (Join-Path $out $ApkName)
$apk = Get-Item (Join-Path $out $ApkName)
Write-Output ("APK_OK " + $apk.FullName + " " + $apk.Length + " bytes")
