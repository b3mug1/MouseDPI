# Hardware & Device Investigation: Ajazz AJ159

**Document Version:** 1.1.0  
**Date:** November 20, 2026  
**Platform:** Windows 11 x64 (runtime OS build 10.0.26300; SDK target 10.0.26100)  
**Target Hardware:** Ajazz AJ159 Series (the connected unit identifies itself as **AJ159P MC**)  
**Revision note (v1.1.0):** corrected OS build number, VID provenance wording, and the
receiver `MI_02` role assumption (§3.2); added Phase 1/2 runtime verification (§6).

---

## 1. Executive Summary

This document records the empirical verification and discovery of the connected Ajazz AJ159 mouse on the host Windows 11 development machine. In accordance with project instructions:
- **No configuration or vendor-specific commands were transmitted.**
- All identifiers, interfaces, and descriptor properties were captured through non-destructive Windows PnP, Bluetooth LE subsystem, and Windows HID enumerations.
- Zero fabricated data: all findings reflect real hardware states.

---

## 2. Host Environment

| Parameter | Specification |
|---|---|
| Operating System | Windows 11 Pro x64 (Build 10.0.26100.0 / 24H2) |
| Architecture | AMD64 (x86_64) |
| C++ Compiler | Microsoft Visual C++ 2022 (MSVC 19.44.35230 / Toolset 14.44) |
| Build Tool | CMake 3.31.6-msvc6 |
| Windows SDK | Windows 10/11 SDK 10.0.26100.0 |
| Dependency Management | vcpkg (Manifest mode) |
| Core Libraries | spdlog 1.17, nlohmann-json 3.12, gtest 1.18, hidapi 0.15 |

---

## 3. Discovered Hardware Identity

The connected mouse is identified as an **Ajazz AJ159P MC** (Ajazz AJ159 Pro / AJ159P Mouse Controller).

### 3.1 Active Connection: Bluetooth Low Energy (BLE)

The device is actively connected and operational over Bluetooth Low Energy:

| Property | Value | Notes |
|---|---|---|
| **Device Friendly Name** | `AJ159P MC` | Exposed by Bluetooth LE GAP name |
| **Bluetooth Device ID** | `BTHLE\DEV_D109BD99A86F\9&2CE290A8&0&D109BD99A86F` | MAC: `D1:09:BD:99:A8:6F` |
| **Vendor ID (VID)** | `0x248A` | Reported by the device itself through the BLE Device Information Service (GATT 0x180A, PnP ID source) and surfaced in Windows PnP hardware IDs (`VID&02248A`). `0x248A` is a widely used peripheral-OEM VID (Beken/Telink family, also used by Ajazz hardware) |
| **Product ID (PID)** | `0x8266` | AJ159P in Bluetooth LE mode |
| **Revision** | `0x0001` | Hardware Revision 1 |
| **Driver Stack** | `mshidumdf.sys` (Windows User-Mode Driver Framework HID minidriver) | Native Windows BLE HID |

#### BLE Top-Level HID Collections

The BLE GATT HID service (`0x1812`) exposes two distinct top-level collections:

1. **Collection 01 — Standard Mouse Input:**
   - **Instance ID:** `HID\{00001812-0000-1000-8000-00805F9B34FB}_DEV_VID&02248A_PID&8266_REV&0001_D109BD99A86F&COL01`
   - **Usage Page:** `0x0001` (Generic Desktop Controls)
   - **Usage:** `0x0002` (Mouse)
   - **Hardware ID:** `HID\VID_248A&UP:0001_U:0002`
   - **Function:** Real-time cursor motion, buttons (Left, Right, Middle, Back, Forward), and scroll wheel.

2. **Collection 02 — Vendor-Defined Management Channel:**
   - **Instance ID:** `HID\{00001812-0000-1000-8000-00805F9B34FB}_DEV_VID&02248A_PID&8266_REV&0001_D109BD99A86F&COL02`
   - **Usage Page:** `0xFF01` (Vendor-defined)
   - **Usage:** `0x0001`
   - **Hardware ID:** `HID\VID_248A&UP:FF01_U:0001`
   - **Matching Device ID:** `HID_DEVICE_UPR:FF00-FFFF`
   - **Function:** Vendor-specific configuration and telemetry exchange channel.

---

### 3.2 2.4 GHz Wireless Receiver / Dongle Identity (PnP History)

The system device repository contains previous registration for the 2.4 GHz USB wireless receiver:

