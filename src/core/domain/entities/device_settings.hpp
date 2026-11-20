#pragma once

#include <string>

namespace mc::core::domain {

struct DeviceSettings {
    bool start_with_windows{false};
    bool minimize_to_tray{true};
    bool close_to_tray{true};
    bool show_notifications{true};
    std::string default_profile_id;
    std::string active_device_id;
};

} // namespace mc::core::domain
