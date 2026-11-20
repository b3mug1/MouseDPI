#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

namespace mc::presentation::tray {

class TrayIcon {
public:
    static constexpr UINT WM_TRAYICON = WM_APP + 101;
    static constexpr UINT ID_TRAY_SHOW = 2001;
    static constexpr UINT ID_TRAY_REFRESH = 2002;
    static constexpr UINT ID_TRAY_EXIT = 2003;

    // Quick Tray Actions
    static constexpr UINT ID_TRAY_DPI_400  = 3001;
    static constexpr UINT ID_TRAY_DPI_800  = 3002;
    static constexpr UINT ID_TRAY_DPI_1200 = 3003;
    static constexpr UINT ID_TRAY_DPI_1600 = 3004;
    static constexpr UINT ID_TRAY_DPI_2400 = 3005;
    static constexpr UINT ID_TRAY_DPI_3200 = 3006;
    static constexpr UINT ID_TRAY_DPI_6400 = 3007;

    static constexpr UINT ID_TRAY_RATE_125  = 3101;
    static constexpr UINT ID_TRAY_RATE_250  = 3102;
    static constexpr UINT ID_TRAY_RATE_500  = 3103;
    static constexpr UINT ID_TRAY_RATE_1000 = 3104;

    TrayIcon(HWND hwnd, UINT uID);
    ~TrayIcon();

    bool create(const std::wstring& tooltip);
    bool update_tooltip(const std::wstring& tooltip);
    bool show_notification(const std::wstring& title, const std::wstring& message, bool is_warning = false);
    void remove();

    void show_menu(int current_dpi = 0, int current_rate = 0);

private:
    HWND hwnd_{nullptr};
    UINT uid_{0};
    bool created_{false};
    NOTIFYICONDATAW nid_{};
};

} // namespace mc::presentation::tray
