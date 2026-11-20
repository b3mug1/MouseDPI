#pragma once

#include <string>
#include <vector>
#include <memory>
#include "device_info.hpp"
#include "mouse_capabilities.hpp"
#include "mouse_profile.hpp"

namespace mc::core::domain {

class MouseDevice {
public:
    MouseDevice(std::string name, std::string manufacturer, ConnectionType conn_type)
        : name_(std::move(name)), manufacturer_(std::move(manufacturer)), connection_type_(conn_type) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& manufacturer() const noexcept { return manufacturer_; }
    [[nodiscard]] ConnectionType connection_type() const noexcept { return connection_type_; }
    [[nodiscard]] bool is_connected() const noexcept { return is_connected_; }
    void set_connected(bool connected) noexcept { is_connected_ = connected; }

    [[nodiscard]] const MouseCapabilities& capabilities() const noexcept { return capabilities_; }
    void set_capabilities(MouseCapabilities caps) noexcept { capabilities_ = std::move(caps); }

    [[nodiscard]] const std::vector<DeviceInfo>& interfaces() const noexcept { return interfaces_; }
    void add_interface(DeviceInfo info) { interfaces_.push_back(std::move(info)); }
    void clear_interfaces() { interfaces_.clear(); }

    [[nodiscard]] const std::string& adapter_name() const noexcept { return adapter_name_; }
    void set_adapter_name(std::string name) { adapter_name_ = std::move(name); }

    [[nodiscard]] int battery_level() const noexcept { return battery_level_; }
    void set_battery_level(int level) noexcept { battery_level_ = level; }

    [[nodiscard]] int current_dpi() const noexcept { return current_dpi_; }
    void set_current_dpi(int dpi) noexcept { current_dpi_ = dpi; }

    [[nodiscard]] int current_polling_rate() const noexcept { return current_polling_rate_; }
    void set_current_polling_rate(int rate) noexcept { current_polling_rate_ = rate; }

    [[nodiscard]] int active_stage_index() const noexcept { return active_stage_index_; }
    void set_active_stage_index(int idx) noexcept { active_stage_index_ = idx; }

    [[nodiscard]] const std::vector<DpiStage>& dpi_stages() const noexcept { return dpi_stages_; }
    void set_dpi_stages(std::vector<DpiStage> stages) { dpi_stages_ = std::move(stages); }

    [[nodiscard]] const std::string& active_profile_id() const noexcept { return active_profile_id_; }
    void set_active_profile_id(std::string id) { active_profile_id_ = std::move(id); }

private:
    std::string name_;
    std::string manufacturer_;
    ConnectionType connection_type_{ConnectionType::Unknown};
    bool is_connected_{true};
    MouseCapabilities capabilities_;
    std::vector<DeviceInfo> interfaces_;
    std::string adapter_name_{"GenericAdapter"};
    int battery_level_{-1};
    int current_dpi_{800};
    int current_polling_rate_{1000};
    int active_stage_index_{0};
    std::vector<DpiStage> dpi_stages_;
    std::string active_profile_id_;
};

} // namespace mc::core::domain
