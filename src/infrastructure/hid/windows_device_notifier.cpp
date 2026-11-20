#include "windows_device_notifier.hpp"
#include "infrastructure/logging/logger.hpp"
#include <hidsdi.h>

namespace mc::infra::hid {

WindowsDeviceNotifier::WindowsDeviceNotifier(HWND hwnd) : hwnd_(hwnd) {}

WindowsDeviceNotifier::~WindowsDeviceNotifier() {
    stop_monitoring();
}

void WindowsDeviceNotifier::set_hwnd(HWND hwnd) {
    if (hwnd_ != hwnd) {
        bool was_monitoring = is_monitoring_;
        stop_monitoring();
        hwnd_ = hwnd;
        if (was_monitoring) {
            start_monitoring();
        }
    }
}

void WindowsDeviceNotifier::set_event_callback(core::domain::DeviceEventCallback callback) {
    callback_ = std::move(callback);
}

void WindowsDeviceNotifier::start_monitoring() {
    if (is_monitoring_ || hwnd_ == nullptr) {
        return;
    }

    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = hid_guid;

    notification_handle_ = RegisterDeviceNotificationW(
        hwnd_,
        &filter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    if (notification_handle_ != nullptr) {
        is_monitoring_ = true;
        MC_INFO("Device connection monitoring initialized via RegisterDeviceNotification");
    } else {
        DWORD err = GetLastError();
        MC_ERROR("RegisterDeviceNotification failed. Windows error: 0x{:X}", err);
    }
}

void WindowsDeviceNotifier::stop_monitoring() {
    if (notification_handle_ != nullptr) {
        UnregisterDeviceNotification(notification_handle_);
        notification_handle_ = nullptr;
    }
    is_monitoring_ = false;
}

bool WindowsDeviceNotifier::handle_device_change(WPARAM wparam, LPARAM lparam) {
    if (!callback_) {
        return false;
    }

    if (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE) {
        auto header = reinterpret_cast<PDEV_BROADCAST_HDR>(lparam);
        if (header != nullptr && header->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
            auto dev_interface = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE_W>(lparam);
            std::wstring path = dev_interface->dbcc_name;

            core::domain::DeviceEventType event_type = (wparam == DBT_DEVICEARRIVAL)
                ? core::domain::DeviceEventType::Connected
                : core::domain::DeviceEventType::Disconnected;

            callback_(path, event_type);
            return true;
        }
    }

    return false;
}

} // namespace mc::infra::hid
