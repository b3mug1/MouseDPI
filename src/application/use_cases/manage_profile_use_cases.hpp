#pragma once

#include <memory>
#include <vector>
#include <string>
#include "core/types.hpp"
#include "core/domain/entities/mouse_profile.hpp"
#include "core/domain/interfaces/i_profile_repository.hpp"
#include "core/domain/interfaces/i_mouse_adapter.hpp"

namespace mc::app::use_cases {

class CreateProfileUseCase {
public:
    explicit CreateProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo);
    core::Result<core::domain::MouseProfile> execute(std::string name, std::string device_id, std::vector<core::domain::DpiStage> stages, int polling_rate);

private:
    std::shared_ptr<core::domain::IProfileRepository> repo_;
};

class UpdateProfileUseCase {
public:
    explicit UpdateProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo);
    core::Result<void> execute(const core::domain::MouseProfile& profile);

private:
    std::shared_ptr<core::domain::IProfileRepository> repo_;
};

class DeleteProfileUseCase {
public:
    explicit DeleteProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo);
    core::Result<void> execute(const std::string& profile_id);

private:
    std::shared_ptr<core::domain::IProfileRepository> repo_;
};

class SwitchProfileUseCase {
public:
    explicit SwitchProfileUseCase(std::shared_ptr<core::domain::IProfileRepository> repo);
    core::Result<core::domain::MouseProfile> execute(const std::string& profile_id, core::domain::IMouseAdapter* adapter);

private:
    std::shared_ptr<core::domain::IProfileRepository> repo_;
};

class GetProfilesUseCase {
public:
    explicit GetProfilesUseCase(std::shared_ptr<core::domain::IProfileRepository> repo);
    core::Result<std::vector<core::domain::MouseProfile>> execute(const std::string& device_id = "");

private:
    std::shared_ptr<core::domain::IProfileRepository> repo_;
};

} // namespace mc::app::use_cases
