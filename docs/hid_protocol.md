# Ajazz AJ159 HID Protocol Specification

**Document Version:** 1.0.0  
**Target Hardware:** Ajazz AJ-series Gaming Mice (specifically AJ159 / AJ159P / AJ159 Apex)  
**USB Vendor ID:** `0x248A`  
**Product IDs:** `0x8266` (Bluetooth LE Mode), `0x5D2E` (2.4 GHz Wireless Receiver / Dongle)  
**Target HID Interface:** Vendor-defined interface (`Usage Page: 0xFF00` - `0xFF01`, `Usage: 0x0001`)

---

## 1. Frame Envelope (64-Byte Feature Report)

All communication to and from the mouse configuration endpoint occurs via **64-byte HID Feature Reports** with Report ID `0x05`.

| Byte Offset | Field | Description |
|---|---|---|
| `0` | **Report ID** | Always `0x05` |
| `1` | **Command ID** | Opcode defining the subsystem operation (`0x21` for DPI, `0x22` for Polling Rate, `0x40` for Battery) |
| `2` | **Sub-Command** | Operation type (Set Stage, Set Active, Get, etc.) |
| `3` | **Payload Length ($N$)** | Number of valid payload bytes following ($0 \le N \le 58$) |
| `4 \dots 3+N` | **Payload Data** | Command-specific parameter bytes |
| `4+N \dots 62` | **Zero Padding** | Filled with `0x00` |
| `63` | **Checksum** | 8-bit arithmetic sum: `(\sum_{i=1}^{62} \text{byte}[i]) \pmod{256}` |

### Checksum Calculation
The checksum occupies byte index `63` (the 64th byte). It is calculated as the low 8 bits of the sum of all bytes from index `1` through `62` (inclusive):
$$\text{Checksum} = \left(\sum_{k=1}^{62} \text{frame}[k]\right) \&\ 0\text{xFF}$$
Byte `0` (Report ID `0x05`) is excluded from the checksum sum.

---

## 2. Command Set

### 2.1 Command `0x21` — DPI Configuration

#### Sub-Command `0x00`: Configure DPI Stage Parameters
Configures the sensor resolution and indicator LED color for a specific stage index.

- **Byte 1:** `0x21` (Command DPI)
- **Byte 2:** `0x00` (Sub-command: Set Stage)
- **Byte 3:** `0x06` (Payload length: 6 bytes)
- **Byte 4:** Stage Index ($1 \dots 6$, 1-based)
- **Bytes 5–6:** DPI Value as a 16-bit Big-Endian unsigned integer ($\text{DPI} = \text{Byte}[5] \times 256 + \text{Byte}[6]$).
  - Minimum: `50` (`0x0032`)
  - Maximum: `26000` (`0x6590`)
  - Step size: `50`
- **Bytes 7–9:** RGB indicator LED color:
  - Byte 7: Red component (`0x00`–`0xFF`)
  - Byte 8: Green component (`0x00`–`0xFF`)
  - Byte 9: Blue component (`0x00`–`0xFF`)
- **Example Frame (Stage 1 = 800 DPI, Red):**
  ```
  05 21 00 06 01 03 20 FF 00 00 00 ... 00 [CS]
  ```
  *(0x0320 = 800)*

#### Sub-Command `0x01`: Set Active DPI Stage
Switches the mouse to use a specific existing stage index.

- **Byte 1:** `0x21` (Command DPI)
- **Byte 2:** `0x01` (Sub-command: Set Active Stage)
- **Byte 3:** `0x01` (Payload length: 1 byte)
- **Byte 4:** Active Stage Index ($1 \dots 6$, 1-based)
- **Example Frame (Switch to Stage 2):**
  ```
  05 21 01 01 02 00 00 00 ... 00 [CS]
  ```

#### Sub-Command `0x02`: Query DPI Configuration
Requests the mouse to return its active DPI stage and configured stages.

- **Byte 1:** `0x21` (Command DPI)
- **Byte 2:** `0x02` (Sub-command: Get Stages)
- **Byte 3:** `0x00` (Payload length: 0)
- **Example Frame:**
  ```
  05 21 02 00 00 00 00 ... 00 [CS]
  ```

---

### 2.2 Command `0x22` — Polling Rate Configuration

#### Sub-Command `0x00`: Set Polling Rate
Configures the USB / Wireless report rate in Hertz.

- **Byte 1:** `0x22` (Command Polling Rate)
- **Byte 2:** `0x00` (Sub-command: Set Polling Rate)
- **Byte 3:** `0x02` (Payload length: 2 bytes)
- **Bytes 4–5:** Polling rate in Hz as a 16-bit Big-Endian unsigned integer:
  | Polling Rate | Hex Bytes (Byte 4, Byte 5) | Decimal |
  |---|---|---|
  | **125 Hz** | `00 7D` | 125 |
  | **250 Hz** | `00 FA` | 250 |
  | **500 Hz** | `01 F4` | 500 |
  | **1000 Hz** | `03 E8` | 1000 |
  | **2000 Hz** *(if supported)* | `07 D0` | 2000 |
  | **4000 Hz** *(if supported)* | `0F A0` | 4000 |
  | **8000 Hz** *(if supported)* | `1F 40` | 8000 |

- **Example Frame (Set Polling Rate to 1000 Hz):**
  ```
  05 22 00 02 03 E8 00 00 00 ... 00 [CS]
  ```
  *(Checksum = (0x22 + 0x00 + 0x02 + 0x03 + 0xE8) & 0xFF = 0x0F)*

#### Sub-Command `0x01`: Query Polling Rate
Requests the current active polling rate.

- **Byte 1:** `0x22` (Command Polling Rate)
- **Byte 2:** `0x01` (Sub-command: Get Polling Rate)
- **Byte 3:** `0x00` (Payload length: 0)
- **Example Frame:**
  ```
  05 22 01 00 00 00 ... 00 [CS]
  ```

---

### 2.3 Command `0x40` — Battery Status Query

- **Byte 1:** `0x40` (Battery Status)
- **Byte 2:** `0x00` (Query)
- **Byte 3:** `0x00`
- **Response Structure:** Byte 4 contains the battery level integer from `0` to `100` (`0x00`–`0x64`), or `0xFF` if unavailable/offline.

---

## 3. Communication Channel Routing

In Windows 11, HID devices expose multiple collections. The application **must strictly send configuration feature reports only to vendor-defined collections**:
- **Bluetooth LE Mode (`PID: 0x8266`):** Collection 02 (`Usage Page 0xFF01, Usage 0x0001`).
- **2.4 GHz USB Dongle Mode (`PID: 0x5D2E`):** Interface 1 (`MI_01:COL03`) or Interface 2 (`MI_02`).
- **Standard Mouse Pointing Interfaces (`Usage Page 0x0001, Usage 0x0002`):** Must **never** be targeted with configuration feature reports to avoid OS mouse driver contention or input interruption.
