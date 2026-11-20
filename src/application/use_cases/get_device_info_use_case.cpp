#include "get_device_info_use_case.hpp"

namespace mc::app::use_cases {

GetDeviceInfoUseCase::GetDeviceInfoUseCase(std::shared_ptr<DiscoverDevicesUseCase> discover_use_case)
    : discover_use_case_(std::move(discover_use_case)) {}

core::Result<std::optional<core::domain::MouseDevice>> GetDeviceInfoUseCase::execute(
    std::string_view adapter_filter) {
    auto devices_res = discover_use_case_->execute();
    if (!devices_res) {
        return devices_res.error();
    }

    for (const auto& dev : devices_res.value()) {
        if (adapter_filter.empty() || dev.adapter_name() == adapter_filter) {
            return std::optional<core::domain::MouseDevice>(dev);
        }
    }

    return std::optional<core::domain::MouseDevice>(std::nullopt);
}

} // namespace mc::app::use_cases
