#pragma once

#include <memory>
#include "core/types.hpp"
#include "core/domain/entities/mouse_capabilities.hpp"
#include "core/domain/entities/device_info.hpp"
#include "infrastructure/adapters/adapter_registry.hpp"

namespace mc::app::use_cases {

class GetDeviceCapabilitiesUseCase {
public:
    explicit GetDeviceCapabilitiesUseCase(std::shared_ptr<infra::adapters::AdapterRegistry> registry);

    core::Result<core::domain::MouseCapabilities> execute(const core::domain::DeviceInfo& device_info);

private:
    std::shared_ptr<infra::adapters::AdapterRegistry> registry_;
};

} // namespace mc::app::use_cases
