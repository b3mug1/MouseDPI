#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <optional>
#include "core/types.hpp"
#include "core/domain/entities/mouse_device.hpp"
#include "core/domain/interfaces/i_mouse_adapter.hpp"
#include "application/use_cases/discover_devices_use_case.hpp"
#include "application/use_cases/set_dpi_use_case.hpp"
#include "application/use_cases/set_polling_rate_use_case.hpp"
#include "infrastructure/adapters/adapter_registry.hpp"
#include "infrastructure/hid/windows_device_notifier.hpp"

namespace mc::app::services {

class DeviceService {
public:
    using DeviceListChangedCallback = std::function<void(const std::vector<core::domain::MouseDevice>&)>;
    using DeviceUpdatedCallback = std::function<void(const core::domain::MouseDevice&)>;

    DeviceService(
        std::shared_ptr<use_cases::DiscoverDevicesUseCase> discover_use_case,
        std::shared_ptr<infra::adapters::AdapterRegistry> registry,
        std::shared_ptr<use_cases::SetDPIUseCase> set_dpi_use_case,
        std::shared_ptr<use_cases::SetPollingRateUseCase> set_polling_rate_use_case,
        std::shared_ptr<infra::hid::WindowsDeviceNotifier> notifier);

    ~DeviceService();

    void initialize();
    void refresh_devices();

    [[nodiscard]] std::vector<core::domain::MouseDevice> get_cached_devices() const;
    [[nodiscard]] std::optional<core::domain::MouseDevice> get_active_device() const;

    core::Result<void> select_active_device(size_t index);
    core::Result<void> set_dpi(int dpi);
    core::Result<void> set_polling_rate(int rate);
    core::Result<int> refresh_battery_level();

    void set_on_devices_changed(DeviceListChangedCallback callback);
    void set_on_device_updated(DeviceUpdatedCallback callback);

private:
    void on_device_event(const std::wstring& device_path, core::domain::DeviceEventType event_type);
    void connect_active_adapter();

    std::shared_ptr<use_cases::DiscoverDevicesUseCase> discover_use_case_;
    std::shared_ptr<infra::adapters::AdapterRegistry> registry_;
    std::shared_ptr<use_cases::SetDPIUseCase> set_dpi_use_case_;
    std::shared_ptr<use_cases::SetPollingRateUseCase> set_polling_rate_use_case_;
    std::shared_ptr<infra::hid::WindowsDeviceNotifier> notifier_;

    mutable std::mutex mutex_;
    std::vector<core::domain::MouseDevice> cached_devices_;
    std::optional<size_t> active_index_;
    std::unique_ptr<core::domain::IMouseAdapter> active_adapter_;

    DeviceListChangedCallback on_changed_callback_;
    DeviceUpdatedCallback on_updated_callback_;
};

} // namespace mc::app::services
