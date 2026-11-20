#pragma once

#include <cstdint>
#include <string>
#include "core/types.hpp"

namespace mc::core::domain {

struct DeviceInfo {
    uint16_t vendor_id{0};
    uint16_t product_id{0};
    uint16_t version_number{0};

    std::string manufacturer;
    std::string product_name;
    std::string serial_number;

    std::string device_path;
    std::wstring device_path_w;

    uint16_t usage_page{0};
    uint16_t usage{0};

    uint16_t input_report_byte_length{0};
    uint16_t output_report_byte_length{0};
    uint16_t feature_report_byte_length{0};

    ConnectionType connection_type{ConnectionType::Unknown};

    bool is_mouse{false};
    bool is_vendor_defined{false};
    bool is_active{true};
    int interface_number{-1};

    [[nodiscard]] std::string format_vid_pid() const {
        return std::format("VID: {:04X}, PID: {:04X}", vendor_id, product_id);
    }

    [[nodiscard]] std::string format_usage() const {
        return std::format("UsagePage: 0x{:04X}, Usage: 0x{:04X}", usage_page, usage);
    }
};

} // namespace mc::core::domain
