#include "generic_adapter.hpp"
#include "infrastructure/logging/logger.hpp"

namespace mc::infra::adapters::generic {

core::Result<bool> GenericAdapter::connect(const core::domain::DeviceInfo& device) {
    connected_device_ = device;
    is_connected_ = true;
    MC_INFO("GenericAdapter bound to diagnostics interface ({:04X}:{:04X})", 
        device.vendor_id, device.product_id);
    return true;
}

core::Result<core::domain::MouseCapabilities> GenericAdapter::get_capabilities() {
    core::domain::MouseCapabilities caps{};
    caps.supports_dpi = false;
    caps.supports_polling_rate = false;
    caps.supports_battery_level = false;
    caps.supports_onboard_profiles = false;
    return caps;
}

core::Result<int> GenericAdapter::get_dpi() {
    return core::Result<int>(core::ErrorCode::NotSupported, "GenericAdapter does not support DPI control");
}

core::Result<void> GenericAdapter::set_dpi([[maybe_unused]] int dpi) {
    return core::Result<void>(core::ErrorCode::NotSupported, "GenericAdapter does not support DPI control");
}

core::Result<int> GenericAdapter::get_polling_rate() {
    return core::Result<int>(core::ErrorCode::NotSupported, "GenericAdapter does not support polling rate control");
}

core::Result<void> GenericAdapter::set_polling_rate([[maybe_unused]] int rate) {
    return core::Result<void>(core::ErrorCode::NotSupported, "GenericAdapter does not support polling rate control");
}

void GenericAdapter::disconnect() {
    is_connected_ = false;
}

} // namespace mc::infra::adapters::generic
