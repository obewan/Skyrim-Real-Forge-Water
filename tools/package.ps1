# Builds the mod archive for Nexus: dist\RealForgeWater-<version>.zip, with the DLL, its INI and the
# vanilla wave texture, laid out as the game's Data folder.
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1
#
# Run build.cmd first. The texture comes from tools\extract_wave_texture.py.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$version = (Get-Content (Join-Path $root "vcpkg.json") -Raw | ConvertFrom-Json)."version-semver"
$dll = Join-Path $root "build\release\RealForgeWater.dll"
$ini = Join-Path $root "data\SKSE\Plugins\RealForgeWater.ini"
$texture = Join-Path $root "data\Textures\RealForgeWater\Water\DefaultWater.dds"

if (-not (Test-Path $dll)) { throw "$dll not found: run build.cmd first." }
if (-not (Test-Path $texture)) {
    throw "$texture not found: run tools\extract_wave_texture.py with your Skyrim Data folder first."
}

$stage = Join-Path $root "dist\stage"
if (Test-Path $stage) { Remove-Item -Recurse -Force -LiteralPath $stage }
New-Item -ItemType Directory -Force (Join-Path $stage "SKSE\Plugins") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $stage "Textures\RealForgeWater\Water") | Out-Null
Copy-Item $dll, $ini (Join-Path $stage "SKSE\Plugins")
Copy-Item $texture (Join-Path $stage "Textures\RealForgeWater\Water")

$zip = Join-Path $root "dist\RealForgeWater-$version.zip"
Compress-Archive -Path (Join-Path $stage "SKSE"), (Join-Path $stage "Textures") -DestinationPath $zip -Force
Remove-Item -Recurse -Force -LiteralPath $stage
Write-Host "packaged $zip"
