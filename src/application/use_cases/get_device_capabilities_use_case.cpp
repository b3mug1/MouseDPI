#include "get_device_capabilities_use_case.hpp"

namespace mc::app::use_cases {

GetDeviceCapabilitiesUseCase::GetDeviceCapabilitiesUseCase(
    std::shared_ptr<infra::adapters::AdapterRegistry> registry)
    : registry_(std::move(registry)) {}

core::Result<core::domain::MouseCapabilities> GetDeviceCapabilitiesUseCase::execute(
    const core::domain::DeviceInfo& device_info) {
    if (!registry_) {
        return core::Result<core::domain::MouseCapabilities>(
            core::ErrorCode::InvalidOperation, "Adapter registry is null");
    }

    auto adapter = registry_->create_adapter_for(device_info);
    if (!adapter) {
        return core::Result<core::domain::MouseCapabilities>(
            core::ErrorCode::DeviceNotFound, "No compatible adapter for device");
    }

    auto conn_res = adapter->connect(device_info);
    if (!conn_res) {
        return conn_res.error();
    }

    auto caps_res = adapter->get_capabilities();
    adapter->disconnect();
    return caps_res;
}

} // namespace mc::app::use_cases
