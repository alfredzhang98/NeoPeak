$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path "$PSScriptRoot\..\.."
$buildDir = Join-Path $repoRoot "build\lvgl_preview"
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "C:\vcpkg" }
$vcpkgToolchain = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

Write-Host "LVGL preview repo: $repoRoot"
Write-Host "Build directory: $buildDir"
Write-Host "Using vcpkg root: $vcpkgRoot"

if (-not (Test-Path $vcpkgToolchain)) {
    throw "vcpkg toolchain not found at $vcpkgToolchain. Install vcpkg and SDL2 first, or set VCPKG_ROOT."
}

function Remove-CMakeConfigureCache {
    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    $cmakeFilesDir = Join-Path $buildDir "CMakeFiles"

    if (Test-Path $cacheFile) {
        Remove-Item -LiteralPath $cacheFile -Force
    }

    if (Test-Path $cmakeFilesDir) {
        Remove-Item -LiteralPath $cmakeFilesDir -Recurse -Force
    }
}

function Import-VsDevEnv {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "MSVC compiler not found. Install Visual Studio Build Tools 2022 with 'Desktop development with C++'."
    }

    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsInstall) {
        throw "MSVC compiler not found. Install Visual Studio Build Tools 2022 with 'Desktop development with C++'."
    }

    $vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat not found at $vcvars."
    }

    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
}

Import-VsDevEnv

$cacheFile = Join-Path $buildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $cache = Get-Content -Raw $cacheFile
    if ($cache -match "SDL2_DIR:PATH=SDL2_DIR-NOTFOUND") {
        Write-Host "Clearing stale CMake cache that could not find SDL2..."
        Remove-CMakeConfigureCache
    }
}

Write-Host "Configuring LVGL preview. First run may download LVGL from GitHub..."
cmake -S "$PSScriptRoot" -B "$buildDir" -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE="$vcpkgToolchain" `
    -DFETCHCONTENT_QUIET=OFF
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed."
}

Write-Host "Building LVGL preview..."
cmake --build "$buildDir"
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed."
}
