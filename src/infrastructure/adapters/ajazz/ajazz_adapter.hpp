#pragma once

#include "core/domain/interfaces/i_mouse_adapter.hpp"
#include "ajazz_protocol.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace mc::infra::adapters::ajazz {

class AjazzAdapter : public core::domain::IMouseAdapter {
public:
    static constexpr uint16_t VENDOR_ID_AJAZZ = 0x248A;
    static constexpr uint16_t PID_AJ159_BLE   = 0x8266;
    static constexpr uint16_t PID_AJ159_24G   = 0x5D2E;

    AjazzAdapter();
    ~AjazzAdapter() override;

    [[nodiscard]] std::string_view get_adapter_name() const noexcept override {
        return "AjazzAdapter";
    }

    [[nodiscard]] bool matches(const core::domain::DeviceInfo& device) const override;

    core::Result<bool> connect(const core::domain::DeviceInfo& device) override;

    [[nodiscard]] core::Result<core::domain::MouseCapabilities> get_capabilities() override;

    [[nodiscard]] core::Result<int> get_dpi() override;

    core::Result<void> set_dpi(int dpi) override;

    [[nodiscard]] core::Result<int> get_polling_rate() override;

    core::Result<void> set_polling_rate(int rate) override;

    [[nodiscard]] core::Result<int> get_battery_level() override;

    void disconnect() override;

    [[nodiscard]] bool is_connected() const noexcept override {
        return is_connected_;
    }

private:
    core::Result<void> open_config_handle();
    void close_config_handle();
    core::Result<void> send_packet(const AjazzPacket& packet);
    core::Result<AjazzPacket> query_packet(const AjazzPacket& query);

    bool is_connected_{false};
    core::domain::DeviceInfo connected_device_;
    HANDLE config_handle_{INVALID_HANDLE_VALUE};
    std::wstring config_path_;

    int current_dpi_{800};
    int current_stage_{1};
    int current_polling_rate_{1000};
};

} // namespace mc::infra::adapters::ajazz
