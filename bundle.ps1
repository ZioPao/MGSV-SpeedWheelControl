param(
    [string]$GameDir   = "C:\Steam\steamapps\common\MGS_TPP",
    [string]$BundleDir = "bundle",
    [switch]$Deploy,
    [switch]$Zip,
    [switch]$Mgsv,
    [string]$DmmMods   = $env:DMM_MODS
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

$dll = Join-Path $root "build\Release\SpeedWheel.dll"
$lua = Join-Path $root "lua\SpeedWheel_Core.lua"
foreach ($f in @($dll, $lua)) {
    if (-not (Test-Path $f)) { throw "missing $f - build failed" }
}

$out = Join-Path $root $BundleDir
if (Test-Path $out) { Remove-Item $out -Recurse -Force }

# Mirror the game root: bundle/<GameDir name>/plugins, bundle/<GameDir name>/mod/modules
$stage = Join-Path $out $gameName
New-Item -ItemType Directory -Force (Join-Path $stage "plugins")     | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage "mod\modules") | Out-Null

Copy-Item $dll (Join-Path $stage "plugins\SpeedWheel.dll")
Copy-Item $lua (Join-Path $stage "mod\modules\SpeedWheel_Core.lua")

if ($Zip) {
    $zip = Join-Path $root "SpeedWheel.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path $stage -DestinationPath $zip
    Write-Host "zipped: $zip"
}

if ($Mgsv) {
    # DMM / SnakeBite package: a zip with metadata.xml at the root and the game
    # files under GameDir/. Same layout DMM writes for SpeedWheelControl-v1.2.0.
    $cmake = Get-Content (Join-Path $root "src\SpeedWheel\CMakeLists.txt") -Raw
    if ($cmake -notmatch 'project\(SpeedWheel\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        throw "could not read the version from src/SpeedWheel/CMakeLists.txt"
    }
    $version = $Matches[1]
    $name = "Speed Wheel Control"

    $dllHash = (Get-FileHash -Algorithm MD5 -Path $dll).Hash
    $luaHash = (Get-FileHash -Algorithm MD5 -Path $lua).Hash
    $metadata = @"
<?xml version="1.0" encoding="utf-8"?>
<ModEntry xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" Name="$name" Version="$version" Author="ZioPao" Website="https://github.com/ZioPao/MGSV-SpeedWheelControl">
  <MGSVersion Version="1.0.15.4" />
  <SBVersion Version="0.9.2.5" />
  <Description>Dynamic on-foot movement speed for M+KB players, similar to how Splinter Cell did it back in the day.</Description>
  <QarEntries />
  <FpkEntries />
  <FileEntries>
    <FileEntry FilePath="/mod/modules/SpeedWheel_Core.lua" ContentHash="$luaHash" />
    <FileEntry FilePath="/plugins/SpeedWheel.dll" ContentHash="$dllHash" />
  </FileEntries>
  <WmvEntries />
</ModEntry>
"@

    $dist = Join-Path $root "dist"
    New-Item -ItemType Directory -Force $dist | Out-Null
    $mgsvPath = Join-Path $dist "SpeedWheelControl.mgsv"
    if (Test-Path $mgsvPath) { Remove-Item $mgsvPath -Force }

    $metaTmp = Join-Path ([System.IO.Path]::GetTempPath()) "speedwheel_metadata.xml"
    [System.IO.File]::WriteAllText($metaTmp, $metadata, (New-Object System.Text.UTF8Encoding($false)))

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::Open($mgsvPath, "Create")
    try {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $metaTmp, "metadata.xml", "Optimal") | Out-Null
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $lua, "GameDir/mod/modules/SpeedWheel_Core.lua", "Optimal") | Out-Null
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $dll, "GameDir/plugins/SpeedWheel.dll", "Optimal") | Out-Null
    } finally {
        $archive.Dispose()
        Remove-Item $metaTmp -Force -ErrorAction SilentlyContinue
    }
    Write-Host "mgsv: $mgsvPath"

    if ($DmmMods) {
        if (-not (Test-Path $DmmMods)) {
            throw "DMM Mods folder not found: $DmmMods (pass -DmmMods or set DMM_MODS)"
        }
        Copy-Item $mgsvPath (Join-Path $DmmMods "SpeedWheelControl.mgsv") -Force
        Write-Host "copied to DMM: $DmmMods"
    } else {
        Write-Host "DMM_MODS not set; skipping copy to DMM"
    }
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

    # Old names from before the rename must not be loaded alongside this DLL.
    foreach ($legacy in @(
        (Join-Path $gameRoot "plugins\MGSVSpeed.dll"),
        (Join-Path $gameRoot "plugins\MGSVSpeed.ini"),
        (Join-Path $gameRoot "plugins\MGSVSpeed.log"),
        (Join-Path $gameRoot "plugins\MGSVSpeed.runtime"),
        (Join-Path $gameRoot "mod\modules\MGSVSpeed_Core.lua"),
        (Join-Path $gameRoot "plugins\KeyboardSpeedControl.dll"),
        (Join-Path $gameRoot "plugins\KeyboardSpeedControl.ini"),
        (Join-Path $gameRoot "plugins\KeyboardSpeedControl.log"),
        (Join-Path $gameRoot "plugins\KeyboardSpeedControl.runtime"),
        (Join-Path $gameRoot "mod\modules\KeyboardSpeedControl_Core.lua")
    )) {
        if (Test-Path $legacy) {
            Remove-Item $legacy -Force
            Write-Host "removed legacy $legacy"
        }
    }
}

Write-Host "bundle: $out"
