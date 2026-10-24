param(
    [string]$GameDir   = "C:\Steam\steamapps\common\MGS_TPP",
    [string]$BundleDir = "bundle",
    [switch]$Deploy,
    [switch]$Zip
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path $GameDir)) {
    throw "GameDir not found: $GameDir"
}
$gameRoot = (Resolve-Path $GameDir).Path.TrimEnd('\', '/')
if (-not (Test-Path (Join-Path $gameRoot "mgsvtpp.exe"))) {
    throw "GameDir does not look like MGS_TPP: $gameRoot"
}
$gameName = Split-Path -Leaf $gameRoot

# Build first; the bundle (not build.ps1) is the source of truth for deployment.
& (Join-Path $root "build.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dll = Join-Path $root "build\Release\KeyboardSpeedControl.dll"
$lua = Join-Path $root "lua\KeyboardSpeedControl_Core.lua"
foreach ($f in @($dll, $lua)) {
    if (-not (Test-Path $f)) { throw "missing $f - build failed" }
}

$out = Join-Path $root $BundleDir
if (Test-Path $out) { Remove-Item $out -Recurse -Force }

# Mirror the game root: bundle/<GameDir name>/plugins, bundle/<GameDir name>/mod/modules
$stage = Join-Path $out $gameName
New-Item -ItemType Directory -Force (Join-Path $stage "plugins")     | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage "mod\modules") | Out-Null

Copy-Item $dll (Join-Path $stage "plugins\KeyboardSpeedControl.dll")
Copy-Item $lua (Join-Path $stage "mod\modules\KeyboardSpeedControl_Core.lua")

if ($Zip) {
    $zip = Join-Path $root "KeyboardSpeedControl.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path $stage -DestinationPath $zip
    Write-Host "zipped: $zip"
}

if ($Deploy) {
    # Copy the staged tree into the real game folder, preserving relative paths.
    Get-ChildItem $stage -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($stage.Length + 1)
        $target = Join-Path $gameRoot $relative
        $targetDir = Split-Path -Parent $target
        if (-not (Test-Path $targetDir)) {
            New-Item -ItemType Directory -Force $targetDir | Out-Null
        }
        Copy-Item $_.FullName $target -Force
        Write-Host "deployed $relative"
    }

    # Old name from before the rename must not be loaded alongside this DLL.
    foreach ($legacy in @(
        (Join-Path $gameRoot "plugins\MGSVSpeed.dll"),
        (Join-Path $gameRoot "plugins\MGSVSpeed.ini"),
        (Join-Path $gameRoot "plugins\MGSVSpeed.log"),
        (Join-Path $gameRoot "plugins\MGSVSpeed.runtime"),
        (Join-Path $gameRoot "mod\modules\MGSVSpeed_Core.lua")
    )) {
        if (Test-Path $legacy) {
            Remove-Item $legacy -Force
            Write-Host "removed legacy $legacy"
        }
    }
}

Write-Host "bundle: $out"
