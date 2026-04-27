$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path "$PSScriptRoot\..\.."
$exe = Join-Path $repoRoot "build\lvgl_preview\neopeak_lvgl_preview.exe"

if (-not (Test-Path $exe)) {
    & "$PSScriptRoot\build_preview.ps1"
}

& $exe
