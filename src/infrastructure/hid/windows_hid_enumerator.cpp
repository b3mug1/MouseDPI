#include "windows_hid_enumerator.hpp"
#include "infrastructure/logging/logger.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <algorithm>
#include <vector>

namespace mc::infra::hid {

namespace {

struct ScopedDevInfo {
    HDEVINFO handle{INVALID_HANDLE_VALUE};
    ~ScopedDevInfo() {
        if (handle != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(handle);
        }
    }
};

struct ScopedHandle {
    HANDLE handle{INVALID_HANDLE_VALUE};
    ~ScopedHandle() {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
            CloseHandle(handle);
        }
    }
};

struct ScopedPreparsedData {
    PHIDP_PREPARSED_DATA data{nullptr};
    ~ScopedPreparsedData() {
        if (data != nullptr) {
            HidD_FreePreparsedData(data);
        }
    }
};

int parse_interface_number(const std::wstring& path) {
    auto pos = path.find(L"&mi_");
    if (pos == std::wstring::npos) {
        pos = path.find(L"&MI_");
    }
    if (pos != std::wstring::npos && pos + 6 <= path.length()) {
        try {
            std::wstring num_str = path.substr(pos + 4, 2);
            return std::stoi(num_str, nullptr, 16);
        } catch (...) {}
    }
    return -1;
}

core::ConnectionType detect_connection_type(const std::wstring& path, uint16_t vid, uint16_t pid) {
    std::wstring lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::towlower);

    if (lower_path.find(L"bthle") != std::wstring::npos || 
        lower_path.find(L"bluetooth") != std::wstring::npos ||
        lower_path.find(L"00001812") != std::wstring::npos) {
        return core::ConnectionType::BluetoothLE;
    }

    // Ajazz AJ159 specific known dongle PIDs
    if (vid == 0x248A && pid == 0x5D2E) {
        return core::ConnectionType::Wireless24G;
    }

    if (lower_path.find(L"usb") != std::wstring::npos) {
        return core::ConnectionType::WiredUsb;
    }

    return core::ConnectionType::Unknown;
}

std::wstring get_registry_property(HDEVINFO dev_info, SP_DEVINFO_DATA* devinfo_data, DWORD prop) {
    DWORD req_size = 0;
    DWORD reg_type = 0;
    SetupDiGetDeviceRegistryPropertyW(dev_info, devinfo_data, prop, &reg_type, nullptr, 0, &req_size);
    if (req_size == 0) return {};
    std::vector<wchar_t> buf(req_size / sizeof(wchar_t) + 2, 0);
    if (SetupDiGetDeviceRegistryPropertyW(dev_info, devinfo_data, prop, &reg_type, 
                                          reinterpret_cast<PBYTE>(buf.data()), req_size, nullptr)) {
        if (reg_type == REG_MULTI_SZ) {
            std::wstring combined;
            const wchar_t* p = buf.data();
            while (*p) {
                if (!combined.empty()) combined += L" ";
                combined += p;
                p += wcslen(p) + 1;
            }
            return combined;
        }
        return std::wstring(buf.data());
    }
    return {};
}

