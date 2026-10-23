param(
    [switch]$Deploy,
    [string]$GameDir = "C:\Steam\steamapps\common\MGS_TPP",
    [string]$Generator = "Visual Studio 18 2026"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

cmake -S "$root\src\MGSVSpeed" -B "$root\build" -G $Generator -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build "$root\build" --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dll = Join-Path $root "build\Release\MGSVSpeed.dll"
Write-Host "built: $dll"

if ($Deploy) {
    $plugins = Join-Path $GameDir "plugins"
    if (-not (Test-Path $plugins)) { throw "plugins folder not found: $plugins" }
    Copy-Item $dll (Join-Path $plugins "MGSVSpeed.dll") -Force
    Write-Host "deployed to $plugins"
}
