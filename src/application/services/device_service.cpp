#include "device_service.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/hid/windows_hid_enumerator.hpp"

namespace mc::app::services {

DeviceService::DeviceService(
    std::shared_ptr<use_cases::DiscoverDevicesUseCase> discover_use_case,
    std::shared_ptr<infra::adapters::AdapterRegistry> registry,
    std::shared_ptr<use_cases::SetDPIUseCase> set_dpi_use_case,
    std::shared_ptr<use_cases::SetPollingRateUseCase> set_polling_rate_use_case,
    std::shared_ptr<infra::hid::WindowsDeviceNotifier> notifier)
    : discover_use_case_(std::move(discover_use_case)),
      registry_(std::move(registry)),
      set_dpi_use_case_(std::move(set_dpi_use_case)),
      set_polling_rate_use_case_(std::move(set_polling_rate_use_case)),
      notifier_(std::move(notifier)) {}

DeviceService::~DeviceService() {
    if (notifier_) {
        notifier_->stop_monitoring();
    }
    if (active_adapter_) {
        active_adapter_->disconnect();
    }
}

void DeviceService::initialize() {
    if (notifier_) {
        notifier_->set_event_callback([this](const std::wstring& path, core::domain::DeviceEventType event_type) {
            on_device_event(path, event_type);
        });
        notifier_->start_monitoring();
    }
    refresh_devices();
}

void DeviceService::connect_active_adapter() {
    if (!active_index_.has_value() || active_index_.value() >= cached_devices_.size()) {
        active_adapter_.reset();
        return;
    }

    auto& dev = cached_devices_[active_index_.value()];
    if (dev.interfaces().empty()) {
        return;
    }

    if (registry_) {
        active_adapter_ = registry_->create_adapter_for(dev.interfaces().front());
        if (active_adapter_) {
            auto conn_res = active_adapter_->connect(dev.interfaces().front());
            if (conn_res) {
                // Populate initial device state
                auto bat_res = active_adapter_->get_battery_level();
                if (bat_res) {
                    dev.set_battery_level(bat_res.value());
                }
                auto dpi_res = active_adapter_->get_dpi();
                if (dpi_res) {
                    dev.set_current_dpi(dpi_res.value());
                }
                auto poll_res = active_adapter_->get_polling_rate();
                if (poll_res) {
                    dev.set_current_polling_rate(poll_res.value());
                }
                MC_INFO("DeviceService: active adapter '{}' connected to '{}' (Battery: {}%, DPI: {}, Rate: {}Hz)",
                    active_adapter_->get_adapter_name(), dev.name(), dev.battery_level(), dev.current_dpi(), dev.current_polling_rate());
            }
        }
    }
}

void DeviceService::refresh_devices() {
    auto res = discover_use_case_->execute();
    if (!res) {
        MC_ERROR("Failed to refresh devices: {}", res.error().to_string());
        return;
    }

    DeviceListChangedCallback list_cb;
    DeviceUpdatedCallback update_cb;
    std::vector<core::domain::MouseDevice> current;
    std::optional<core::domain::MouseDevice> active_dev;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cached_devices_ = std::move(res.value());

        // Prefer selecting Ajazz hardware or first available mouse
        if (!active_index_.has_value() && !cached_devices_.empty()) {
            active_index_ = 0;
            for (size_t i = 0; i < cached_devices_.size(); ++i) {
                if (cached_devices_[i].adapter_name() == "AjazzAdapter") {
                    active_index_ = i;
                    break;
                }
            }
        } else if (active_index_.has_value() && active_index_.value() >= cached_devices_.size()) {
            active_index_ = cached_devices_.empty() ? std::nullopt : std::optional<size_t>(0);
        }

        connect_active_adapter();

        current = cached_devices_;
        if (active_index_.has_value() && active_index_.value() < cached_devices_.size()) {
            active_dev = cached_devices_[active_index_.value()];
        }
        list_cb = on_changed_callback_;
        update_cb = on_updated_callback_;
    }

    if (list_cb) {
        list_cb(current);
    }
    if (update_cb && active_dev.has_value()) {
        update_cb(active_dev.value());
    }
}

std::vector<core::domain::MouseDevice> DeviceService::get_cached_devices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cached_devices_;
}

std::optional<core::domain::MouseDevice> DeviceService::get_active_device() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (active_index_.has_value() && active_index_.value() < cached_devices_.size()) {
        return cached_devices_[active_index_.value()];
    }
    return std::nullopt;
}

core::Result<void> DeviceService::select_active_device(size_t index) {
    DeviceUpdatedCallback cb;
    core::domain::MouseDevice updated("Unknown", "Unknown", core::ConnectionType::Unknown);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= cached_devices_.size()) {
            return core::Result<void>(core::ErrorCode::InvalidParameter, "Device index out of bounds");
        }
        active_index_ = index;
        connect_active_adapter();
        updated = cached_devices_[index];
        cb = on_updated_callback_;
    }

    if (cb) {
        cb(updated);
    }
    return core::Result<void>::ok();
}

core::Result<void> DeviceService::set_dpi(int dpi) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_adapter_) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "No active device adapter");
    }

    auto res = set_dpi_use_case_->execute(active_adapter_.get(), dpi);
    if (res && active_index_.has_value()) {
        cached_devices_[active_index_.value()].set_current_dpi(dpi);
        if (on_updated_callback_) {
            on_updated_callback_(cached_devices_[active_index_.value()]);
        }
    }
    return res;
}

core::Result<void> DeviceService::set_polling_rate(int rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_adapter_) {
        return core::Result<void>(core::ErrorCode::DeviceNotFound, "No active device adapter");
    }

    auto res = set_polling_rate_use_case_->execute(active_adapter_.get(), rate);
    if (res && active_index_.has_value()) {
        cached_devices_[active_index_.value()].set_current_polling_rate(rate);
        if (on_updated_callback_) {
            on_updated_callback_(cached_devices_[active_index_.value()]);
        }
    }
    return res;
}

core::Result<int> DeviceService::refresh_battery_level() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_adapter_) {
        return core::Result<int>(core::ErrorCode::DeviceNotFound, "No active device adapter");
    }

    auto res = active_adapter_->get_battery_level();
    if (res && active_index_.has_value()) {
        cached_devices_[active_index_.value()].set_battery_level(res.value());
        if (on_updated_callback_) {
            on_updated_callback_(cached_devices_[active_index_.value()]);
        }
    }
    return res;
}

void DeviceService::set_on_devices_changed(DeviceListChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_changed_callback_ = std::move(callback);
}

void DeviceService::set_on_device_updated(DeviceUpdatedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_updated_callback_ = std::move(callback);
}

void DeviceService::on_device_event(const std::wstring& device_path, core::domain::DeviceEventType event_type) {
    MC_INFO("Device event: {} - path: {}", 
        (event_type == core::domain::DeviceEventType::Connected ? "Arrival" : "Removal"),
        infra::hid::WindowsHidEnumerator::wide_to_utf8(device_path));
    refresh_devices();
}

} // namespace mc::app::services