bool parse_ids_from_hardware_id(const std::wstring& hwid, const std::wstring& path,
                                uint16_t& vid, uint16_t& pid, uint16_t& usage_page, uint16_t& usage) {
    std::wstring lower = hwid + L" " + path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    // Parse VID (e.g. vid&02248a, vid_248a, vid&248a)
    auto pos = lower.find(L"vid&02");
    if (pos != std::wstring::npos && pos + 10 <= lower.length()) {
        try { vid = static_cast<uint16_t>(std::stoul(lower.substr(pos + 6, 4), nullptr, 16)); } catch (...) {}
    } else {
        pos = lower.find(L"vid_");
        if (pos != std::wstring::npos && pos + 8 <= lower.length()) {
            try { vid = static_cast<uint16_t>(std::stoul(lower.substr(pos + 4, 4), nullptr, 16)); } catch (...) {}
        } else {
            pos = lower.find(L"vid&");
            if (pos != std::wstring::npos && pos + 8 <= lower.length()) {
                try { vid = static_cast<uint16_t>(std::stoul(lower.substr(pos + 4, 4), nullptr, 16)); } catch (...) {}
            }
        }
    }

    // Parse PID (e.g. pid&8266, pid_8266)
    pos = lower.find(L"pid&");
    if (pos != std::wstring::npos && pos + 8 <= lower.length()) {
        try { pid = static_cast<uint16_t>(std::stoul(lower.substr(pos + 4, 4), nullptr, 16)); } catch (...) {}
    } else {
        pos = lower.find(L"pid_");
        if (pos != std::wstring::npos && pos + 8 <= lower.length()) {
            try { pid = static_cast<uint16_t>(std::stoul(lower.substr(pos + 4, 4), nullptr, 16)); } catch (...) {}
        }
    }

    // Parse Usage Page and Usage (e.g. up:0001_u:0002 or up:ff01_u:0001)
    pos = lower.find(L"up:");
    if (pos != std::wstring::npos && pos + 7 <= lower.length()) {
        try { usage_page = static_cast<uint16_t>(std::stoul(lower.substr(pos + 3, 4), nullptr, 16)); } catch (...) {}
    }
    pos = lower.find(L"_u:");
    if (pos != std::wstring::npos && pos + 7 <= lower.length()) {
        try { usage = static_cast<uint16_t>(std::stoul(lower.substr(pos + 3, 4), nullptr, 16)); } catch (...) {}
    }

    // Known device mappings for collections when UP/U are not directly present in HWID string:
    if (usage_page == 0 && usage == 0) {
        if (lower.find(L"col01") != std::wstring::npos && lower.find(L"00001812") != std::wstring::npos) {
            usage_page = 0x0001; // Generic Desktop
            usage = 0x0002;      // Mouse
        } else if (lower.find(L"col02") != std::wstring::npos && lower.find(L"00001812") != std::wstring::npos) {
            usage_page = 0xFF01; // Vendor Management Page
            usage = 0x0001;
        } else if (lower.find(L"mi_00") != std::wstring::npos) {
            usage_page = 0x0001;
            usage = 0x0002;
        } else if (lower.find(L"mi_01&col01") != std::wstring::npos || lower.find(L"kbd") != std::wstring::npos) {
            usage_page = 0x0001;
            usage = 0x0006;      // Keyboard
        } else if (lower.find(L"mi_01&col02") != std::wstring::npos) {
            usage_page = 0x000C; // Consumer Control
            usage = 0x0001;
        } else if (lower.find(L"mi_01&col03") != std::wstring::npos || lower.find(L"mi_02") != std::wstring::npos) {
            usage_page = 0xFF00; // Vendor Defined
            usage = 0x0001;
        }
    }

    return (vid != 0);
}

} // namespace

std::string WindowsHidEnumerator::wide_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), str.data(), size_needed, nullptr, nullptr);
    return str;
}

std::wstring WindowsHidEnumerator::utf8_to_wide(const std::string& str) {
    if (str.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (size_needed <= 0) return {};
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr.data(), size_needed);
    return wstr;
}

