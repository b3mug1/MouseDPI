#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include "infrastructure/hid/windows_hid_enumerator.hpp"
#include "infrastructure/adapters/ajazz/ajazz_adapter.hpp"
#include "infrastructure/logging/logger.hpp"

namespace {

std::string get_usage_page_name(uint16_t page) {
    if (page >= 0xFF00) return "Vendor-defined (0xFF00-0xFFFF)";
    switch (page) {
        case 0x0001: return "Generic Desktop (0x0001)";
        case 0x0002: return "Simulation Controls (0x0002)";
        case 0x0003: return "VR Controls (0x0003)";
        case 0x0004: return "Sport Controls (0x0004)";
        case 0x0005: return "Game Controls (0x0005)";
        case 0x0006: return "Generic Device Controls (0x0006)";
        case 0x0007: return "Keyboard / Keypad (0x0007)";
        case 0x0008: return "LEDs (0x0008)";
        case 0x0009: return "Button (0x0009)";
        case 0x000A: return "Ordinal (0x000A)";
        case 0x000B: return "Telephony (0x000B)";
        case 0x000C: return "Consumer (0x000C)";
        case 0x000D: return "Digitizers (0x000D)";
        case 0x000E: return "Haptics (0x000E)";
        case 0x0020: return "Sensors (0x0020)";
        case 0x0059: return "Lighting and Illumination (0x0059)";
        default:     return "Other standard page";
    }
}

std::string get_usage_name(uint16_t page, uint16_t usage) {
    if (page >= 0xFF00) {
        return std::format("Vendor Usage 0x{:04X}", usage);
    }
    if (page == 0x0001) {
        switch (usage) {
            case 0x0001: return "Pointer (0x0001)";
            case 0x0002: return "Mouse (0x0002)";
            case 0x0004: return "Joystick (0x0004)";
            case 0x0005: return "Gamepad (0x0005)";
            case 0x0006: return "Keyboard (0x0006)";
            case 0x0007: return "Keypad (0x0007)";
            case 0x0008: return "Multi-axis Controller (0x0008)";
            case 0x0080: return "System Control (0x0080)";
            default:     return std::format("Generic Desktop 0x{:04X}", usage);
        }
    }
    if (page == 0x000C) {
        switch (usage) {
            case 0x0001: return "Consumer Control (0x0001)";
            default:     return std::format("Consumer 0x{:04X}", usage);
        }
    }
    return std::format("Usage 0x{:04X}", usage);
}

} // namespace

int main(int argc, char* argv[]) {
    bool ajazz_only = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ajazz-only" || arg == "-a") {
            ajazz_only = true;
        }
    }

    // Initialize logger for inspector
    mc::infra::logging::Logger::init("logs/device_inspector.log", true);

    std::cout << "======================================================================\n";
    std::cout << "  MouseControl HID Hardware Inspector (v0.1.0 - Phase 1/2)\n";
    std::cout << "  Safe Read-Only Hardware Enumeration & Verification Tool\n";
    std::cout << "======================================================================\n\n";

    mc::infra::hid::WindowsHidEnumerator enumerator;
    auto result = enumerator.enumerate_devices();

    if (!result) {
        std::cerr << "[-] Error enumerating HID devices: " << result.error().to_string() << "\n";
        return 1;
    }

    const auto& devices = result.value();
    std::cout << "[+] Found " << devices.size() << " total HID device interface(s) present.\n\n";

    int index = 0;
    int ajazz_count = 0;

    for (const auto& dev : devices) {
        bool is_ajazz = (dev.vendor_id == mc::infra::adapters::ajazz::AjazzAdapter::VENDOR_ID_AJAZZ);
        if (is_ajazz) {
            ajazz_count++;
        }

        if (ajazz_only && !is_ajazz) {
            continue;
        }

        index++;
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "Interface #" << index;
        if (is_ajazz) {
            std::cout << " [*** AJAZZ HARDWARE DETECTED ***]";
        }
        std::cout << "\n";

        std::cout << "  VID / PID:      0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << dev.vendor_id
                  << " : 0x" << std::setw(4) << dev.product_id << std::dec << " (Version: " << dev.version_number << ")\n";
        std::cout << "  Manufacturer:   " << (dev.manufacturer.empty() ? "(empty/unreported)" : dev.manufacturer) << "\n";
        std::cout << "  Product Name:   " << (dev.product_name.empty() ? "(empty/unreported)" : dev.product_name) << "\n";
        std::cout << "  Serial Number:  " << (dev.serial_number.empty() ? "(none/unreported)" : dev.serial_number) << "\n";
        std::cout << "  Connection:     " << mc::core::connection_type_to_string(dev.connection_type) << "\n";
        std::cout << "  Usage Page:     0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << dev.usage_page
                  << " -> " << get_usage_page_name(dev.usage_page) << "\n";
        std::cout << "  Usage:          0x" << std::setw(4) << dev.usage << std::dec
                  << " -> " << get_usage_name(dev.usage_page, dev.usage) << "\n";
        std::cout << "  Report Lengths: Input: " << dev.input_report_byte_length 
                  << " B | Output: " << dev.output_report_byte_length 
                  << " B | Feature: " << dev.feature_report_byte_length << " B\n";
        std::cout << "  Device Path:    " << dev.device_path << "\n";
    }

    std::cout << "======================================================================\n";
    std::cout << "  DETECTION SUMMARY\n";
    std::cout << "======================================================================\n";
    std::cout << "Total HID interfaces:        " << devices.size() << "\n";
    std::cout << "Ajazz interfaces detected:   " << ajazz_count << "\n";

    if (ajazz_count > 0) {
        std::cout << "\n[SUCCESS] Ajazz AJ159 hardware was successfully detected on the host system!\n";
        std::cout << "Identified hardware characteristics:\n";
        for (const auto& dev : devices) {
            if (dev.vendor_id == mc::infra::adapters::ajazz::AjazzAdapter::VENDOR_ID_AJAZZ) {
                std::cout << "  - PID: 0x" << std::hex << std::uppercase << dev.product_id << std::dec
                          << " | Mode: " << mc::core::connection_type_to_string(dev.connection_type)
                          << " | UsagePage: 0x" << std::hex << dev.usage_page
                          << " | Usage: 0x" << dev.usage << std::dec;
                if (dev.usage_page >= 0xFF00) {
                    std::cout << " [Configuration & Telemetry Channel]";
                } else if (dev.is_mouse) {
                    std::cout << " [Mouse Pointing Stream]";
                }
                std::cout << "\n";
            }
        }
    } else {
        std::cout << "\n[NOTICE] No active Ajazz hardware was found in the current enumeration.\n";
    }

    std::cout << "\n[SAFETY] Read-only inspection completed. No configuration commands were transmitted.\n";
    std::cout << "======================================================================\n";

    return 0;
}
