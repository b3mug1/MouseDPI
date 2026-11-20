#pragma once

#include <vector>
#include <optional>
#include <string>
#include "core/types.hpp"
#include "core/domain/entities/device_info.hpp"

namespace mc::core::domain {

class IDeviceEnumerator {
public:
    virtual ~IDeviceEnumerator() = default;

    [[nodiscard]] virtual Result<std::vector<DeviceInfo>> enumerate_devices() = 0;

    [[nodiscard]] virtual Result<std::optional<DeviceInfo>> find_by_path(const std::wstring& device_path) = 0;
};

} // namespace mc::core::domain
