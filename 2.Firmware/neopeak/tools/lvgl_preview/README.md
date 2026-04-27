# NeoPeak LVGL Preview

This folder builds the NeoPeak LVGL UI as a Windows desktop preview using SDL2.
The preview and the ESP-IDF firmware share `components/ui/imu_cube_ui.cpp`.

## Install once

Install these tools on Windows:

- Git
- CMake
- Ninja
- Visual Studio Build Tools with the C++ desktop workload
- vcpkg

Install SDL2 with vcpkg:

```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install sdl2:x64-windows
```

If vcpkg is not installed at `C:\vcpkg`, set `VCPKG_ROOT` before building:

```powershell
$env:VCPKG_ROOT = "D:\Tools\vcpkg"
```

## Build

From the repo root:

```powershell
.\tools\lvgl_preview\build_preview.ps1
```

Or manually:

```powershell
cmake -S tools/lvgl_preview -B build/lvgl_preview -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build/lvgl_preview
```

## Run

```powershell
.\tools\lvgl_preview\run_preview.ps1
```

The preview opens a 240x240 LVGL window and drives the IMU cube with fake roll
and pitch values.

## Firmware build

The ESP-IDF firmware uses the same UI source through the `ui` component. Build
the firmware normally after ESP-IDF is installed:

```powershell
& D:\SDKs\.espressif\v6.0\esp-idf\export.ps1
idf.py build
```
