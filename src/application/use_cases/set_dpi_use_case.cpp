#include "set_dpi_use_case.hpp"
#include "infrastructure/logging/logger.hpp"

namespace mc::app::use_cases {

core::Result<void> SetDPIUseCase::execute(core::domain::IMouseAdapter* adapter, int target_dpi) {
    if (!adapter || !adapter->is_connected()) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "No mouse adapter connected");
    }

    auto caps_res = adapter->get_capabilities();
    if (!caps_res) {
        return caps_res.error();
    }

    const auto& caps = caps_res.value();
    if (!caps.supports_dpi) {
        return core::Result<void>(core::ErrorCode::NotSupported, "Device does not support hardware DPI adjustment");
    }

    if (target_dpi < caps.min_dpi || target_dpi > caps.max_dpi) {
        return core::Result<void>(core::ErrorCode::InvalidParameter, 
            fmt::format("DPI {} is out of supported range [{}, {}]", target_dpi, caps.min_dpi, caps.max_dpi));
    }

    if (caps.dpi_step > 1 && ((target_dpi - caps.min_dpi) % caps.dpi_step != 0)) {
        return core::Result<void>(core::ErrorCode::InvalidParameter, 
            fmt::format("DPI {} is not a multiple of step size {}", target_dpi, caps.dpi_step));
    }

    auto set_res = adapter->set_dpi(target_dpi);
    if (!set_res) {
        MC_ERROR("Failed to set DPI: {}", set_res.error().to_string());
        return set_res;
    }

    MC_INFO("SetDPIUseCase: successfully set DPI to {}", target_dpi);
    return core::Result<void>::ok();
}

} // namespace mc::app::use_cases
