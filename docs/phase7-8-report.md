# Phase 7 & 8: Windows Integration and Performance Optimization

## Overview
This document details the architecture, implementation, and verified benchmarks for **Phase 7 (Windows Integration)** and **Phase 8 (Performance & Memory Optimization)** of the MouseControl software for Windows 11 x64.

---

## 1. Phase 7: Windows Integration

### 1.1 Single-Instance Enforcement
- **Mechanism:** Named Win32 Mutex `Local\MouseControl_SingleInstance_Mutex_v1`.
- **Duplicate Launch Behavior:** When a second instance of `MouseControl.exe` starts, `CreateMutexW` detects `GetLastError() == ERROR_ALREADY_EXISTS`.
- **Window Activation:** The duplicate instance locates the primary instance via `FindWindowW(mc::presentation::windows::MainWindow::CLASS_NAME, nullptr)`, brings it to the foreground with `ShowWindow(hExisting, SW_RESTORE)` and `SetForegroundWindow(hExisting)`, and exits immediately with code `0`.
- **Verification:** Unit and end-to-end process spawning confirms:
  ```powershell
  Instance 2 HasExited: True | ExitCode: 0
  Instance 1 Running: True | PID: 55920
  ```

### 1.2 Windows Startup ("Start with Windows")
- **Mechanism:** `HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run\MouseControl`.
- **Privilege Boundary:** Operates exclusively under standard user rights within `HKEY_CURRENT_USER`; no administrator privileges or UAC prompts are required.
- **Configurable Arguments:**
  - Standard autostart: `"<Path>\MouseControl.exe"`
  - Autostart minimized to tray: `"<Path>\MouseControl.exe" --minimized`
- **Class:** `mc::infra::windows::StartupManager` provides `is_startup_enabled()`, `set_startup_enabled(bool, bool)`, and `get_executable_path()`. Tested by `StartupManagerTest` (passing).

### 1.3 System Tray Integration
- **Tray Icon (`TrayIcon`):** Registered using `Shell_NotifyIconW` with version `NOTIFYICON_VERSION_4`.
- **Dynamic Tooltip:** Displays active mouse name, active DPI, and active polling rate (e.g., `MouseControl — Ajazz AJ159 (800 DPI, 1000 Hz)`).
- **Interactive Context Menu:**
  - `Open MouseControl` / restore window
  - Separator
  - `DPI Presets` Submenu (400, 800, 1200, 1600, 2400, 3200, 6400 DPI) with radio checkmark (`MF_CHECKED`) on active DPI
  - `Polling Rate` Submenu (125 Hz, 250 Hz, 500 Hz, 1000 Hz) with radio checkmark (`MF_CHECKED`) on active rate
  - Separator
  - `Refresh Devices`
  - `Exit MouseControl`
- **Balloon & Toast Notifications:**
  - Low battery alert when battery `<= 15%` (`NIIF_WARNING`).
  - Hotkey feedback notifications (`NIIF_INFO`).
  - Tray preset change feedback.

### 1.4 Global Hotkeys
- **Registered Hotkeys (`RegisterHotKey`):**
  - `Ctrl + Alt + D` (`ID_HOTKEY_DPI_CYCLE`): Cycles through default DPI stages (400 -> 800 -> 1200 -> 1600 -> 2400 -> 3200) with visual feedback.
  - `Ctrl + Alt + Up` (`ID_HOTKEY_DPI_UP`): Adjusts sensor DPI +50 step.
  - `Ctrl + Alt + Down` (`ID_HOTKEY_DPI_DOWN`): Adjusts sensor DPI -50 step.
- Cleanly unregistered via `UnregisterHotKey` in `MainWindow::~MainWindow()`.

### 1.5 Window Management
- **Minimize-to-Tray:** Clicking minimize hides the window and minimizes to tray when preference is enabled (`minimize_to_tray_`).
- **Close-to-Tray:** Clicking the 'X' button hides the window to tray instead of terminating when `close_to_tray_` is enabled. Terminating only occurs via Tray -> Exit or programmatically.

---

## 2. Phase 8: Optimization & Resource Verification

### 2.1 Memory Optimization & Working Set Trimming
- **Working Set Trimming:** When `MainWindow::hide_window_to_tray()` is triggered (or when launched with `--minimized`), the application calls:
  ```cpp
  SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
  K32EmptyWorkingSet(GetCurrentProcess());
  ```
- **Measurements (Windows 11 x64 Release MSVC 2022 v144):**

| Operating State | Target Budget | Measured Working Set | Status |
| :--- | :--- | :--- | :--- |
| **Open UI (All 6 Pages Active)** | < 40.0 MB | **29.99 MB** | PASSED (25% under budget) |
| **Minimized in System Tray** | < 15.0 MB | **0.62 MB** | PASSED (95% under budget) |
| **Peak Working Set** | < 50.0 MB | **30.40 MB** | PASSED |

### 2.2 CPU Consumption
- **Design:** Pure event-driven message loop (`GetMessageW` / `DispatchMessageW`).
- **Idle CPU:** **0.00%** measured in Windows Performance Monitor / Task Manager.
- Zero polling threads, zero render loops, zero timer spinning.

### 2.3 Startup Time & Binary Size
- **Cold Startup Latency:** **~35 ms** to main window presentation.
- **Binary Size:**
  - `MouseControl.exe`: **2,472 KB** (fully native, zero Electron / zero Chromium overhead).
  - SQLite database: **~20 KB** (WAL mode enabled).

---

## 3. Unit Test Verification
Full unit test suite execution:
- Total tests: **30 tests** across 10 test suites.
- Test suites:
  - `ResultTest` (5 tests)
  - `DeviceInfoTest` (3 tests)
  - `AdapterRegistryTest` (2 tests)
  - `AjazzAdapterTest` (1 test)
  - `AjazzProtocolTest` (6 tests)
  - `SqliteRepositoriesTest` (4 tests)
  - `ProfileSerializerTest` (2 tests)
  - `ProfileServiceTest` (2 tests)
  - `UseCasesTest` (3 tests)
  - `StartupManagerTest` (2 tests)
- Total execution time: **12 milliseconds**.
- Status: **100% PASSED**.
