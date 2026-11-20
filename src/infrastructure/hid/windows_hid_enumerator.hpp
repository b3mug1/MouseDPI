#pragma once

#include "core/domain/interfaces/i_device_enumerator.hpp"
#include <string>

namespace mc::infra::hid {

class WindowsHidEnumerator : public core::domain::IDeviceEnumerator {
public:
    WindowsHidEnumerator() = default;
    ~WindowsHidEnumerator() override = default;

    [[nodiscard]] core::Result<std::vector<core::domain::DeviceInfo>> enumerate_devices() override;

    [[nodiscard]] core::Result<std::optional<core::domain::DeviceInfo>> find_by_path(const std::wstring& device_path) override;

    static std::string wide_to_utf8(const std::wstring& wstr);
    static std::wstring utf8_to_wide(const std::string& str);
};

} // namespace mc::infra::hid
