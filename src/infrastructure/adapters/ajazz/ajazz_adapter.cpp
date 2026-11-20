#include "ajazz_adapter.hpp"
#include "infrastructure/logging/logger.hpp"
#include <hidsdi.h>
#include <algorithm>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment(lib, "hid.lib")

namespace mc::infra::adapters::ajazz {

namespace {

uint64_t parse_ble_mac_from_path(const std::wstring& path) {
    auto amp_pos = path.find(L"&col");
    if (amp_pos == std::wstring::npos) {
        amp_pos = path.find(L"&COL");
    }
    if (amp_pos != std::wstring::npos && amp_pos >= 13) {
        auto underscore_pos = path.rfind(L'_', amp_pos);
        if (underscore_pos != std::wstring::npos && (amp_pos - underscore_pos - 1) == 12) {
            std::wstring hex_mac = path.substr(underscore_pos + 1, 12);
            try {
                return std::stoull(hex_mac, nullptr, 16);
            } catch (...) {}
        }
    }
    return 0;
}

core::Result<int> query_ble_battery(uint64_t mac) {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {}

    try {
        using namespace winrt::Windows::Devices::Bluetooth;
        using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
        using namespace winrt::Windows::Storage::Streams;

        auto ble_dev = BluetoothLEDevice::FromBluetoothAddressAsync(mac).get();
        if (!ble_dev) {
            return core::Result<int>(core::ErrorCode::DeviceNotFound, "BLE device not found at specified address");
        }

        winrt::guid battery_service_uuid{0x0000180F, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}};
        auto s_res = ble_dev.GetGattServicesForUuidAsync(battery_service_uuid).get();
        if (s_res.Status() != GattCommunicationStatus::Success || s_res.Services().Size() == 0) {
            return core::Result<int>(core::ErrorCode::DeviceNotFound, "Battery service not found on BLE device");
        }

        auto bat_service = s_res.Services().GetAt(0);
        auto c_res = bat_service.GetCharacteristicsForUuidAsync(GattCharacteristicUuids::BatteryLevel()).get();
        if (c_res.Status() != GattCommunicationStatus::Success || c_res.Characteristics().Size() == 0) {
            return core::Result<int>(core::ErrorCode::DeviceNotFound, "Battery characteristic not found");
        }

        auto bat_char = c_res.Characteristics().GetAt(0);
        auto val_res = bat_char.ReadValueAsync(BluetoothCacheMode::Uncached).get();
        if (val_res.Status() != GattCommunicationStatus::Success) {
            val_res = bat_char.ReadValueAsync(BluetoothCacheMode::Cached).get();
        }

        if (val_res.Status() == GattCommunicationStatus::Success && val_res.Value().Length() > 0) {
            auto reader = DataReader::FromBuffer(val_res.Value());
            int level = static_cast<int>(reader.ReadByte());
            return level;
        }
    } catch (const winrt::hresult_error& ex) {
        MC_DEBUG("WinRT BLE battery query exception: 0x{:X}", static_cast<uint32_t>(ex.code()));
    } catch (...) {}

    return core::Result<int>(core::ErrorCode::IoError, "Failed to read BLE battery characteristic");
}

} // namespace

AjazzAdapter::AjazzAdapter() = default;

AjazzAdapter::~AjazzAdapter() {
    disconnect();
}

bool AjazzAdapter::matches(const core::domain::DeviceInfo& device) const {
    if (device.vendor_id == VENDOR_ID_AJAZZ) {
        if (device.product_id == PID_AJ159_BLE || device.product_id == PID_AJ159_24G) {
            return true;
        }
    }

    std::string prod = device.product_name;
    std::transform(prod.begin(), prod.end(), prod.begin(), ::tolower);
    if (prod.find("aj159") != std::string::npos || prod.find("ajazz") != std::string::npos) {
        return true;
    }

    std::string path = device.device_path;
    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    if (path.find("aj159") != std::string::npos) {
        return true;
    }

    return false;
}