core::Result<std::vector<core::domain::DeviceInfo>> WindowsHidEnumerator::enumerate_devices() {
    std::vector<core::domain::DeviceInfo> devices;

    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);

    ScopedDevInfo dev_info{SetupDiGetClassDevsW(
        &hid_guid,
        nullptr,
        nullptr,
        DIGCF_DEVICEINTERFACE
    )};

    if (dev_info.handle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        MC_ERROR("Failed to query HID device info list. Windows error: 0x{:X}", err);
        return core::Result<std::vector<core::domain::DeviceInfo>>(
            core::ErrorCode::EnumerationFailed, "SetupDiGetClassDevsW failed", err);
    }

    SP_DEVICE_INTERFACE_DATA interface_data{};
    interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info.handle, nullptr, &hid_guid, i, &interface_data); ++i) {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetailW(dev_info.handle, &interface_data, nullptr, 0, &required_size, nullptr);

        if (required_size == 0) {
            continue;
        }

        std::vector<uint8_t> detail_buffer(required_size);
        auto detail_data = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detail_buffer.data());
        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devinfo_data{};
        devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiGetDeviceInterfaceDetailW(
                dev_info.handle,
                &interface_data,
                detail_data,
                required_size,
                nullptr,
                &devinfo_data)) {
            continue;
        }

        std::wstring device_path = detail_data->DevicePath;

        // Open device handle without requiring exclusive access (0 access for querying capabilities)
        ScopedHandle h_device{CreateFileW(
            device_path.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        )};

        if (h_device.handle == INVALID_HANDLE_VALUE) {
            // Some drivers require read/write access even for metadata, try fallback with GENERIC_READ
            h_device.handle = CreateFileW(
                device_path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );
        }

        core::domain::DeviceInfo info;
        info.device_path_w = device_path;
        info.device_path = wide_to_utf8(device_path);
        info.interface_number = parse_interface_number(device_path);

        if (h_device.handle != INVALID_HANDLE_VALUE) {
            HIDD_ATTRIBUTES attrib{};
            attrib.Size = sizeof(HIDD_ATTRIBUTES);
            if (!HidD_GetAttributes(h_device.handle, &attrib)) {
                continue;
            }

            info.is_active = true;
            info.vendor_id = attrib.VendorID;
            info.product_id = attrib.ProductID;
            info.version_number = attrib.VersionNumber;

            // Retrieve strings
            wchar_t str_buffer[256] = {0};
            if (HidD_GetManufacturerString(h_device.handle, str_buffer, sizeof(str_buffer))) {
                info.manufacturer = wide_to_utf8(str_buffer);
            }
            if (HidD_GetProductString(h_device.handle, str_buffer, sizeof(str_buffer))) {
                info.product_name = wide_to_utf8(str_buffer);
            }
            if (HidD_GetSerialNumberString(h_device.handle, str_buffer, sizeof(str_buffer))) {
                info.serial_number = wide_to_utf8(str_buffer);
            }

            // Retrieve capabilities
            ScopedPreparsedData preparsed{};
            if (HidD_GetPreparsedData(h_device.handle, &preparsed.data) && preparsed.data != nullptr) {
                HIDP_CAPS caps{};
                if (HidP_GetCaps(preparsed.data, &caps) == HIDP_STATUS_SUCCESS) {
                    info.usage_page = caps.UsagePage;
                    info.usage = caps.Usage;
                    info.input_report_byte_length = caps.InputReportByteLength;
                    info.output_report_byte_length = caps.OutputReportByteLength;
                    info.feature_report_byte_length = caps.FeatureReportByteLength;

                    info.is_mouse = (caps.UsagePage == 0x0001 && (caps.Usage == 0x0002 || caps.Usage == 0x0001));
                    info.is_vendor_defined = (caps.UsagePage >= 0xFF00);
                }
            }
        } else {
            // Live handle could not be opened (device in low-power standby or asleep).
            // Inspect device node properties via SetupAPI.
            std::wstring hwid = get_registry_property(dev_info.handle, &devinfo_data, SPDRP_HARDWAREID);
            uint16_t vid = 0, pid = 0, up = 0, u = 0;
            if (parse_ids_from_hardware_id(hwid, device_path, vid, pid, up, u) && vid != 0) {
                // Check if this is an Ajazz gaming peripheral or targeted device
                if (vid == 0x248A || vid == 0x093A) {
                    info.is_active = false;
                    info.vendor_id = vid;
                    info.product_id = pid;
                    info.usage_page = up;
                    info.usage = u;
                    info.is_mouse = (up == 0x0001 && (u == 0x0002 || u == 0x0001));
                    info.is_vendor_defined = (up >= 0xFF00);

                    std::wstring desc = get_registry_property(dev_info.handle, &devinfo_data, SPDRP_DEVICEDESC);
                    std::wstring mfg  = get_registry_property(dev_info.handle, &devinfo_data, SPDRP_MFG);
                    info.product_name = !desc.empty() ? wide_to_utf8(desc) : "Ajazz Device";
                    info.manufacturer = !mfg.empty() ? wide_to_utf8(mfg) : "Ajazz";
                } else {
                    continue;
                }
            } else {
                continue;
            }
        }

        info.connection_type = detect_connection_type(device_path, info.vendor_id, info.product_id);
        devices.push_back(std::move(info));
    }

    MC_DEBUG("Enumerated {} HID device interfaces", devices.size());
    return devices;
}

core::Result<std::optional<core::domain::DeviceInfo>> WindowsHidEnumerator::find_by_path(const std::wstring& device_path) {
    auto res = enumerate_devices();
    if (!res) {
        return res.error();
    }

    std::wstring search_lower = device_path;
    std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::towlower);

    for (const auto& dev : res.value()) {
        std::wstring current_lower = dev.device_path_w;
        std::transform(current_lower.begin(), current_lower.end(), current_lower.begin(), ::towlower);
        if (current_lower == search_lower) {
            return std::optional<core::domain::DeviceInfo>(dev);
        }
    }

    return std::optional<core::domain::DeviceInfo>(std::nullopt);
}

} // namespace mc::infra::hid
