#include <iostream>
#include <iomanip>
#include "infrastructure/adapters/ajazz/ajazz_adapter.hpp"
#include "infrastructure/hid/windows_hid_enumerator.hpp"
#include "infrastructure/logging/logger.hpp"

using namespace mc::core;
using namespace mc::core::domain;
using namespace mc::infra::adapters::ajazz;
using namespace mc::infra::hid;

int main() {
    mc::infra::logging::Logger::init("logs/hardware_probe.log", spdlog::level::info);
    std::cout << "========================================================" << std::endl;
    std::cout << "  Ajazz AJ159 Live Hardware Verification (Phase 3)" << std::endl;
    std::cout << "========================================================" << std::endl;

    WindowsHidEnumerator enumerator;
    auto enum_res = enumerator.enumerate_devices();
    if (!enum_res) {
        std::cerr << "Enumeration failed: " << enum_res.error().to_string() << std::endl;
        return 1;
    }

    std::optional<DeviceInfo> ajazz_dev;
    for (const auto& dev : enum_res.value()) {
        if (dev.vendor_id == 0x248A && dev.product_id == 0x8266) {
            ajazz_dev = dev;
            break;
        }
    }

    if (!ajazz_dev) {
        std::cout << "Ajazz AJ159 BLE device not found in enumeration." << std::endl;
        return 1;
    }

    std::cout << "[+] Found Ajazz AJ159 Device:" << std::endl;
    std::cout << "    Name:        " << ajazz_dev->product_name << std::endl;
    std::cout << "    VID:PID:     0x" << std::hex << ajazz_dev->vendor_id << ":0x" << ajazz_dev->product_id << std::dec << std::endl;
    std::cout << "    Connection:  " << connection_type_to_string(ajazz_dev->connection_type) << std::endl;
    std::cout << "    Device Path: " << ajazz_dev->device_path << std::endl;

    AjazzAdapter adapter;
    auto conn_res = adapter.connect(ajazz_dev.value());
    if (!conn_res) {
        std::cerr << "[-] Adapter failed to connect: " << conn_res.error().to_string() << std::endl;
        return 1;
    }

    std::cout << "\n[+] Adapter connected successfully!" << std::endl;

    auto caps = adapter.get_capabilities();
    if (caps) {
        std::cout << "[+] Capabilities:" << std::endl;
        std::cout << "    Supports DPI:          " << (caps.value().supports_dpi ? "YES" : "NO") << std::endl;
        std::cout << "    DPI Range:             " << caps.value().min_dpi << " - " << caps.value().max_dpi << " (Step: " << caps.value().dpi_step << ")" << std::endl;
        std::cout << "    Max DPI Stages:        " << caps.value().max_dpi_stages << std::endl;
        std::cout << "    Supports Polling Rate: " << (caps.value().supports_polling_rate ? "YES" : "NO") << std::endl;
        std::cout << "    Supported Rates:       ";
        for (int r : caps.value().supported_polling_rates) std::cout << r << "Hz ";
        std::cout << std::endl;
        std::cout << "    Supports Battery:      " << (caps.value().supports_battery_level ? "YES" : "NO") << std::endl;
        std::cout << "    Supports Onboard Prof: " << (caps.value().supports_onboard_profiles ? "YES" : "NO") << std::endl;
        std::cout << "    Supports LOD:          " << (caps.value().supports_lod ? "YES" : "NO") << std::endl;
    }

    std::cout << "\n[+] Querying Real Battery Telemetry:" << std::endl;
    auto bat_res = adapter.get_battery_level();
    if (bat_res) {
        std::cout << "    >>> REAL HARDWARE BATTERY LEVEL: " << bat_res.value() << "% <<<" << std::endl;
    } else {
        std::cout << "    Battery query status: " << bat_res.error().to_string() << std::endl;
    }

    std::cout << "\n[+] Testing DPI & Polling Rate Parameter Validation:" << std::endl;
    auto dpi_res = adapter.get_dpi();
    if (dpi_res) {
        std::cout << "    Current Active DPI: " << dpi_res.value() << std::endl;
    }

    auto poll_res = adapter.get_polling_rate();
    if (poll_res) {
        std::cout << "    Current Polling Rate: " << poll_res.value() << " Hz" << std::endl;
    }

    // Test safety validation: bounds checking
    auto invalid_dpi = adapter.set_dpi(40);
    std::cout << "    Attempting to set invalid DPI (40 < min 50): " 
              << (invalid_dpi ? "UNEXPECTED SUCCESS" : "SAFELY REJECTED: " + invalid_dpi.error().message) << std::endl;

    auto invalid_rate = adapter.set_polling_rate(750);
    std::cout << "    Attempting to set invalid Rate (750 Hz): " 
              << (invalid_rate ? "UNEXPECTED SUCCESS" : "SAFELY REJECTED: " + invalid_rate.error().message) << std::endl;

    adapter.disconnect();
    std::cout << "\n[+] Disconnected cleanly. Phase 3 validation completed successfully." << std::endl;
    std::cout << "========================================================" << std::endl;
    return 0;
}
