# MouseControl — Universal Gaming Mouse Management Software

## 1. Project Overview

You are a senior C++ software engineer and Windows systems developer.

Your task is to design and develop **MouseControl**, a lightweight native Windows desktop application for managing gaming mice.

The application must provide a unified interface for controlling hardware-level mouse settings, including:

- DPI sensitivity.
- DPI stages.
- Polling rate.
- Device profiles.
- Device information.
- Hardware capabilities.
- Device connection monitoring.

The first target device is the **Ajazz AJ159**, but the architecture must support additional manufacturers and mouse models in the future.

This must be a real working application. Do not create fake controls, simulated hardware functionality, or UI elements that are not connected to actual application logic.

## 2. Core Technical Requirements

### Programming Language

- C++20 (C++23 where beneficial).
- Native Windows 11 support.
- x64 architecture.
- RAII and modern C++ practices.
- Smart pointers instead of unnecessary manual memory management.
- Strong type safety.
- Clear separation of responsibilities.

### Technology Stack

| Component | Technology |
|---|---|
| Language | C++20 |
| Operating System | Windows 11 |
| Window Management | Win32 API |
| Graphics | Direct2D / DirectWrite |
| Rendering | Direct3D 11 where required |
| Device Communication | HIDAPI / Windows HID API |
| Database | SQLite |
| Build System | CMake |
| Compiler | MSVC |
| Dependency Management | vcpkg |
| Testing | GoogleTest |
| Logging | spdlog |
| Configuration | JSON |

Avoid Electron, Chromium Embedded Framework, Python runtime, and other heavyweight dependencies.

Do not introduce Qt unless there is a strong technical justification.

## 3. Primary Engineering Objective: Low Resource Consumption

One of the most important requirements is minimizing RAM and CPU usage.

MouseControl is intended to run continuously in the Windows system tray.

Target resource budgets:

- Background RAM: 5–15 MB where technically achievable.
- Open application RAM: 20–40 MB where technically achievable.
- Idle CPU usage: approximately 0%.
- No unnecessary background polling.
- No permanent rendering loop when the interface is hidden.
- No unnecessary worker threads.
- No memory leaks.
- No unnecessary dynamic allocations.

These are optimization targets, not guaranteed values.

### Performance Requirements

1. Use an event-driven architecture.
2. Use Windows messages instead of continuously polling device states.
3. Use `WM_DEVICECHANGE` to detect device connection changes.
4. Use `RegisterDeviceNotification` where appropriate.
5. Avoid unnecessary timers.
6. Render the interface only when needed.
7. Release graphics resources when appropriate.
8. Initialize expensive components lazily.
9. Avoid unnecessary object creation and copying.
10. Measure actual RAM consumption in Release builds.

Do not sacrifice reliability or device communication correctness just to achieve a lower memory footprint.

## 4. Application Architecture

Use a modular architecture inspired by Clean Architecture.

Separate the application into the following layers:

### Core / Domain

Contains:

- MouseDevice.
- MouseCapabilities.
- MouseProfile.
- DPIStage.
- DeviceConnection.
- DeviceSettings.
- Adapter interfaces.
- Domain exceptions.

This layer must not depend on Win32, HIDAPI, SQLite, Direct2D, or UI libraries.

### Application

Contains application use cases:

- DiscoverDevicesUseCase.
- GetDeviceInformationUseCase.
- GetDeviceCapabilitiesUseCase.
- SetDPIUseCase.
- SetPollingRateUseCase.
- CreateProfileUseCase.
- UpdateProfileUseCase.
- SwitchProfileUseCase.
- SaveSettingsUseCase.
- RestoreSettingsUseCase.

### Infrastructure

Contains:

- HID communication.
- USB device discovery.
- Windows HID integration.
- Manufacturer-specific protocols.
- SQLite repositories.
- JSON configuration.
- Logging.
- Windows system integration.

### Presentation

Contains:

- Main window.
- Dashboard.
- DPI configuration.
- Polling rate configuration.
- Device information.
- Profile management.
- Settings.
- System tray integration.

## 5. Universal Device Adapter System

This is a critical architectural requirement.

Do not implement manufacturer-specific communication directly inside the UI or application core.

Create an extensible adapter system.

Define an abstract interface similar to:

```cpp
class IMouseAdapter {
public:
    virtual ~IMouseAdapter() = default;

    virtual bool connect(const DeviceInfo& device) = 0;

    virtual Result<MouseCapabilities> get_capabilities() = 0;

    virtual Result<int> get_dpi() = 0;

    virtual Result<void> set_dpi(int dpi) = 0;

    virtual Result<int> get_polling_rate() = 0;

    virtual Result<void> set_polling_rate(int rate) = 0;

    virtual void disconnect() = 0;
};
```

Adapt this interface to the project's actual Result/Error type.

Implement an Adapter Registry that selects the correct adapter based on:

- Vendor ID.
- Product ID.
- HID Usage Page.
- HID Usage.
- Interface information.
- Supported hardware revision.
- Connection mode, where identifiable.

Initial adapter structure:

- AjazzAdapter.
- LogitechAdapter (future implementation).
- RazerAdapter (future implementation).
- GenericAdapter.

The GenericAdapter must support device discovery and diagnostics only.

Never send undocumented vendor-specific commands to an unknown device.

A device may be detected without supporting hardware-level DPI or polling-rate modification.

Represent unsupported or unknown capabilities explicitly.

## 6. Initial Device: Ajazz AJ159

The first fully supported device must be Ajazz AJ159.

Important: The AJ159 product family may include different hardware revisions and configurations.

Do not assume all AJ159 variants have identical capabilities.

### Research Requirements

Before implementing hardware communication:

1. Identify the actual USB Vendor ID and Product ID.
2. Inspect all HID interfaces.
3. Identify the correct HID Usage Page and Usage.
4. Determine which interface accepts vendor-specific Feature Reports.
5. Research the AJAZZ HID protocol.
6. Verify command formats and checksums.
7. Verify the actual supported DPI range.
8. Verify supported polling rates for each connection mode.
9. Test whether settings persist after disconnecting the device.
10. Document all findings.

Investigate the publicly available AJAZZ protocol documentation and the open-source AJAZZ Control Center project as research references.

Do not blindly copy protocol implementations without understanding their behavior and verifying compatibility.

### HID Protocol

Investigate the reported AJAZZ Feature Report format:

- Report size: 64 bytes.
- Report ID: 0x05.
- Report commands reportedly include DPI management (0x21) and polling-rate management (0x22).
- Other commands may include LOD and button configuration.

Treat these values as research leads, not guaranteed specifications for every AJ159 revision.

Implement protocol commands only after verifying their exact payload structure, checksum calculation, response format, and compatibility.

Never guess undocumented payload bytes.

### Hardware Safety

- Validate DPI values before sending commands.
- Validate polling-rate values against device capabilities.
- Never send arbitrary commands to unknown HID devices.
- Never overwrite onboard profiles without explicit user action.
- Handle communication failures gracefully.
- Provide clear error messages.
- Do not claim that a setting was applied without confirmation.
- Use a safe disconnect and reconnect mechanism.

## 7. Device Discovery

Implement device discovery using HIDAPI and/or Windows HID APIs.

Collect:

- Vendor ID.
- Product ID.
- Manufacturer.
- Product name.
- Serial number when available.
- Device path.
- HID Usage Page.
- HID Usage.
- Interface information.
- Connection status.

Use Windows device notifications rather than constant polling wherever possible.

The application must detect:

- Device connected.
- Device disconnected.
- Device reconnected.
- Multiple compatible devices.
- Unsupported devices.

Do not assume that every HID interface belonging to a mouse is its configuration interface.

Avoid opening interfaces used for normal mouse input when a separate configuration interface is available.

## 8. DPI Management

Implement a dedicated DPI management module.

Required functionality:

- Display current DPI.
- Change hardware DPI.
- Support multiple DPI stages.
- Enable or disable stages where supported.
- Add and remove stages where supported.
- Validate minimum and maximum DPI.
- Validate DPI step size.
- Select active DPI stage.
- Display available DPI values.
- Save settings into device memory when supported.

The UI must display only supported settings.

Do not confuse Windows pointer speed with actual hardware DPI.

Do not use Windows mouse sensitivity APIs as a substitute for changing sensor DPI.

## 9. Polling Rate Management

Implement hardware polling-rate configuration.

Potential values:

- 125 Hz.
- 250 Hz.
- 500 Hz.
- 1000 Hz.
- 2000 Hz.
- 4000 Hz.
- 8000 Hz.

These are possible values, not universal capabilities.

Determine available rates dynamically for each device and connection mode.

Required functionality:

- Display current polling rate when readable.
- Change polling rate.
- Validate supported values.
- Handle unsupported values.
- Handle failed communication.
- Verify applied settings where possible.

Do not claim to change the polling rate by merely changing a Windows timer or cursor update frequency.

## 10. Profile Management

Implement persistent profiles using SQLite.

Users must be able to:

- Create profiles.
- Rename profiles.
- Delete profiles.
- Duplicate profiles.
- Export profiles.
- Import profiles.
- Associate profiles with specific devices.
- Switch profiles manually.
- Restore previous configurations.

Example profile:

```json
{
    "name": "CS2",
    "dpi_stages": [400, 800],
    "active_dpi": 400,
    "polling_rate": 1000
}
```

Handle incompatible profiles safely.

If a profile requests an unsupported DPI or polling rate, warn the user and provide an appropriate alternative.

Do not silently apply incompatible settings.

## 11. User Interface

Create a modern, minimalist, dark-themed Windows desktop interface.

Design principles:

- Clean layout.
- Minimal visual clutter.
- Responsive interactions.
- Clear typography.
- Consistent spacing.
- Subtle animations.
- No unnecessary visual effects.
- No excessive GPU usage.

Main navigation:

1. Dashboard.
2. DPI.
3. Polling Rate.
4. Profiles.
5. Device Information.
6. Settings.

### Dashboard

Display:

- Connected mouse.
- Manufacturer.
- Model.
- Connection mode.
- Current DPI.
- Current polling rate.
- Battery percentage, if available.
- Connection status.
- Active profile.

### DPI Page

Display:

- DPI slider.
- Numeric DPI input.
- DPI stages.
- Active stage.
- Supported DPI range.
- Apply button.
- Reset button.

### Polling Rate Page

Display available polling rates and the currently selected value.

### Profiles Page

Display saved profiles with options to create, edit, duplicate, delete, import, and export.

### Device Information Page

Display:

- Vendor ID.
- Product ID.
- HID interfaces.
- Firmware information, if available.
- Supported capabilities.
- Connection type.

Do not invent or display unavailable hardware information.

## 12. Windows Integration

Implement:

- System tray icon.
- Tray context menu.
- Start with Windows option.
- Minimize to tray.
- Close-to-tray behavior.
- Global hotkeys.
- Device connection notifications.
- Single-instance application behavior.

Closing the main window must not necessarily terminate the application.

Provide a proper Exit option in the tray menu.

Do not request administrator privileges unless a specific operation requires them.

## 13. Project Structure

Use the following structure as a starting point:

```text
MouseControl/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── README.md
│
├── src/
│   ├── main.cpp
│   │
│   ├── core/
│   ├── domain/
│   │   ├── entities/
│   │   ├── interfaces/
│   │   └── exceptions/
│   │
│   ├── application/
│   │   ├── use_cases/
│   │   └── services/
│   │
│   ├── infrastructure/
│   │   ├── hid/
│   │   ├── windows/
│   │   ├── database/
│   │   ├── logging/
│   │   └── adapters/
│   │       ├── ajazz/
│   │       ├── logitech/
│   │       ├── razer/
│   │       └── generic/
│   │
│   ├── presentation/
│   │   ├── windows/
│   │   ├── widgets/
│   │   ├── controllers/
│   │   └── styles/
│   │
│   └── system_tray/
│
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── hardware/
│   └── mocks/
│
├── docs/
│   ├── architecture.md
│   ├── hid_protocol.md
│   ├── supported_devices.md
│   └── performance.md
│
└── scripts/
    └── device_inspector/
```

You may improve this structure when technically justified.

## 14. Testing Requirements

Implement several testing levels.

### Unit Tests

Test:

- DPI validation.
- Polling-rate validation.
- Profile serialization.
- Adapter registry.
- HID report construction.
- Checksum calculation.
- Error handling.

### Integration Tests

Test:

- HID device discovery.
- SQLite persistence.
- Profile restoration.
- Device reconnection.
- Adapter selection.

### Hardware Tests

With a physical Ajazz AJ159:

- Device identification.
- DPI modification.
- Polling-rate modification.
- Read-back verification.
- Persistence verification.
- Wired mode.
- 2.4 GHz mode.
- Bluetooth mode, if supported by the exact revision.

Hardware tests must never run automatically on an arbitrary connected device.

Use mocked HID devices for CI testing.

## 15. Development Roadmap

Develop the project incrementally.

### Phase 1 — Foundation