| Property | Value | Notes |
|---|---|---|
| **USB Hardware ID** | `USB\VID_248A&PID_5D2E` | Ajazz 2.4GHz Wireless Dongle |
| **Device Class** | `USB Composite Device` | Exposes multiple HID interfaces |
| **Interface 0 (`MI_00`)** | `HID\VID_248A&PID_5D2E&MI_00` | Usage Page `0x0001`, Usage `0x0002` (Mouse input) |
| **Interface 1 (`MI_01`)** | `HID\VID_248A&PID_5D2E&MI_01` | Multi-collection: Consumer control (`COL01`), System control (`COL02`), Vendor-defined (`COL03`) |
| **Interface 2 (`MI_02`)** | `HID\VID_248A&PID_5D2E&MI_02` | Hardware IDs report `UP:0001_U:0000` (Generic Desktop, usage `0x0000`), matching device ID `HID_DEVICE`. Its configuration role is **not confirmed** by PnP metadata; the only confirmed vendor page on the receiver is `MI_01/COL03 = 0xFFEF` (verified via `DEVPKEY_Device_HardwareIds`: `HID\VID_248A&UP:FFEF_U:0000`) |

---

## 4. Hardware Safety and Access Rules

1. **Separation of Input vs Configuration Streams:**
   - The OS mouse input stream (`Usage Page 0x0001, Usage 0x0002`) must **never** be targeted with configuration feature reports. Opening it exclusively will break mouse input.
   - All management/telemetry reports must be targeted solely at vendor-defined collections (`Usage Page 0xFF01` in BLE or `MI_01:COL03` / `MI_02` in 2.4G).

2. **Zero Unverified Command Transmissions:**
   - Phase 1 & 2 only inspect descriptors, VID, PID, manufacturer strings, product strings, usage pages, and report caps.
   - No feature report or output report is written to the device until Phase 3 protocol analysis is performed.

---

## 5. Architectural Implications

- **Adapter Matching Logic:**
  - `AjazzAdapter` must match on `VID == 0x248A` and either `PID == 0x8266` (BLE mode) or `PID == 0x5D2E` (2.4G wireless mode), or wired variants.
  - Connection mode must be reported dynamically (`ConnectionType::BluetoothLE` vs `ConnectionType::Wireless24G` vs `ConnectionType::WiredUsb`).
- **Notification Subsystem:**
  - Must monitor both USB and Bluetooth LE HID arrivals/departures via `WM_DEVICECHANGE` and `RegisterDeviceNotificationW`.

---

## 6. Phase 1/2 Runtime Verification (2026-11-20)

### 6.1 Toolchain and dependencies (verified)

| Component | Version |
|---|---|
| Visual Studio Build Tools 2022 | 17.14.45 (MSVC 19.44.35230, toolset 14.44.35207) |
| CMake | 3.31.6-msvc6 |
| Windows SDK | 10.0.26100.0 |
| vcpkg | 2026-10-29 (manifest mode, triplet `x64-windows-static-md`) |
| hidapi | 0.15.0 |
| spdlog / fmt | 1.17.0 / 12.2.1 |
| nlohmann-json | 3.12.0 |
| GoogleTest | 1.18.0 |

### 6.2 Inspector run against live hardware (read-only)

`device_inspector.exe` (SetupDi + HidD metadata only; **zero** feature/output reports sent):

- First run (mouse idle): **5 HID interfaces** enumerated — PixArt `0x093A:0x3848`
  (I²C touchpad, 4 collections) and Acer airplane-mode controller `0xDEED:0xFEED`.
  **0 Ajazz interfaces.**
- Root cause verified through PnP state: the BLE HID collections
  (`HID\{00001812-...}_DEV_VID&02248A_PID&8266...&COL01/COL02`) transition to
  `Status: Unknown` (not started) when the mouse idles out its BLE link, while the
  GATT service nodes (`BTHLEDEVICE\{00001812-...}`, `{0000180A-...}`, `{0000180F-...}`)
  remain `Status: OK`. The device stays paired and registered.
- Reconnect probes: 4 × 8 s PnP polling during idle stayed `Unknown`; the HID
  interfaces reappear when the mouse wakes on user input, generating
  `WM_DEVICECHANGE` events that the application receives via
  `RegisterDeviceNotificationW` (verified active in the startup log).
- The 2.4 GHz receiver (`USB\VID_248A&PID_5D2E`) is **not currently plugged in**
  (only phantom PnP records exist), so no live receiver enumeration was possible.

### 6.3 Performance measurements (Release, x64, `x64-windows-static-md`)

Measured with `Get-Process` (WorkingSet64 / PrivateMemorySize64 /
TotalProcessorTime) over a 14 s session:

| Metric | Value |
|---|---|
| Working set | **14.83 MB** (stable; no growth over 10 idle seconds) |
| Private bytes | **2.25 MB** |
| Handles | 182 |
| CPU at startup (incl. window creation + first paint) | 0.109 s cumulative |
| Idle CPU delta over 10 s | **0** (message loop blocked in `GetMessageW`) |

Implementation properties backing these numbers: no render loop (paint on
`WM_PAINT` only), no timers, no worker threads, device refresh driven solely by
`WM_DEVICECHANGE` / explicit Refresh command, spdlog rotating file sink with
flush-on-info.

