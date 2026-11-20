#pragma once

#include <string_view>
#include <memory>
#include "core/types.hpp"
#include "core/domain/entities/device_info.hpp"
#include "core/domain/entities/mouse_capabilities.hpp"

namespace mc::core::domain {

class IMouseAdapter {
public:
    virtual ~IMouseAdapter() = default;

    [[nodiscard]] virtual std::string_view get_adapter_name() const noexcept = 0;

    [[nodiscard]] virtual bool matches(const DeviceInfo& device) const = 0;

    virtual Result<bool> connect(const DeviceInfo& device) = 0;

    [[nodiscard]] virtual Result<MouseCapabilities> get_capabilities() = 0;

    [[nodiscard]] virtual Result<int> get_dpi() = 0;

    virtual Result<void> set_dpi(int dpi) = 0;

    [[nodiscard]] virtual Result<int> get_polling_rate() = 0;

    virtual Result<void> set_polling_rate(int rate) = 0;

    [[nodiscard]] virtual Result<int> get_battery_level() {
        return Result<int>(ErrorCode::NotSupported, "Battery query not supported by this adapter");
    }

    virtual void disconnect() = 0;

    [[nodiscard]] virtual bool is_connected() const noexcept = 0;
};

} // namespace mc::core::domain
