# Dot-source after setup: . ./tools/activate.ps1
param(
  [string]$IdfPath = $env:WHALETALK_IDF_PATH,
  [string]$ToolsPath = $env:IDF_TOOLS_PATH,
  [string]$HostToolsPath = $env:WHALETALK_HOST_TOOLS_PATH
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$IdfPath) { $IdfPath = Join-Path $projectRoot '.tools/esp-idf-v5.5.3' }
if (!$ToolsPath) { $ToolsPath = Join-Path $projectRoot '.tools/espressif' }
if (!$HostToolsPath) { $HostToolsPath = Join-Path $projectRoot '.tools/host' }
if (!(Test-Path -LiteralPath "$IdfPath/export.ps1")) { throw "ESP-IDF missing at $IdfPath; run tools/setup.ps1" }
$env:IDF_TOOLS_PATH = $ToolsPath
. "$IdfPath/export.ps1"
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF activation failed' }
$hostCompiler = Join-Path $HostToolsPath 'Lib/site-packages/ziglang'
if (Test-Path -LiteralPath "$hostCompiler/zig.exe") { $env:PATH = "$hostCompiler;$env:PATH" }