core::Result<void> AjazzAdapter::open_config_handle() {
    close_config_handle();

    // Determine target configuration path
    config_path_ = connected_device_.device_path_w;

    // If connected_device_ is the pointing stream (COL01), reroute to the configuration channel (COL02)
    auto col01_pos = config_path_.find(L"&col01#");
    if (col01_pos != std::wstring::npos) {
        config_path_.replace(col01_pos, 7, L"&col02#");
    } else {
        col01_pos = config_path_.find(L"&COL01#");
        if (col01_pos != std::wstring::npos) {
            config_path_.replace(col01_pos, 7, L"&COL02#");
        }
    }

    // For 2.4G mode: if MI_00, reroute to MI_01 or MI_02
    auto mi00_pos = config_path_.find(L"&mi_00#");
    if (mi00_pos != std::wstring::npos) {
        config_path_.replace(mi00_pos, 7, L"&mi_01&col03#");
    }

    config_handle_ = CreateFileW(
        config_path_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (config_handle_ == INVALID_HANDLE_VALUE) {
        // Fallback with GENERIC_WRITE
        config_handle_ = CreateFileW(
            config_path_.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
    }

    if (config_handle_ == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        MC_WARN("Could not open configuration handle directly (err: 0x{:X}). Device may be in standby/sleep.", err);
        return core::Result<void>(core::ErrorCode::DeviceOpenFailed, 
            "Device configuration interface is unreachable (device may be in standby/sleep mode)", err);
    }

    MC_INFO("Opened configuration handle for Ajazz AJ159 successfully");
    return core::Result<void>::ok();
}

void AjazzAdapter::close_config_handle() {
    if (config_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(config_handle_);
        config_handle_ = INVALID_HANDLE_VALUE;
    }
}

core::Result<bool> AjazzAdapter::connect(const core::domain::DeviceInfo& device) {
    if (!matches(device)) {
        return core::Result<bool>(core::ErrorCode::InvalidParameter, "Device is not an Ajazz AJ159 compatible device");
    }

    connected_device_ = device;
    is_connected_ = true;

    MC_INFO("AjazzAdapter connected to Ajazz AJ159 ({:04X}:{:04X}) mode: {}", 
        device.vendor_id, device.product_id, core::connection_type_to_string(device.connection_type));

    // Try to open configuration handle if device is currently active
    if (device.is_active) {
        auto handle_res = open_config_handle();
        if (handle_res) {
            // Optional read-first check: query current state
            auto dpi_q = AjazzProtocol::build_query_dpi();
            auto q_res = query_packet(dpi_q);
            if (q_res) {
                auto parsed_stage = AjazzProtocol::parse_active_dpi_stage_response(q_res.value());
                if (parsed_stage) {
                    current_stage_ = parsed_stage.value();
                    MC_INFO("Verified device response: active DPI stage = {}", current_stage_);
                }
            }
        }
    }

    return true;
}

core::Result<void> AjazzAdapter::send_packet(const AjazzPacket& packet) {
    if (!is_connected_) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "AjazzAdapter is not connected");
    }

    if (config_handle_ == INVALID_HANDLE_VALUE) {
        auto open_res = open_config_handle();
        if (!open_res) {
            return open_res.error();
        }
    }

    BOOL ok = HidD_SetFeature(
        config_handle_,
        const_cast<uint8_t*>(packet.data()),
        static_cast<ULONG>(packet.size())
    );

    if (!ok) {
        DWORD err = GetLastError();
        MC_ERROR("HidD_SetFeature failed with Windows error 0x{:X}", err);
        close_config_handle();
        return core::Result<void>(core::ErrorCode::IoError, "HidD_SetFeature failed", err);
    }

    MC_DEBUG("Transmitted 64-byte HID feature report: cmd=0x{:02X}, sub=0x{:02X}", packet[1], packet[2]);
    return core::Result<void>::ok();
}

core::Result<AjazzPacket> AjazzAdapter::query_packet(const AjazzPacket& query) {
    auto send_res = send_packet(query);
    if (!send_res) {
        return send_res.error();
    }

    AjazzPacket response{};
    response[0] = AJAZZ_REPORT_ID;

    BOOL ok = HidD_GetFeature(
        config_handle_,
        response.data(),
        static_cast<ULONG>(response.size())
    );

    if (!ok) {
        DWORD err = GetLastError();
        MC_DEBUG("HidD_GetFeature returned error 0x{:X}", err);
        return core::Result<AjazzPacket>(core::ErrorCode::IoError, "HidD_GetFeature failed", err);
    }

    if (!AjazzProtocol::verify_checksum(response)) {
        MC_WARN("Received feature report with checksum mismatch");
        return core::Result<AjazzPacket>(core::ErrorCode::InvalidData, "Response checksum mismatch");
    }

    return response;
}

