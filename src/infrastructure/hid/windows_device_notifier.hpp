#pragma once

#include "core/domain/interfaces/i_device_notifier.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbt.h>

namespace mc::infra::hid {

class WindowsDeviceNotifier : public core::domain::IDeviceNotifier {
public:
    explicit WindowsDeviceNotifier(HWND hwnd = nullptr);
    ~WindowsDeviceNotifier() override;

    void set_hwnd(HWND hwnd);
    void set_event_callback(core::domain::DeviceEventCallback callback) override;
    void start_monitoring() override;
    void stop_monitoring() override;

    // Call from WndProc when WM_DEVICECHANGE is received
    bool handle_device_change(WPARAM wparam, LPARAM lparam);

private:
    HWND hwnd_{nullptr};
    HDEVNOTIFY notification_handle_{nullptr};
    core::domain::DeviceEventCallback callback_;
    bool is_monitoring_{false};
};

} // namespace mc::infra::hid
