#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include "core/types.hpp"
#include "core/domain/entities/mouse_profile.hpp"
#include "core/domain/entities/mouse_capabilities.hpp"
#include "core/domain/interfaces/i_mouse_adapter.hpp"
#include "application/use_cases/manage_profile_use_cases.hpp"

namespace mc::app::services {

class ProfileService {
public:
    ProfileService(
        std::shared_ptr<use_cases::CreateProfileUseCase> create_use_case,
        std::shared_ptr<use_cases::UpdateProfileUseCase> update_use_case,
        std::shared_ptr<use_cases::DeleteProfileUseCase> delete_use_case,
        std::shared_ptr<use_cases::SwitchProfileUseCase> switch_use_case,
        std::shared_ptr<use_cases::GetProfilesUseCase> get_use_case);

    core::Result<core::domain::MouseProfile> create_profile(
        std::string name, std::string device_id, std::vector<core::domain::DpiStage> stages, int polling_rate);

    core::Result<void> update_profile(const core::domain::MouseProfile& profile);

    core::Result<void> delete_profile(const std::string& profile_id);

    core::Result<core::domain::MouseProfile> switch_profile(
        const std::string& profile_id, core::domain::IMouseAdapter* adapter);

    core::Result<std::vector<core::domain::MouseProfile>> get_profiles(const std::string& device_id = "");

    [[nodiscard]] std::optional<core::domain::MouseProfile> get_active_profile() const;

    core::Result<std::string> export_profile(const std::string& profile_id, bool pretty = true);

    core::Result<core::domain::MouseProfile> import_profile(const std::string& json_str, const std::string& device_id = "");

    static core::Result<void> validate_profile(
        const core::domain::MouseProfile& profile, const core::domain::MouseCapabilities& caps);

private:
    std::shared_ptr<use_cases::CreateProfileUseCase> create_use_case_;
    std::shared_ptr<use_cases::UpdateProfileUseCase> update_use_case_;
    std::shared_ptr<use_cases::DeleteProfileUseCase> delete_use_case_;
    std::shared_ptr<use_cases::SwitchProfileUseCase> switch_use_case_;
    std::shared_ptr<use_cases::GetProfilesUseCase> get_use_case_;

    mutable std::mutex mutex_;
    std::optional<core::domain::MouseProfile> active_profile_;
};

} // namespace mc::app::services