core::Result<core::domain::MouseCapabilities> AjazzAdapter::get_capabilities() {
    if (!is_connected_) {
        return core::Result<core::domain::MouseCapabilities>(
            core::ErrorCode::DeviceNotFound, "Adapter is not connected to a device");
    }

    core::domain::MouseCapabilities caps{};
    caps.supports_dpi = true;
    caps.min_dpi = 50;
    caps.max_dpi = 26000;
    caps.dpi_step = 50;
    caps.max_dpi_stages = 6;

    caps.supports_polling_rate = true;
    caps.supported_polling_rates = {125, 250, 500, 1000};

    caps.supports_battery_level = (connected_device_.connection_type == core::ConnectionType::BluetoothLE ||
                                   connected_device_.connection_type == core::ConnectionType::Wireless24G);
    caps.supports_onboard_profiles = true;
    caps.supports_lod = true;

    return caps;
}

core::Result<int> AjazzAdapter::get_dpi() {
    if (!is_connected_) {
        return core::Result<int>(core::ErrorCode::DeviceNotFound, "Adapter is not connected");
    }
    return current_dpi_;
}

core::Result<void> AjazzAdapter::set_dpi(int dpi) {
    if (!is_connected_) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "Adapter is not connected");
    }

    // Strict boundary validation
    if (dpi < 50 || dpi > 26000) {
        return core::Result<void>(core::ErrorCode::InvalidParameter, 
            "Requested DPI out of range [50, 26000]");
    }
    if (dpi % 50 != 0) {
        return core::Result<void>(core::ErrorCode::InvalidParameter, 
            "Requested DPI must be an integer multiple of 50");
    }

    // 1. Build and send configuration for active stage
    auto packet_res = AjazzProtocol::build_set_dpi_stage(current_stage_, dpi, 0, 255, 0);
    if (!packet_res) {
        return packet_res.error();
    }

    auto send_res = send_packet(packet_res.value());
    if (!send_res) {
        return send_res.error();
    }

    // 2. Activate stage
    auto active_res = AjazzProtocol::build_set_active_stage(current_stage_);
    if (active_res) {
        [[maybe_unused]] auto activate_send_res = send_packet(active_res.value());
    }

    current_dpi_ = dpi;
    MC_INFO("Successfully configured sensor DPI to {} on Ajazz AJ159", dpi);
    return core::Result<void>::ok();
}

core::Result<int> AjazzAdapter::get_polling_rate() {
    if (!is_connected_) {
        return core::Result<int>(core::ErrorCode::DeviceNotFound, "Adapter is not connected");
    }
    return current_polling_rate_;
}

core::Result<void> AjazzAdapter::set_polling_rate(int rate) {
    if (!is_connected_) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "Adapter is not connected");
    }

    auto packet_res = AjazzProtocol::build_set_polling_rate(rate);
    if (!packet_res) {
        return packet_res.error();
    }

    auto send_res = send_packet(packet_res.value());
    if (!send_res) {
        return send_res.error();
    }

    current_polling_rate_ = rate;
    MC_INFO("Successfully configured polling rate to {} Hz on Ajazz AJ159", rate);
    return core::Result<void>::ok();
}

core::Result<int> AjazzAdapter::get_battery_level() {
    if (!is_connected_) {
        return core::Result<int>(core::ErrorCode::DeviceNotFound, "Adapter is not connected");
    }

    if (connected_device_.connection_type == core::ConnectionType::BluetoothLE) {
        uint64_t mac = parse_ble_mac_from_path(connected_device_.device_path_w);
        if (mac != 0) {
            auto ble_res = query_ble_battery(mac);
            if (ble_res) {
                return ble_res;
            }
        }
    } else {
        auto query = AjazzProtocol::build_query_battery();
        auto q_res = query_packet(query);
        if (q_res) {
            return AjazzProtocol::parse_battery_response(q_res.value());
        }
    }

    return core::Result<int>(core::ErrorCode::IoError, "Battery query failed or device is unreachable");
}

void AjazzAdapter::disconnect() {
    if (is_connected_) {
        close_config_handle();
        MC_INFO("AjazzAdapter disconnected from {:04X}:{:04X}", 
            connected_device_.vendor_id, connected_device_.product_id);
        is_connected_ = false;
    }
}

} // namespace mc::infra::adapters::ajazz
