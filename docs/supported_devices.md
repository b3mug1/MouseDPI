# Supported Devices & Hardware Compatibility

## 1. Primary Device: Ajazz AJ159 Series

MouseControl has verified driver support for the Ajazz AJ159 high-performance gaming mouse series (PixArt PAW3395 optical sensor).

### 1.1 Hardware Specifications
- **Sensor:** PixArt PAW3395 (up to 26,000 DPI, 650 IPS, 50G acceleration)
- **DPI Range:** 50 – 26,000 DPI (in 50 DPI steps)
- **Supported Polling Rates:** 125 Hz (8ms), 250 Hz (4ms), 500 Hz (2ms), 1000 Hz (1ms)
- **Hardware Profile Storage:** Up to 6 on-board DPI stages with RGB stage indicator

### 1.2 Identified Hardware Revisions & Identifiers

| Connection Mode | Vendor ID (VID) | Product ID (PID) | Configuration Usage Page | Feature Support |
| :--- | :--- | :--- | :--- | :--- |
| **USB Wired** | `0x248A` | `0x5D2D` | `0xFF00` / `0xFFA0` | Full DPI, Polling Rate, Battery, Firmware Query |
| **2.4 GHz Wireless** | `0x248A` | `0x5D2E` | `0xFF00` / `0xFFA0` | Full DPI, Polling Rate, Battery, Wireless Status |
| **Bluetooth LE** | `0x248A` | `0x8266` | Standard HID Pointer | Read-only connection status, Generic diagnostics |

> [!NOTE]
> When connected via Bluetooth LE, standard Windows security restrictions prevent raw vendor feature reports without pairing an exclusive vendor service profile. For full on-board sensor configuration (DPI/polling rate modifications), the 2.4 GHz wireless receiver or USB wired connection is recommended.

---

## 2. Generic Mice & Diagnostics Mode

All other USB and wireless HID mice connected to Windows 11 are recognized by the built-in `GenericAdapter`:
- Provides read-only interface inspection (VID, PID, Usage Page, Usage, Serial Number, Manufacturer, Device Path).
- Prevents sending unsupported vendor commands to prevent hardware faults.
- Allows applying Windows-level cursor sensitivity and local profile switching.
