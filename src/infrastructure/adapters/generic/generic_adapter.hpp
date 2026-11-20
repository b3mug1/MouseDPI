#pragma once

#include "core/domain/interfaces/i_mouse_adapter.hpp"

namespace mc::infra::adapters::generic {

class GenericAdapter : public core::domain::IMouseAdapter {
public:
    GenericAdapter() = default;
    ~GenericAdapter() override = default;

    [[nodiscard]] std::string_view get_adapter_name() const noexcept override {
        return "GenericAdapter";
    }

    [[nodiscard]] bool matches([[maybe_unused]] const core::domain::DeviceInfo& device) const override {
        return true; // Fallback matches any device
    }

    core::Result<bool> connect(const core::domain::DeviceInfo& device) override;

    [[nodiscard]] core::Result<core::domain::MouseCapabilities> get_capabilities() override;

    [[nodiscard]] core::Result<int> get_dpi() override;

    core::Result<void> set_dpi(int dpi) override;

    [[nodiscard]] core::Result<int> get_polling_rate() override;

    core::Result<void> set_polling_rate(int rate) override;

    void disconnect() override;

    [[nodiscard]] bool is_connected() const noexcept override {
        return is_connected_;
    }

private:
    bool is_connected_{false};
    core::domain::DeviceInfo connected_device_;
};

} // namespace mc::infra::adapters::generic
