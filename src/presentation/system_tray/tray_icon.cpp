#include "tray_icon.hpp"

namespace mc::presentation::tray {

TrayIcon::TrayIcon(HWND hwnd, UINT uID) : hwnd_(hwnd), uid_(uID) {}

TrayIcon::~TrayIcon() {
    remove();
}

bool TrayIcon::create(const std::wstring& tooltip) {
    if (created_) return true;

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd_;
    nid_.uID = uid_;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);

    if (Shell_NotifyIconW(NIM_ADD, &nid_)) {
        nid_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
        created_ = true;
        return true;
    }

    return false;
}

bool TrayIcon::update_tooltip(const std::wstring& tooltip) {
    if (!created_) return false;
    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);
    nid_.uFlags = NIF_TIP;
    return Shell_NotifyIconW(NIM_MODIFY, &nid_) != FALSE;
}

bool TrayIcon::show_notification(const std::wstring& title, const std::wstring& message, bool is_warning) {
    if (!created_) return false;

    nid_.uFlags = NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, message.c_str(), _TRUNCATE);
    nid_.dwInfoFlags = (is_warning ? NIIF_WARNING : NIIF_INFO) | NIIF_LARGE_ICON;
    nid_.uTimeout = 3000;

    return Shell_NotifyIconW(NIM_MODIFY, &nid_) != FALSE;
}

void TrayIcon::remove() {
    if (created_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        created_ = false;
    }
}

void TrayIcon::show_menu(int current_dpi, int current_rate) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING, ID_TRAY_SHOW, L"Open MouseControl");
        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);

        // DPI Submenu
        HMENU hDpiMenu = CreatePopupMenu();
        if (hDpiMenu) {
            auto add_dpi = [&](UINT id, int val, const wchar_t* txt) {
                UINT flags = MF_BYPOSITION | MF_STRING | (current_dpi == val ? MF_CHECKED : MF_UNCHECKED);
                InsertMenuW(hDpiMenu, static_cast<UINT>(-1), flags, id, txt);
            };
            add_dpi(ID_TRAY_DPI_400, 400, L"400 DPI");
            add_dpi(ID_TRAY_DPI_800, 800, L"800 DPI");
            add_dpi(ID_TRAY_DPI_1200, 1200, L"1200 DPI");
            add_dpi(ID_TRAY_DPI_1600, 1600, L"1600 DPI");
            add_dpi(ID_TRAY_DPI_2400, 2400, L"2400 DPI");
            add_dpi(ID_TRAY_DPI_3200, 3200, L"3200 DPI");
            add_dpi(ID_TRAY_DPI_6400, 6400, L"6400 DPI");
            InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(hDpiMenu), L"DPI Presets");
        }

        // Polling Rate Submenu
        HMENU hRateMenu = CreatePopupMenu();
        if (hRateMenu) {
            auto add_rate = [&](UINT id, int val, const wchar_t* txt) {
                UINT flags = MF_BYPOSITION | MF_STRING | (current_rate == val ? MF_CHECKED : MF_UNCHECKED);
                InsertMenuW(hRateMenu, static_cast<UINT>(-1), flags, id, txt);
            };
            add_rate(ID_TRAY_RATE_125, 125, L"125 Hz (8ms)");
            add_rate(ID_TRAY_RATE_250, 250, L"250 Hz (4ms)");
            add_rate(ID_TRAY_RATE_500, 500, L"500 Hz (2ms)");
            add_rate(ID_TRAY_RATE_1000, 1000, L"1000 Hz (1ms)");
            InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(hRateMenu), L"Polling Rate");
        }

        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING, ID_TRAY_REFRESH, L"Refresh Devices");
        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

        // Make Open bold
        SetMenuDefaultItem(hMenu, ID_TRAY_SHOW, FALSE);

        SetForegroundWindow(hwnd_);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
        DestroyMenu(hMenu);
    }
}

} // namespace mc::presentation::tray
