#pragma once

#include <string>
#include <vector>
#include <optional>
#include "core/types.hpp"
#include "core/domain/entities/mouse_profile.hpp"

namespace mc::core::domain {

class IProfileRepository {
public:
    virtual ~IProfileRepository() = default;

    virtual Result<void> save(const MouseProfile& profile) = 0;

    virtual Result<std::optional<MouseProfile>> get_by_id(const std::string& id) = 0;

    virtual Result<std::vector<MouseProfile>> get_by_device(const std::string& device_id) = 0;

    virtual Result<std::vector<MouseProfile>> get_all() = 0;

    virtual Result<void> remove(const std::string& id) = 0;
};

} // namespace mc::core::domain
