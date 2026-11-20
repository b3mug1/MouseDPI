#include "manage_settings_use_cases.hpp"

namespace mc::app::use_cases {

SaveSettingsUseCase::SaveSettingsUseCase(std::shared_ptr<core::domain::ISettingsRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<void> SaveSettingsUseCase::execute(const core::domain::DeviceSettings& settings) {
    if (!repo_) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Repository is null");
    }
    return repo_->save_settings(settings);
}

RestoreSettingsUseCase::RestoreSettingsUseCase(std::shared_ptr<core::domain::ISettingsRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<core::domain::DeviceSettings> RestoreSettingsUseCase::execute() {
    if (!repo_) {
        return core::Result<core::domain::DeviceSettings>(core::ErrorCode::InvalidOperation, "Repository is null");
    }
    return repo_->load_settings();
}

} // namespace mc::app::use_cases
