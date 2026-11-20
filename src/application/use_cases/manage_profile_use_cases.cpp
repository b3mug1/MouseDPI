#include "manage_profile_use_cases.hpp"
#include "infrastructure/logging/logger.hpp"
#include <objbase.h>
#include <chrono>

namespace mc::app::use_cases {

namespace {

std::string generate_id() {
    GUID guid;
    if (CoCreateGuid(&guid) == S_OK) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return std::string(buf);
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "profile-" + std::to_string(ms);
}

} // namespace

// CreateProfileUseCase
CreateProfileUseCase::CreateProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<core::domain::MouseProfile> CreateProfileUseCase::execute(
    std::string name, std::string device_id, std::vector<core::domain::DpiStage> stages, int polling_rate) {
    if (!repo_) {
        return core::Result<core::domain::MouseProfile>(core::ErrorCode::InvalidOperation, "Repository is null");
    }

    if (name.empty()) {
        return core::Result<core::domain::MouseProfile>(core::ErrorCode::InvalidParameter, "Profile name cannot be empty");
    }

    if (stages.empty()) {
        stages.push_back({0, 800, true, 0x00FF00});
    }

    core::domain::MouseProfile profile;
    profile.id = generate_id();
    profile.name = std::move(name);
    profile.device_identifier = std::move(device_id);
    profile.dpi_stages = std::move(stages);
    profile.active_stage_index = 0;
    profile.polling_rate = (polling_rate > 0 ? polling_rate : 1000);

    auto save_res = repo_->save(profile);
    if (!save_res) {
        return save_res.error();
    }

    MC_INFO("CreateProfileUseCase: created profile '{}' ({})", profile.name, profile.id);
    return profile;
}

// UpdateProfileUseCase
UpdateProfileUseCase::UpdateProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<void> UpdateProfileUseCase::execute(const core::domain::MouseProfile& profile) {
    if (!repo_) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Repository is null");
    }

    if (profile.id.empty()) {
        return core::Result<void>(core::ErrorCode::InvalidParameter, "Profile ID cannot be empty");
    }

    return repo_->save(profile);
}

// DeleteProfileUseCase
DeleteProfileUseCase::DeleteProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<void> DeleteProfileUseCase::execute(const std::string& profile_id) {
    if (!repo_) {
        return core::Result<void>(core::ErrorCode::InvalidOperation, "Repository is null");
    }

    if (profile_id.empty()) {
        return core::Result<void>(core::ErrorCode::InvalidParameter, "Profile ID cannot be empty");
    }

    return repo_->remove(profile_id);
}

// SwitchProfileUseCase
SwitchProfileUseCase::SwitchProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<core::domain::MouseProfile> SwitchProfileUseCase::execute(
    const std::string& profile_id, core::domain::IMouseAdapter* adapter) {
    if (!repo_) {
        return core::Result<core::domain::MouseProfile>(core::ErrorCode::InvalidOperation, "Repository is null");
    }

    auto prof_res = repo_->get_by_id(profile_id);
    if (!prof_res) {
        return prof_res.error();
    }

    if (!prof_res.value().has_value()) {
        return core::Result<core::domain::MouseProfile>(core::ErrorCode::DeviceNotFound, "Profile not found");
    }

    const auto& profile = prof_res.value().value();

    if (adapter && adapter->is_connected()) {
        if (!profile.dpi_stages.empty()) {
            size_t idx = static_cast<size_t>(profile.active_stage_index);
            if (idx >= profile.dpi_stages.size()) {
                idx = 0;
            }
            int target_dpi = profile.dpi_stages[idx].dpi;
            auto dpi_res = adapter->set_dpi(target_dpi);
            if (!dpi_res) {
                MC_WARN("SwitchProfile: hardware set_dpi failed: {}", dpi_res.error().to_string());
            }
        }

        if (profile.polling_rate > 0) {
            auto rate_res = adapter->set_polling_rate(profile.polling_rate);
            if (!rate_res) {
                MC_WARN("SwitchProfile: hardware set_polling_rate failed: {}", rate_res.error().to_string());
            }
        }
    }

    MC_INFO("SwitchProfileUseCase: switched to profile '{}' ({})", profile.name, profile.id);
    return profile;
}

// GetProfilesUseCase
GetProfilesUseCase::GetProfilesUseCase(std::shared_ptr<core::domain::IProfileRepository> repo)
    : repo_(std::move(repo)) {}

core::Result<std::vector<core::domain::MouseProfile>> GetProfilesUseCase::execute(const std::string& device_id) {
    if (!repo_) {
        return core::Result<std::vector<core::domain::MouseProfile>>(core::ErrorCode::InvalidOperation, "Repository is null");
    }

    if (!device_id.empty()) {
        return repo_->get_by_device(device_id);
    }
    return repo_->get_all();
}

} // namespace mc::app::use_cases
