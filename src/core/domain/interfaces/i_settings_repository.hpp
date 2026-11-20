#pragma once

#include "core/types.hpp"
#include "core/domain/entities/device_settings.hpp"

namespace mc::core::domain {

class ISettingsRepository {
public:
    virtual ~ISettingsRepository() = default;

    virtual Result<void> save_settings(const DeviceSettings& settings) = 0;

    virtual Result<DeviceSettings> load_settings() = 0;
};

} // namespace mc::core::domain
