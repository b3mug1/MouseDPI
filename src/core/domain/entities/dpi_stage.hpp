#pragma once

#include <cstdint>

namespace mc::core::domain {

struct DpiStage {
    int index{0};
    int dpi{800};
    bool enabled{true};
    uint32_t color_rgb{0x00FF00}; // Default green indicator
};

} // namespace mc::core::domain
