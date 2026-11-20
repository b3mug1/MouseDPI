#include "startup_manager.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/hid/windows_hid_enumerator.hpp"
#include <vector>

namespace mc::infra::windows {

std::wstring StartupManager::get_executable_path() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (len >= path.size()) {
        path.resize(32768);
        len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    path.resize(len);
    return path;
}

bool StartupManager::is_startup_enabled() {
    HKEY hKey = nullptr;
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        RUN_KEY_PATH,
        0,
        KEY_READ,
        &hKey
    );

    if (status != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    status = RegQueryValueExW(hKey, APP_VALUE_NAME, nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);

    return (status == ERROR_SUCCESS);
}

bool StartupManager::set_startup_enabled(bool enable, bool start_minimized) {
    HKEY hKey = nullptr;
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        RUN_KEY_PATH,
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (status != ERROR_SUCCESS) {
        MC_WARN("Failed to open Registry Run key for writing (Error: {})", status);
        return false;
    }

    if (enable) {
        std::wstring exe_path = get_executable_path();
        std::wstring cmd = L"\"" + exe_path + L"\"";
        if (start_minimized) {
            cmd += L" --minimized";
        }

        status = RegSetValueExW(
            hKey,
            APP_VALUE_NAME,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(cmd.c_str()),
            static_cast<DWORD>((cmd.length() + 1) * sizeof(wchar_t))
        );

        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS) {
            MC_INFO("Configured Windows autostart: {}", infra::hid::WindowsHidEnumerator::wide_to_utf8(cmd));
            return true;
        } else {
            MC_ERROR("Failed to write autostart registry value (Error: {})", status);
            return false;
        }
    } else {
        status = RegDeleteValueW(hKey, APP_VALUE_NAME);
        RegCloseKey(hKey);
        if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) {
            MC_INFO("Removed Windows autostart registry entry");
            return true;
        } else {
            MC_ERROR("Failed to remove autostart registry value (Error: {})", status);
            return false;
        }
    }
}

} // namespace mc::infra::windows
