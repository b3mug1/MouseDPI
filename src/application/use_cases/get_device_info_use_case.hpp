#pragma once

#include <optional>
#include <memory>
#include "core/types.hpp"
#include "core/domain/entities/mouse_device.hpp"
#include "discover_devices_use_case.hpp"

namespace mc::app::use_cases {

class GetDeviceInfoUseCase {
public:
    explicit GetDeviceInfoUseCase(std::shared_ptr<DiscoverDevicesUseCase> discover_use_case);

    [[nodiscard]] core::Result<std::optional<core::domain::MouseDevice>> execute(std::string_view adapter_filter);

private:
    std::shared_ptr<DiscoverDevicesUseCase> discover_use_case_;
};

} // namespace mc::app::use_cases
