#pragma once

#include <memory>
#include "core/types.hpp"
#include "core/domain/entities/device_settings.hpp"
#include "core/domain/interfaces/i_settings_repository.hpp"

namespace mc::app::use_cases {

class SaveSettingsUseCase {
public:
    explicit SaveSettingsUseCase(std::shared_ptr<core::domain::ISettingsRepository> repo);
    core::Result<void> execute(const core::domain::DeviceSettings& settings);

private:
    std::shared_ptr<core::domain::ISettingsRepository> repo_;
};

class RestoreSettingsUseCase {
public:
    explicit RestoreSettingsUseCase(std::shared_ptr<core::domain::ISettingsRepository> repo);
    core::Result<core::domain::DeviceSettings> execute();

private:
    std::shared_ptr<core::domain::ISettingsRepository> repo_;
};

} // namespace mc::app::use_cases
