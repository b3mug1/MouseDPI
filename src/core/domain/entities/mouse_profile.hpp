#pragma once

#include <string>
#include <vector>
#include "dpi_stage.hpp"

namespace mc::core::domain {

struct MouseProfile {
    std::string id;
    std::string name{"Default"};
    std::string device_identifier;

    std::vector<DpiStage> dpi_stages;
    int active_stage_index{0};
    int polling_rate{1000};
};

} // namespace mc::core::domain
