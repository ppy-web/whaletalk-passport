# Installs isolated ESP32-C3 development tools; does not flash or change the profile.
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
if (!(Test-Path -LiteralPath $IdfPath)) {
  git clone --depth 1 --branch v5.5.3 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git $IdfPath
  if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF download failed' }
}
$idfVersion = git -C $IdfPath describe --tags --exact-match
if ($idfVersion -ne 'v5.5.3') { throw "Expected ESP-IDF v5.5.3 at $IdfPath" }
git -C $IdfPath submodule update --init --recursive --depth 1
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF submodule setup failed' }
$env:IDF_TOOLS_PATH = $ToolsPath
python "$IdfPath/tools/idf_tools.py" --non-interactive install --targets=esp32c3
if ($LASTEXITCODE -ne 0) { throw 'ESP32-C3 tool installation failed' }
python "$IdfPath/tools/idf_tools.py" --non-interactive install-python-env
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF Python environment installation failed' }
$hostEnvironment = $HostToolsPath
if (!(Test-Path -LiteralPath "$hostEnvironment/Scripts/python.exe")) {
  python -m venv $hostEnvironment
  if ($LASTEXITCODE -ne 0) { throw 'Host test environment creation failed' }
}
& "$hostEnvironment/Scripts/python.exe" -m pip install -r "$PSScriptRoot/host-requirements.txt"
if ($LASTEXITCODE -ne 0) { throw 'Host test tools installation failed' }
Write-Output 'Installed. Dot-source tools/activate.ps1, then python tools/validate.py --firmware'
