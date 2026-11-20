#pragma once

#include <vector>
#include <cstdint>

namespace mc::core::domain {

struct MouseCapabilities {
    bool supports_dpi{false};
    int min_dpi{50};
    int max_dpi{26000};
    int dpi_step{50};

    bool supports_polling_rate{false};
    std::vector<int> supported_polling_rates{125, 250, 500, 1000};

    int max_dpi_stages{6};
    bool supports_battery_level{false};
    bool supports_onboard_profiles{false};
    bool supports_lod{false};
};

} // namespace mc::core::domain
