#pragma once

#include <string>
#include "core/types.hpp"
#include "core/domain/entities/mouse_profile.hpp"

namespace mc::infra::serialization {

class ProfileSerializer {
public:
    static std::string to_json(const core::domain::MouseProfile& profile, bool pretty = true);

    static core::Result<core::domain::MouseProfile> from_json(const std::string& json_str);
};

} // namespace mc::infra::serialization
