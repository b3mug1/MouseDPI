#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>

namespace mc::infra::windows {

class StartupManager {
public:
    static constexpr const wchar_t* RUN_KEY_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static constexpr const wchar_t* APP_VALUE_NAME = L"MouseControl";

    static bool is_startup_enabled();
    static bool set_startup_enabled(bool enable, bool start_minimized = true);
    static std::wstring get_executable_path();
};

} // namespace mc::infra::windows