- Set up CMake.
- Configure MSVC.
- Configure vcpkg.
- Establish the directory structure.
- Implement logging.
- Implement error handling.
- Create the initial Win32 application.

### Phase 2 — Device Discovery

- Integrate HIDAPI.
- Enumerate HID devices.
- Display VID/PID.
- Identify Ajazz AJ159.
- Implement connection monitoring.
- Build a standalone device inspector.

### Phase 3 — AJAZZ Protocol

- Research the actual AJ159 hardware revision.
- Implement report construction.
- Implement checksum calculation.
- Implement safe HID communication.
- Verify device responses.
- Implement DPI modification.
- Implement polling-rate modification.

Do not move forward with unsupported assumptions about the hardware protocol.

### Phase 4 — Core Functionality

- Implement the adapter registry.
- Implement capabilities.
- Implement device management.
- Implement DPI management.
- Implement polling-rate management.
- Implement device settings.

### Phase 5 — Database and Profiles

- Integrate SQLite.
- Implement repositories.
- Implement profile CRUD.
- Implement import/export.
- Implement settings persistence.

### Phase 6 — User Interface

- Build the main window.
- Implement Dashboard.
- Implement DPI page.
- Implement Polling Rate page.
- Implement Profiles page.
- Implement Device Information page.
- Add a dark theme.

### Phase 7 — Windows Integration

- Implement system tray.
- Implement device notifications.
- Implement global hotkeys.
- Implement single-instance behavior.
- Implement startup configuration.

### Phase 8 — Optimization

- Profile memory consumption.
- Profile CPU consumption.
- Remove unnecessary allocations.
- Optimize startup time.
- Test long-running stability.
- Test device reconnection.

### Phase 9 — Release

- Configure Release build.
- Optimize binary size.
- Package dependencies.
- Create installer.
- Write documentation.
- Publish GitHub repository.

## 16. Code Quality Standards

Follow these rules:

- Use modern C++20.
- Follow RAII.
- Prefer `std::unique_ptr` and `std::shared_ptr` only where appropriate.
- Avoid raw owning pointers.
- Avoid global mutable state.
- Use `const` wherever possible.
- Use `enum class` for enumerations.
- Use `std::expected` where available or a compatible Result type.
- Avoid unnecessary inheritance.
- Follow SOLID principles where useful.
- Keep functions small and focused.
- Separate hardware communication from UI logic.
- Document non-obvious protocol behavior.
- Use consistent naming conventions.
- Enable compiler warnings.
- Avoid undefined behavior.
- Never suppress errors silently.

## 17. Important Development Rules

1. Do not generate the entire application in one enormous source file.
2. Do not implement fake device functionality.
3. Do not invent undocumented HID commands.
4. Do not assume all mouse manufacturers use the same protocol.
5. Do not assume every AJ159 revision supports the same settings.
6. Do not sacrifice stability for premature optimization.
7. Do not add heavyweight dependencies without justification.
8. Write tests for protocol construction and parsing.
9. Keep the application usable when no supported mouse is connected.
10. Document limitations honestly.

When hardware access is unavailable, create mock implementations and clearly distinguish simulated tests from real hardware tests.

## 18. Expected Deliverables

The completed project must include:

- Fully functional C++ source code.
- CMake build configuration.
- Dependency configuration.
- Windows 11 executable.
- Working system tray.
- Device discovery.
- Ajazz AJ159 adapter, validated against real hardware.
- Hardware-level DPI control where supported.
- Hardware-level polling-rate control where supported.
- Profile management.
- SQLite persistence.
- Automated tests.
- Architecture documentation.
- HID protocol documentation.
- Supported device documentation.
- Release build instructions.

## 19. First Task: Do Not Start Coding Everything Immediately

Before implementing the full application:

1. Analyze the requirements.
2. Propose the final architecture.
3. Identify technical risks.
4. Research the Ajazz AJ159 HID protocol.
5. Create a hardware compatibility investigation plan.
6. Establish the CMake project.
7. Implement a small HID device inspector.
8. Verify that the actual mouse can be detected.

After the discovery stage, present the findings and continue with the first hardware-control implementation.

The most important objective is to create a lightweight, reliable, native Windows application that genuinely controls supported mouse hardware rather than merely simulating settings in its interface.

**Project name: MouseControl**

**Primary platform: Windows 11 x64**

**Primary language: C++20**

**First supported device: Ajazz AJ159**

**Long-term goal: Universal gaming mouse management software.**