#pragma once

#include <string>
#include <functional>

namespace mc::core::domain {

enum class DeviceEventType {
    Connected,
    Disconnected
};

using DeviceEventCallback = std::function<void(const std::wstring& device_path, DeviceEventType event_type)>;

class IDeviceNotifier {
public:
    virtual ~IDeviceNotifier() = default;

    virtual void set_event_callback(DeviceEventCallback callback) = 0;
    virtual void start_monitoring() = 0;
    virtual void stop_monitoring() = 0;
};

} // namespace mc::core::domain
