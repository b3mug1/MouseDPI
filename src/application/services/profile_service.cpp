#include "profile_service.hpp"
#include "infrastructure/serialization/profile_serializer.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>

namespace mc::app::services {

ProfileService::ProfileService(
    std::shared_ptr<use_cases::CreateProfileUseCase> create_use_case,
    std::shared_ptr<use_cases::UpdateProfileUseCase> update_use_case,
    std::shared_ptr<use_cases::DeleteProfileUseCase> delete_use_case,
    std::shared_ptr<use_cases::SwitchProfileUseCase> switch_use_case,
    std::shared_ptr<use_cases::GetProfilesUseCase> get_use_case)
    : create_use_case_(std::move(create_use_case)),
      update_use_case_(std::move(update_use_case)),
      delete_use_case_(std::move(delete_use_case)),
      switch_use_case_(std::move(switch_use_case)),
      get_use_case_(std::move(get_use_case)) {}

core::Result<core::domain::MouseProfile> ProfileService::create_profile(
    std::string name, std::string device_id, std::vector<core::domain::DpiStage> stages, int polling_rate) {
    auto res = create_use_case_->execute(std::move(name), std::move(device_id), std::move(stages), polling_rate);
    if (res) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_profile_.has_value()) {
            active_profile_ = res.value();
        }
    }
    return res;
}

core::Result<void> ProfileService::update_profile(const core::domain::MouseProfile& profile) {
    auto res = update_use_case_->execute(profile);
    if (res) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_profile_.has_value() && active_profile_->id == profile.id) {
            active_profile_ = profile;
        }
    }
    return res;
}

core::Result<void> ProfileService::delete_profile(const std::string& profile_id) {
    auto res = delete_use_case_->execute(profile_id);
    if (res) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_profile_.has_value() && active_profile_->id == profile_id) {
            active_profile_.reset();
        }
    }
    return res;
}

core::Result<core::domain::MouseProfile> ProfileService::switch_profile(
    const std::string& profile_id, core::domain::IMouseAdapter* adapter) {
    auto res = switch_use_case_->execute(profile_id, adapter);
    if (res) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_profile_ = res.value();
    }
    return res;
}

core::Result<std::vector<core::domain::MouseProfile>> ProfileService::get_profiles(const std::string& device_id) {
    return get_use_case_->execute(device_id);
}

std::optional<core::domain::MouseProfile> ProfileService::get_active_profile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_profile_;
}

core::Result<std::string> ProfileService::export_profile(const std::string& profile_id, bool pretty) {
    auto list_res = get_use_case_->execute();
    if (!list_res) return list_res.error();

    for (const auto& p : list_res.value()) {
        if (p.id == profile_id) {
            return infra::serialization::ProfileSerializer::to_json(p, pretty);
        }
    }
    return core::Result<std::string>(core::ErrorCode::DeviceNotFound, "Profile not found for export");
}

core::Result<core::domain::MouseProfile> ProfileService::import_profile(
    const std::string& json_str, const std::string& device_id) {
    auto parse_res = infra::serialization::ProfileSerializer::from_json(json_str);
    if (!parse_res) {
        return parse_res.error();
    }

    auto p = parse_res.value();
    if (!device_id.empty()) {
        p.device_identifier = device_id;
    }

    return create_use_case_->execute(p.name, p.device_identifier, p.dpi_stages, p.polling_rate);
}

core::Result<void> ProfileService::validate_profile(
    const core::domain::MouseProfile& profile, const core::domain::MouseCapabilities& caps) {
    if (caps.supports_dpi) {
        for (const auto& s : profile.dpi_stages) {
            if (s.dpi < caps.min_dpi || s.dpi > caps.max_dpi) {
                return core::Result<void>(core::ErrorCode::InvalidParameter,
                    fmt::format("Profile stage DPI {} is out of hardware range [{}, {}]", s.dpi, caps.min_dpi, caps.max_dpi));
            }
            if (caps.dpi_step > 1 && ((s.dpi - caps.min_dpi) % caps.dpi_step != 0)) {
                return core::Result<void>(core::ErrorCode::InvalidParameter,
                    fmt::format("Profile stage DPI {} violates step size {}", s.dpi, caps.dpi_step));
            }
        }
    }

    if (caps.supports_polling_rate && profile.polling_rate > 0) {
        const auto& allowed = caps.supported_polling_rates;
        if (std::find(allowed.begin(), allowed.end(), profile.polling_rate) == allowed.end()) {
            return core::Result<void>(core::ErrorCode::InvalidParameter,
                fmt::format("Profile polling rate {} Hz is unsupported by hardware", profile.polling_rate));
        }
    }

    return core::Result<void>::ok();
}

} // namespace mc::app::services