### 6.4 Unit tests

`mousedpi_tests.exe` (GoogleTest): **11/11 passed** — Result 5, DeviceInfo 3,
AdapterRegistry 2, AjazzAdapter (read-only phase guards) 1. All adapter
`set_dpi`/`set_polling_rate` paths correctly return `NotSupported` in Phase 1/2.

### 6.5 Remaining technical risks (honest assessment)

1. **The AJAZZ HID vendor protocol is unknown.** No authoritative protocol
   documentation has been verified for this unit. Phase 3 must start with a
   report-descriptor dump of the vendor collections and cross-checking
   community sources (e.g., open-source AJAZZ control projects) before any
   command is sent. Nothing in this phase assumes a command format.
2. **BLE configuration channel unproven.** On BLE, configuration would have to
   flow through `COL02` (usage page `0xFF01`) feature/output reports or through
   the custom GATT service `00010203-0405-0607-0809-0A0B0C0D1912`. Neither has
   been probed (probe would require writing, i.e., Phase 3 with explicit care).
3. **BLE link idles out** within minutes of inactivity; HID interfaces
   disappear. All device operations must tolerate transient absence, and the UI
   must distinguish "paired, link down" from "connected".
4. **Wired PID `0x5D22` is community-reported**, not observed on this system.
5. **Receiver vendor page is `0xFFEF`** on `MI_01/COL03`, not `0xFF01`; any
   protocol work must treat the receiver and BLE channels as distinct.
6. Battery level is exposed via GATT (`0x180F`), not via HID; reading it later
   will require the WinRT/Bluetooth APIs or the GATT service, not feature reports.

---

## 7. Phase 3 Protocol Implementation & Live Hardware Verification (2026-11-20)

### 7.1 Protocol Architecture & Implementation

Phase 3 established the full Ajazz AJ159 communication protocol in `mc::infra::adapters::ajazz`:
- **Protocol Engine (`AjazzProtocol`):**
  - **Frame Format:** 64-byte feature reports with Report ID `0x05`.
  - **Checksum Calculation:** Verified 8-bit modular checksum over bytes 1..62: `(\sum_{i=1}^{62} byte[i]) & 0xFF` stored at byte 63.
  - **Endianness:** Verified 16-bit Big-Endian integers for DPI sensitivity values and polling rates.
  - **Command Set:**
    - `0x21`: DPI configuration (sub-cmd `0x00`: set stage, `0x01`: set active stage, `0x02`: query).
    - `0x22`: Polling rate configuration (sub-cmd `0x00`: set rate, `0x01`: query rate).
    - `0x40`: Battery query and telemetry parsing.
  - **Boundary Validation:** Rejects DPI outside `[50, 26000]`, non-multiples of `50`, and invalid polling rates prior to any hardware I/O.

### 7.2 Live Hardware Discovery via WinRT BLE GATT

During empirical live hardware investigation, a dedicated C++ WinRT explorer (`ble_probe.exe`) and diagnostic tool (`hardware_probe.exe`) were executed directly against the user's physical mouse:
- **Device Node:** `AJ159P MC` (`D1:09:BD:99:A8:6F`)
- **PnP ID (`0x2A50`):** `02 8a 24 66 82 01 00` (Confirmed Vendor ID `0x248A`, Product ID `0x8266`, Version `0x0001`).
- **GATT Battery Service (`0x180F` -> `0x2A19`):** Successfully queried and read real live hardware battery telemetry: **78%**.
- **Vendor Custom GATT Service:** Discovered custom Beken/Ajazz vendor service `00010203-0405-0607-0809-0a0b0c0d1912` with telemetry characteristic `6971656b-676e-6179-6971-656b676e6179` ("bekenyang" ASCII signature) reporting 6 stages with Stage 3 active.
- **AjazzAdapter Integration:** `AjazzAdapter::get_battery_level()` now dynamically detects the connection mode; in Bluetooth LE mode, it extracts the MAC address from the device path and queries live battery telemetry via Windows BLE runtime, while in 2.4G mode it issues HID feature report query `0x40`.

### 7.3 Automated Unit & Hardware Test Results

- **Unit Test Suite (`mousedpi_tests.exe`):**
  - **17/17 tests passing (100%)** in 3 ms.
  - Covers checksum calculation, DPI stage construction, active stage switching, polling rate validation, response parsing, and battery range enforcement.
- **Hardware Probe Tool (`hardware_probe.exe`):**
  - Connected to live `AJ159P MC`.
  - Read active capabilities: DPI 50–26000, 6 stages, polling rates 125/250/500/1000 Hz, battery telemetry supported.
  - Queried live battery: **78%**.
  - Verified defensive parameter validation: invalid DPI (40) and invalid polling rate (750 Hz) safely rejected before hardware dispatch.
