#pragma once

#include <vector>
#include <memory>
#include "core/types.hpp"
#include "core/domain/entities/mouse_device.hpp"
#include "core/domain/interfaces/i_device_enumerator.hpp"
#include "infrastructure/adapters/adapter_registry.hpp"

namespace mc::app::use_cases {

class DiscoverDevicesUseCase {
public:
    DiscoverDevicesUseCase(
        std::shared_ptr<core::domain::IDeviceEnumerator> enumerator,
        std::shared_ptr<infra::adapters::AdapterRegistry> registry);

    [[nodiscard]] core::Result<std::vector<core::domain::MouseDevice>> execute();

private:
    std::shared_ptr<core::domain::IDeviceEnumerator> enumerator_;
    std::shared_ptr<infra::adapters::AdapterRegistry> registry_;
};

} // namespace mc::app::use_cases
