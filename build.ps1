param(
    [switch]$Deploy,
    [string]$GameDir = "C:\Steam\steamapps\common\MGS_TPP",
    [string]$Generator = "Visual Studio 18 2026"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

# MinHook is a git submodule; make sure it is present before configuring.
if (Test-Path (Join-Path $root ".gitmodules")) {
    git -C $root submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

cmake -S "$root\src\KeyboardSpeedControl" -B "$root\build" -G $Generator -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build "$root\build" --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dll = Join-Path $root "build\Release\KeyboardSpeedControl.dll"
Write-Host "built: $dll"

if ($Deploy) {
    $plugins = Join-Path $GameDir "plugins"
    if (-not (Test-Path $plugins)) { throw "plugins folder not found: $plugins" }

    # Remove the old name so IHHook does not load both DLLs (double scaling).
    $legacy = @(
        (Join-Path $plugins "MGSVSpeed.dll"),
        (Join-Path $plugins "MGSVSpeed.ini"),
        (Join-Path $plugins "MGSVSpeed.log"),
        (Join-Path $plugins "MGSVSpeed.runtime"),
        (Join-Path $GameDir "mod\modules\MGSVSpeed_Core.lua")
    )
    foreach ($f in $legacy) {
        if (Test-Path $f) { Remove-Item $f -Force; Write-Host "removed legacy $f" }
    }

    Copy-Item $dll (Join-Path $plugins "KeyboardSpeedControl.dll") -Force
    Write-Host "deployed DLL to $plugins"

    $luaSrc = Join-Path $root "lua\KeyboardSpeedControl_Core.lua"
    $luaDst = Join-Path $GameDir "mod\modules\KeyboardSpeedControl_Core.lua"
    if (Test-Path $luaSrc) {
        $luaDir = Split-Path -Parent $luaDst
        if (-not (Test-Path $luaDir)) { throw "Infinite Heaven modules folder not found: $luaDir" }
        Copy-Item $luaSrc $luaDst -Force
        Write-Host "deployed IH module to $luaDst"
    }
}
