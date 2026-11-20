#include "set_polling_rate_use_case.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>

namespace mc::app::use_cases {

core::Result<void> SetPollingRateUseCase::execute(core::domain::IMouseAdapter* adapter, int rate_hz) {
    if (!adapter || !adapter->is_connected()) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "No mouse adapter connected");
    }

    auto caps_res = adapter->get_capabilities();
    if (!caps_res) {
        return caps_res.error();
    }

    const auto& caps = caps_res.value();
    if (!caps.supports_polling_rate) {
        return core::Result<void>(core::ErrorCode::NotSupported, "Device does not support polling rate adjustment");
    }

    const auto& allowed = caps.supported_polling_rates;
    if (std::find(allowed.begin(), allowed.end(), rate_hz) == allowed.end()) {
        return core::Result<void>(core::ErrorCode::InvalidParameter,
            fmt::format("Polling rate {} Hz is not supported by device", rate_hz));
    }

    auto set_res = adapter->set_polling_rate(rate_hz);
    if (!set_res) {
        MC_ERROR("Failed to set polling rate: {}", set_res.error().to_string());
        return set_res;
    }

    MC_INFO("SetPollingRateUseCase: successfully set polling rate to {} Hz", rate_hz);
    return core::Result<void>::ok();
}

} // namespace mc::app::use_cases
