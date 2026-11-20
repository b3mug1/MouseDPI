# MouseControl Release & Deployment Guide

This guide covers building, packaging, and installing MouseControl v1.0.0 on Windows 11 x64 systems.

---

## 1. Prerequisites

- **Operating System:** Windows 11 x64
- **Compiler:** Microsoft Visual C++ 2022 (MSVC Toolset v144 or newer)
- **Build System:** CMake 3.25+
- **Package Manager:** vcpkg (Manifest Mode)
- **Windows SDK:** 10.0.26100.0 or newer

---

## 2. Compiling the Release Build

Execute from the project root directory:

```powershell
# 1. Configure build directory with CMake and vcpkg toolchain
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake"

# 2. Build all Release binaries
cmake --build build --config Release
```

Target output files generated in `build\Release\`:
- `MouseControl.exe`: Native Win32 UI application (~620 KB)
- `device_inspector.exe`: Command-line HID diagnostics tool
- `mousedpi_tests.exe`: Automated test suite
- `fmt.dll`, `spdlog.dll`, `sqlite3.dll`: Required runtime dependencies

---

## 3. Running the Automated Tests

Verify system integrity by running the test suite:

```powershell
.\build\Release\mousedpi_tests.exe
```

All 30 unit tests across 10 test suites should report `PASSED`.

---

## 4. Generating Distribution Packages

### 4.1 Portable ZIP Package (via CPack)
Run CPack to generate a standalone zip archive:

```powershell
cpack --config build/CPackConfig.cmake -C Release -B dist
```

Output: `dist/MouseControl-1.0.0-win64.zip` containing all executables and bundled DLLs in a self-contained directory.

### 4.2 Inno Setup Installer (Optional)
If Inno Setup 6 is installed:

```powershell
iscc installer/MouseControl_Setup.iss
```

Output: `dist/MouseControl-Setup-1.0.0.exe`

---

## 5. End-User Installation

### 5.1 PowerShell Installer (Recommended for local deployments)
To install for the current user into `%LOCALAPPDATA%\MouseControl`:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\install.ps1 -CreateDesktopShortcut -StartAfterInstall
```

- Installs all payload files.
- Creates Start Menu shortcut.
- Creates Desktop shortcut.
- Registers standard Windows Add/Remove Programs uninstall entry.
- Requires no administrator privileges or UAC elevation.

### 5.2 Uninstallation
Run from Start Menu / Settings or via PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File "$env:LOCALAPPDATA\MouseControl\uninstall.ps1"
```
