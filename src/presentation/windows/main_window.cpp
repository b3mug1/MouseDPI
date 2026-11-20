#include "main_window.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/hid/windows_hid_enumerator.hpp"
#include "infrastructure/windows/startup_manager.hpp"
#include <psapi.h>
#include <format>
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <dwmapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace mc::presentation::windows {
namespace {

constexpr COLORREF kCanvas = RGB(13, 17, 23);
constexpr COLORREF kSidebar = RGB(16, 21, 29);
constexpr COLORREF kPanel = RGB(23, 30, 40);
constexpr COLORREF kPanelRaised = RGB(29, 38, 50);
constexpr COLORREF kBorder = RGB(39, 51, 66);
constexpr COLORREF kAccent = RGB(190, 242, 100);
constexpr COLORREF kText = RGB(237, 242, 247);
constexpr COLORREF kMuted = RGB(145, 158, 174);
constexpr COLORREF kGreen = RGB(105, 220, 151);
constexpr COLORREF kRed = RGB(255, 112, 112);

void draw_text(HDC dc, HFONT font, COLORREF color, const wchar_t* text, RECT rect, UINT flags = DT_LEFT | DT_TOP | DT_SINGLELINE) {
    SelectObject(dc, font);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rect, flags);
}

void draw_panel(HDC dc, RECT rect, HBRUSH brush, HPEN pen, int radius = 12) {
    SelectObject(dc, brush);
    SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
}

void draw_owner_button(const DRAWITEMSTRUCT& item, bool checked = false) {
    const UINT id = item.CtlID;
    const bool primary = id == MainWindow::ID_BTN_REFRESH
        || id == MainWindow::ID_BTN_DPI_APPLY
        || id == MainWindow::ID_BTN_SAVE_SETTINGS
        || id == MainWindow::ID_BTN_PROF_SWITCH
        || id == MainWindow::ID_BTN_PROF_NEW;
    const bool checkbox = id >= MainWindow::ID_CHK_STARTUP && id <= MainWindow::ID_CHK_NOTIFY;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF fill = disabled ? RGB(32, 38, 46)
        : primary ? (pressed ? RGB(147, 195, 72) : kAccent)
        : pressed ? RGB(43, 56, 71) : kPanelRaised;
    const COLORREF foreground = primary ? kCanvas : disabled ? kMuted : kText;

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, primary ? fill : kBorder);
    HGDIOBJ old_brush = SelectObject(item.hDC, brush);
    HGDIOBJ old_pen = SelectObject(item.hDC, pen);
    SetBkMode(item.hDC, TRANSPARENT);

    if (checkbox) {
        RECT box{ item.rcItem.left + 2, item.rcItem.top + 5, item.rcItem.left + 21, item.rcItem.top + 24 };
        HBRUSH box_brush = CreateSolidBrush(checked ? kAccent : kPanelRaised);
        HPEN box_pen = CreatePen(PS_SOLID, 1, kBorder);
        HGDIOBJ old_box_brush = SelectObject(item.hDC, box_brush);
        HGDIOBJ old_box_pen = SelectObject(item.hDC, box_pen);
        RoundRect(item.hDC, box.left, box.top, box.right, box.bottom, 4, 4);
        if (checked) {
            HPEN check_pen = CreatePen(PS_SOLID, 2, kCanvas);
            HGDIOBJ old_check_pen = SelectObject(item.hDC, check_pen);
            MoveToEx(item.hDC, box.left + 4, box.top + 9, nullptr);
            LineTo(item.hDC, box.left + 8, box.top + 13);
            LineTo(item.hDC, box.left + 16, box.top + 5);
            SelectObject(item.hDC, old_check_pen);
            DeleteObject(check_pen);
        }
        SelectObject(item.hDC, old_box_brush);
        SelectObject(item.hDC, old_box_pen);
        DeleteObject(box_brush);
        DeleteObject(box_pen);

        RECT label{ item.rcItem.left + 32, item.rcItem.top, item.rcItem.right, item.rcItem.bottom };
        const int length = GetWindowTextLengthW(item.hwndItem);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(item.hwndItem, text.data(), length + 1);
        draw_text(item.hDC, reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0)),
            disabled ? kMuted : kText, text.c_str(), label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, 9, 9);
        const int length = GetWindowTextLengthW(item.hwndItem);
        std::wstring text(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(item.hwndItem, text.data(), length + 1);
        draw_text(item.hDC, reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0)),
            foreground, text.c_str(), item.rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(item.hDC, &focus);
    }

    SelectObject(item.hDC, old_brush);
    SelectObject(item.hDC, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

} // namespace

MainWindow::MainWindow(
    HINSTANCE hInstance,
    std::shared_ptr<app::services::DeviceService> device_service,
    std::shared_ptr<app::services::ProfileService> profile_service,
    std::shared_ptr<core::domain::ISettingsRepository> settings_repo)
    : hInstance_(hInstance),
      device_service_(std::move(device_service)),
      profile_service_(std::move(profile_service)),
      settings_repo_(std::move(settings_repo)) {

    bg_brush_          = CreateSolidBrush(kCanvas);
    sidebar_brush_     = CreateSolidBrush(kSidebar);
    card_brush_        = CreateSolidBrush(kPanel);
    card_active_brush_ = CreateSolidBrush(kPanelRaised);
    header_brush_      = CreateSolidBrush(kSidebar);
    border_pen_        = CreatePen(PS_SOLID, 1, kBorder);
    accent_pen_        = CreatePen(PS_SOLID, 2, kAccent);
    green_pen_         = CreatePen(PS_SOLID, 2, kGreen);

    font_title_ = CreateFontW(23, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_header_ = CreateFontW(27, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_bold_  = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_body_  = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_mono_  = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              FIXED_PITCH | FF_MODERN, L"Consolas");
    font_large_stat_ = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

MainWindow::~MainWindow() {
    if (hwnd_) {
        UnregisterHotKey(hwnd_, ID_HOTKEY_DPI_CYCLE);
        UnregisterHotKey(hwnd_, ID_HOTKEY_DPI_UP);
        UnregisterHotKey(hwnd_, ID_HOTKEY_DPI_DOWN);
    }
    if (tray_icon_) {
        tray_icon_->remove();
    }
    if (bg_brush_) DeleteObject(bg_brush_);
    if (sidebar_brush_) DeleteObject(sidebar_brush_);
    if (card_brush_) DeleteObject(card_brush_);
    if (card_active_brush_) DeleteObject(card_active_brush_);
    if (header_brush_) DeleteObject(header_brush_);
    if (border_pen_) DeleteObject(border_pen_);
    if (accent_pen_) DeleteObject(accent_pen_);
    if (green_pen_) DeleteObject(green_pen_);
    if (font_title_) DeleteObject(font_title_);
    if (font_header_) DeleteObject(font_header_);
    if (font_bold_) DeleteObject(font_bold_);
    if (font_body_) DeleteObject(font_body_);
    if (font_mono_) DeleteObject(font_mono_);
    if (font_large_stat_) DeleteObject(font_large_stat_);
}

void MainWindow::set_status_message(std::wstring msg, bool is_error) {
    status_message_ = std::move(msg);
    status_is_error_ = is_error;
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void MainWindow::switch_tab(NavTab tab) {
    active_tab_ = tab;
    update_controls_visibility();
    if (active_tab_ == NavTab::Profiles) {
        refresh_profile_list();
    } else if (active_tab_ == NavTab::Settings) {
        load_settings_into_ui();
    }
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void MainWindow::update_controls_visibility() {
    auto show_ctrl = [](HWND h, bool show) {
        if (h) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
    };

    bool is_dpi  = (active_tab_ == NavTab::Dpi);
    bool is_poll = (active_tab_ == NavTab::PollingRate);
    bool is_prof = (active_tab_ == NavTab::Profiles);
    bool is_sett = (active_tab_ == NavTab::Settings);
    show_ctrl(btn_refresh_, active_tab_ == NavTab::Dashboard || active_tab_ == NavTab::DeviceInfo);

    // DPI Controls
    show_ctrl(btn_dpi_400_, is_dpi);
    show_ctrl(btn_dpi_800_, is_dpi);
    show_ctrl(btn_dpi_1200_, is_dpi);
    show_ctrl(btn_dpi_1600_, is_dpi);
    show_ctrl(btn_dpi_2400_, is_dpi);
    show_ctrl(btn_dpi_3200_, is_dpi);
    show_ctrl(btn_dpi_6400_, is_dpi);
    show_ctrl(slider_dpi_, is_dpi);
    show_ctrl(btn_dpi_minus_, is_dpi);
    show_ctrl(btn_dpi_plus_, is_dpi);
    show_ctrl(btn_dpi_apply_, is_dpi);
    show_ctrl(btn_dpi_reset_, is_dpi);
    show_ctrl(edit_dpi_custom_, is_dpi);

    // Polling Rate Controls
    show_ctrl(btn_rate_125_, is_poll);
    show_ctrl(btn_rate_250_, is_poll);
    show_ctrl(btn_rate_500_, is_poll);
    show_ctrl(btn_rate_1000_, is_poll);

    // Profile Controls
    show_ctrl(list_profiles_, is_prof);
    show_ctrl(btn_prof_switch_, is_prof);
    show_ctrl(btn_prof_new_, is_prof);
    show_ctrl(btn_prof_duplicate_, is_prof);
    show_ctrl(btn_prof_delete_, is_prof);
    show_ctrl(btn_prof_export_, is_prof);
    show_ctrl(btn_prof_import_, is_prof);

    // Settings Controls
    show_ctrl(chk_startup_, is_sett);
    show_ctrl(chk_min_tray_, is_sett);
    show_ctrl(chk_close_tray_, is_sett);
    show_ctrl(chk_notify_, is_sett);
    show_ctrl(btn_save_settings_, is_sett);
}

void MainWindow::refresh_profile_list() {
    if (!list_profiles_ || !profile_service_) return;
    SendMessageW(list_profiles_, LB_RESETCONTENT, 0, 0);

    auto list_res = profile_service_->get_profiles();
    if (list_res) {
        cached_profiles_ = list_res.value();
        auto active_p = profile_service_->get_active_profile();

        for (size_t i = 0; i < cached_profiles_.size(); ++i) {
            std::wstring item_text = infra::hid::WindowsHidEnumerator::utf8_to_wide(cached_profiles_[i].name);
            if (active_p && active_p->id == cached_profiles_[i].id) {
                item_text += L" [ACTIVE]";
            }
            item_text += std::format(L"  ({} Hz)", cached_profiles_[i].polling_rate);
            SendMessageW(list_profiles_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item_text.c_str()));
        }
        if (!cached_profiles_.empty()) {
            SendMessageW(list_profiles_, LB_SETCURSEL, 0, 0);
        }
    }
}

void MainWindow::load_settings_into_ui() {
    if (!settings_repo_) return;
    auto s_res = settings_repo_->load_settings();
    if (s_res) {
        const auto& s = s_res.value();
        start_with_windows_ = s.start_with_windows;
        minimize_to_tray_   = s.minimize_to_tray;
        close_to_tray_      = s.close_to_tray;
        show_notifications_ = s.show_notifications;

    }
}

void MainWindow::save_settings_from_ui() {
    if (!settings_repo_) return;
    core::domain::DeviceSettings s;
    s.start_with_windows = start_with_windows_;
    s.minimize_to_tray   = minimize_to_tray_;
    s.close_to_tray      = close_to_tray_;
    s.show_notifications = show_notifications_;

    start_with_windows_ = s.start_with_windows;
    minimize_to_tray_   = s.minimize_to_tray;
    close_to_tray_      = s.close_to_tray;
    show_notifications_ = s.show_notifications;

    auto res = settings_repo_->save_settings(s);

    // Phase 7: Configure Windows Startup via Registry
    infra::windows::StartupManager::set_startup_enabled(s.start_with_windows, s.minimize_to_tray);

    set_status_message(res ? L"Settings saved & autostart updated" : L"Failed to save settings", !res);
}

void MainWindow::trim_working_set() {
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    K32EmptyWorkingSet(GetCurrentProcess());
    query_memory_stats();
}

void MainWindow::hide_window_to_tray() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        trim_working_set();
    }
}

void MainWindow::show_window_from_tray() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        query_memory_stats();
    }
}

void MainWindow::sync_dpi_controls_from_staged() {
    if (edit_dpi_custom_) {
        SetWindowTextW(edit_dpi_custom_, std::to_wstring(staged_dpi_).c_str());
    }
    if (slider_dpi_) {
        SendMessageW(slider_dpi_, TBM_SETPOS, TRUE, staged_dpi_ / 50);
    }
}

void MainWindow::cycle_dpi_stage() {
    const int default_stages[] = { 400, 800, 1200, 1600, 2400, 3200 };
    constexpr int total_stages = 6;

    current_stage_index_ = (current_stage_index_ + 1) % total_stages;
    staged_dpi_ = default_stages[current_stage_index_];
    sync_dpi_controls_from_staged();

    if (device_service_) {
        [[maybe_unused]] auto res = device_service_->set_dpi(staged_dpi_);
    }

    if (show_notifications_ && tray_icon_) {
        tray_icon_->show_notification(
            L"DPI Cycled via Hotkey",
            std::format(L"Sensor DPI: {} (Stage {})", staged_dpi_, current_stage_index_ + 1)
        );
    }

    set_status_message(std::format(L"Hotkey: DPI set to {} (Stage {})", staged_dpi_, current_stage_index_ + 1));
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

bool MainWindow::create(const std::wstring& title, int width, int height) {
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = hInstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = bg_brush_;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExW(&wc);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (screen_w - width) / 2;
    int pos_y = (screen_h - height) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        pos_x, pos_y, width, height,
        nullptr, nullptr, hInstance_, this
    );

    if (!hwnd_) {
        MC_ERROR("Failed to create main Win32 window");
        return false;
    }

    const BOOL use_dark_title_bar = TRUE;
    const HRESULT theme_result = DwmSetWindowAttribute(hwnd_, 20, &use_dark_title_bar, sizeof(use_dark_title_bar));
    if (FAILED(theme_result)) {
        MC_DEBUG("Windows did not apply the dark title bar theme (HRESULT 0x{:X})", static_cast<unsigned long>(theme_result));
    }

    // Phase 7: Register Global Hotkeys (Ctrl+Alt+D for DPI cycle, Ctrl+Alt+Up/Down for step)
    RegisterHotKey(hwnd_, ID_HOTKEY_DPI_CYCLE, MOD_CONTROL | MOD_ALT, 'D');
    RegisterHotKey(hwnd_, ID_HOTKEY_DPI_UP, MOD_CONTROL | MOD_ALT, VK_UP);
    RegisterHotKey(hwnd_, ID_HOTKEY_DPI_DOWN, MOD_CONTROL | MOD_ALT, VK_DOWN);

    // Shared Header Controls
    btn_refresh_ = CreateWindowExW(
        0, L"BUTTON", L"Scan devices",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT,
        width - 180, 18, 140, 32,
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_REFRESH)), hInstance_, nullptr
    );

    // DPI Controls (positioned on DPI page canvas)
    int dpi_y1 = 230;
    btn_dpi_400_  = CreateWindowExW(0, L"BUTTON", L"400", WS_TABSTOP | WS_CHILD | BS_FLAT, 240, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_400)), hInstance_, nullptr);
    btn_dpi_800_  = CreateWindowExW(0, L"BUTTON", L"800", WS_TABSTOP | WS_CHILD | BS_FLAT, 325, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_800)), hInstance_, nullptr);
    btn_dpi_1200_ = CreateWindowExW(0, L"BUTTON", L"1200", WS_TABSTOP | WS_CHILD | BS_FLAT, 410, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_1200)), hInstance_, nullptr);
    btn_dpi_1600_ = CreateWindowExW(0, L"BUTTON", L"1600", WS_TABSTOP | WS_CHILD | BS_FLAT, 495, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_1600)), hInstance_, nullptr);
    btn_dpi_2400_ = CreateWindowExW(0, L"BUTTON", L"2400", WS_TABSTOP | WS_CHILD | BS_FLAT, 580, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_2400)), hInstance_, nullptr);
    btn_dpi_3200_ = CreateWindowExW(0, L"BUTTON", L"3200", WS_TABSTOP | WS_CHILD | BS_FLAT, 665, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_3200)), hInstance_, nullptr);
    btn_dpi_6400_ = CreateWindowExW(0, L"BUTTON", L"6400", WS_TABSTOP | WS_CHILD | BS_FLAT, 750, dpi_y1, 75, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_6400)), hInstance_, nullptr);

    int dpi_y_slider = 295;
    slider_dpi_ = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_TABSTOP | WS_CHILD | TBS_AUTOTICKS | TBS_HORZ,
        240, dpi_y_slider, 585, 30,
        hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SLIDER_DPI)), hInstance_, nullptr
    );
    SendMessageW(slider_dpi_, TBM_SETRANGE, TRUE, MAKELPARAM(1, 520)); // 50 to 26000 (val * 50)
    SendMessageW(slider_dpi_, TBM_SETPOS, TRUE, staged_dpi_ / 50);
    SendMessageW(slider_dpi_, TBM_SETTICFREQ, 20, 0); // 1000 DPI tick interval

    int dpi_y2 = 355;
    btn_dpi_minus_   = CreateWindowExW(0, L"BUTTON", L"-50", WS_TABSTOP | WS_CHILD | BS_FLAT, 240, dpi_y2, 60, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_MINUS)), hInstance_, nullptr);
    edit_dpi_custom_ = CreateWindowExW(0, L"EDIT", L"800", WS_TABSTOP | WS_CHILD | ES_NUMBER | ES_CENTER, 310, dpi_y2 + 2, 80, 30, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_DPI_CUSTOM)), hInstance_, nullptr);
    btn_dpi_plus_    = CreateWindowExW(0, L"BUTTON", L"+50", WS_TABSTOP | WS_CHILD | BS_FLAT, 400, dpi_y2, 60, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_PLUS)), hInstance_, nullptr);
    btn_dpi_apply_   = CreateWindowExW(0, L"BUTTON", L"Apply DPI to Hardware", WS_TABSTOP | WS_CHILD | BS_DEFPUSHBUTTON, 470, dpi_y2, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_APPLY)), hInstance_, nullptr);
    btn_dpi_reset_   = CreateWindowExW(0, L"BUTTON", L"Reset", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, dpi_y2, 80, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_DPI_RESET)), hInstance_, nullptr);

    // Polling Rate Controls
    int rate_y = 200;
    btn_rate_125_  = CreateWindowExW(0, L"BUTTON", L"Set 125 Hz (8ms)", WS_TABSTOP | WS_CHILD | BS_FLAT, 240, rate_y, 160, 42, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_RATE_125)), hInstance_, nullptr);
    btn_rate_250_  = CreateWindowExW(0, L"BUTTON", L"Set 250 Hz (4ms)", WS_TABSTOP | WS_CHILD | BS_FLAT, 420, rate_y, 160, 42, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_RATE_250)), hInstance_, nullptr);
    btn_rate_500_  = CreateWindowExW(0, L"BUTTON", L"Set 500 Hz (2ms)", WS_TABSTOP | WS_CHILD | BS_FLAT, 240, rate_y + 60, 160, 42, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_RATE_500)), hInstance_, nullptr);
    btn_rate_1000_ = CreateWindowExW(0, L"BUTTON", L"Set 1000 Hz (1ms)", WS_TABSTOP | WS_CHILD | BS_FLAT, 420, rate_y + 60, 160, 42, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_RATE_1000)), hInstance_, nullptr);

    // Profile Controls
    list_profiles_      = CreateWindowExW(0, L"LISTBOX", L"", WS_TABSTOP | WS_CHILD | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL, 240, 140, 400, 260, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LIST_PROFILES)), hInstance_, nullptr);
    btn_prof_switch_    = CreateWindowExW(0, L"BUTTON", L"Activate Selected", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, 140, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROF_SWITCH)), hInstance_, nullptr);
    btn_prof_new_       = CreateWindowExW(0, L"BUTTON", L"Create New Profile", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, 180, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROF_NEW)), hInstance_, nullptr);
    btn_prof_duplicate_ = CreateWindowExW(0, L"BUTTON", L"Duplicate Profile", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, 220, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROF_DUPLICATE)), hInstance_, nullptr);
    btn_prof_delete_    = CreateWindowExW(0, L"BUTTON", L"Delete Profile", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, 260, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROF_DELETE)), hInstance_, nullptr);
    btn_prof_export_    = CreateWindowExW(0, L"BUTTON", L"Export to JSON...", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, 310, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROF_EXPORT)), hInstance_, nullptr);
    btn_prof_import_    = CreateWindowExW(0, L"BUTTON", L"Import from JSON...", WS_TABSTOP | WS_CHILD | BS_FLAT, 660, 355, 180, 34, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROF_IMPORT)), hInstance_, nullptr);

    // Settings Controls
    chk_startup_       = CreateWindowExW(0, L"BUTTON", L"Launch MouseControl automatically with Windows", WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX, 240, 140, 450, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHK_STARTUP)), hInstance_, nullptr);
    chk_min_tray_      = CreateWindowExW(0, L"BUTTON", L"Start minimized to System Tray", WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX, 240, 180, 450, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHK_MIN_TRAY)), hInstance_, nullptr);
    chk_close_tray_    = CreateWindowExW(0, L"BUTTON", L"Close button minimizes to System Tray instead of exiting", WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX, 240, 220, 450, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHK_CLOSE_TRAY)), hInstance_, nullptr);
    chk_notify_        = CreateWindowExW(0, L"BUTTON", L"Show notifications when mice connect or disconnect", WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX, 240, 260, 450, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHK_NOTIFY)), hInstance_, nullptr);
    btn_save_settings_ = CreateWindowExW(0, L"BUTTON", L"Save Preferences", WS_TABSTOP | WS_CHILD | BS_DEFPUSHBUTTON, 240, 310, 180, 36, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_SAVE_SETTINGS)), hInstance_, nullptr);

    const std::array<HWND, 27> buttons{
        btn_refresh_, btn_dpi_400_, btn_dpi_800_, btn_dpi_1200_, btn_dpi_1600_,
        btn_dpi_2400_, btn_dpi_3200_, btn_dpi_6400_, btn_dpi_minus_, btn_dpi_plus_,
        btn_dpi_apply_, btn_dpi_reset_, btn_rate_125_, btn_rate_250_, btn_rate_500_,
        btn_rate_1000_, btn_prof_switch_, btn_prof_new_, btn_prof_duplicate_,
        btn_prof_delete_, btn_prof_export_, btn_prof_import_, chk_startup_, chk_min_tray_,
        chk_close_tray_, chk_notify_, btn_save_settings_
    };
    for (HWND button : buttons) {
        LONG_PTR style = GetWindowLongPtrW(button, GWL_STYLE);
        SetWindowLongPtrW(button, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(font_body_), TRUE);
    }
    SendMessageW(edit_dpi_custom_, WM_SETFONT, reinterpret_cast<WPARAM>(font_body_), TRUE);
    SendMessageW(slider_dpi_, WM_SETFONT, reinterpret_cast<WPARAM>(font_body_), TRUE);
    SendMessageW(list_profiles_, WM_SETFONT, reinterpret_cast<WPARAM>(font_body_), TRUE);
    SendMessageW(list_profiles_, LB_SETITEMHEIGHT, 0, 48);

    // Initialize System Tray
    tray_icon_ = std::make_unique<tray::TrayIcon>(hwnd_, 1);
    tray_icon_->create(L"MouseControl \u2014 Active");

    // Load initial settings
    load_settings_into_ui();

    // Hook device service
    if (device_service_) {
        device_service_->set_on_devices_changed([this](const std::vector<core::domain::MouseDevice>& devs) {
            size_t prev_count = devices_.size();
            devices_ = devs;

            // Phase 7: Device connect/disconnect toast notifications
            if (show_notifications_ && tray_icon_ && prev_count > 0) {
                if (devs.size() > prev_count && !devs.empty()) {
                    tray_icon_->show_notification(
                        L"Mouse Connected",
                        std::format(L"{} connected to system", infra::hid::WindowsHidEnumerator::utf8_to_wide(devs.front().name()))
                    );
                } else if (devs.size() < prev_count) {
                    tray_icon_->show_notification(
                        L"Mouse Disconnected",
                        L"Gaming mouse was disconnected",
                        true
                    );
                }
            }

            if (!devs.empty() && tray_icon_) {
                tray_icon_->update_tooltip(std::format(
                    L"MouseControl \u2014 {} ({} DPI, {} Hz)",
                    infra::hid::WindowsHidEnumerator::utf8_to_wide(devs.front().name()),
                    devs.front().current_dpi(),
                    devs.front().current_polling_rate()
                ));
            }

            if (hwnd_) {
                InvalidateRect(hwnd_, nullptr, TRUE);
            }
        });

        device_service_->set_on_device_updated([this](const core::domain::MouseDevice& dev) {
            staged_dpi_ = dev.current_dpi();
            sync_dpi_controls_from_staged();

            // Phase 7: Low battery warning (< 15%)
            if (show_notifications_ && tray_icon_ && dev.battery_level() > 0 && dev.battery_level() <= 15) {
                tray_icon_->show_notification(
                    L"Low Battery Alert",
                    std::format(L"{} battery at {}% \u2014 please recharge",
                        infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.name()), dev.battery_level()),
                    true
                );
            }

            if (hwnd_) {
                InvalidateRect(hwnd_, nullptr, TRUE);
            }
        });

        devices_ = device_service_->get_cached_devices();
        auto act = device_service_->get_active_device();
        if (act) {
            staged_dpi_ = act->current_dpi();
            sync_dpi_controls_from_staged();
            if (tray_icon_) {
                tray_icon_->update_tooltip(std::format(
                    L"MouseControl \u2014 {} ({} DPI, {} Hz)",
                    infra::hid::WindowsHidEnumerator::utf8_to_wide(act->name()),
                    act->current_dpi(),
                    act->current_polling_rate()
                ));
            }
        }
    }

    switch_tab(NavTab::Dashboard);
    layout_controls();
    query_memory_stats();
    return true;
}

void MainWindow::show(int nCmdShow) {
    if (hwnd_) {
        ShowWindow(hwnd_, nCmdShow);
        UpdateWindow(hwnd_);
    }
}

void MainWindow::query_memory_stats() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        ram_working_set_kb_ = pmc.WorkingSetSize / 1024;
        ram_peak_working_set_kb_ = pmc.PeakWorkingSetSize / 1024;
    }
}

void MainWindow::layout_controls() {
    if (!hwnd_) return;

    RECT client{};
    GetClientRect(hwnd_, &client);
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    constexpr int content_x = 260;
    constexpr int content_top = 126;
    constexpr int right_margin = 32;
    const int content_width = (std::max)(320, client_width - content_x - right_margin);
    auto place = [](HWND control, int x, int y, int width, int height) {
        if (control) SetWindowPos(control, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    place(btn_refresh_, client_width - 190, 25, 158, 42);

    const std::array<HWND, 7> dpi_buttons{
        btn_dpi_400_, btn_dpi_800_, btn_dpi_1200_, btn_dpi_1600_,
        btn_dpi_2400_, btn_dpi_3200_, btn_dpi_6400_
    };
    const int dpi_gap = 10;
    const int dpi_button_width = (content_width - dpi_gap * 6) / 7;
    const int dpi_y = content_top + 318;
    for (size_t i = 0; i < dpi_buttons.size(); ++i) {
        place(dpi_buttons[i], content_x + static_cast<int>(i) * (dpi_button_width + dpi_gap), dpi_y, dpi_button_width, 44);
    }

    place(slider_dpi_, content_x + 6, content_top + 424, content_width - 12, 34);
    place(btn_dpi_minus_, content_x, content_top + 488, 72, 42);
    place(edit_dpi_custom_, content_x + 84, content_top + 488, 120, 42);
    place(btn_dpi_plus_, content_x + 216, content_top + 488, 72, 42);
    place(btn_dpi_apply_, content_x + 304, content_top + 488, 190, 42);
    place(btn_dpi_reset_, content_x + 508, content_top + 488, 100, 42);

    const std::array<HWND, 4> rate_buttons{ btn_rate_125_, btn_rate_250_, btn_rate_500_, btn_rate_1000_ };
    const int rate_gap = 14;
    const int rate_width = (content_width - rate_gap * 3) / 4;
    for (size_t i = 0; i < rate_buttons.size(); ++i) {
        place(rate_buttons[i], content_x + static_cast<int>(i) * (rate_width + rate_gap), content_top + 308, rate_width, 86);
    }

    const int profile_list_width = (std::max)(280, content_width * 2 / 3);
    place(list_profiles_, content_x + 18, content_top + 132, profile_list_width, (std::max)(180, client_height - content_top - 196));
    const int actions_x = content_x + profile_list_width + 42;
    const int action_width = (std::max)(150, content_width - profile_list_width - 42);
    place(btn_prof_switch_, actions_x, content_top + 132, action_width, 42);
    place(btn_prof_new_, actions_x, content_top + 186, action_width, 42);
    place(btn_prof_duplicate_, actions_x, content_top + 240, action_width, 42);
    place(btn_prof_delete_, actions_x, content_top + 294, action_width, 42);
    place(btn_prof_export_, actions_x, content_top + 372, action_width, 42);
    place(btn_prof_import_, actions_x, content_top + 426, action_width, 42);

    place(chk_startup_, content_x + 24, content_top + 138, content_width - 48, 36);
    place(chk_min_tray_, content_x + 24, content_top + 194, content_width - 48, 36);
    place(chk_close_tray_, content_x + 24, content_top + 250, content_width - 48, 36);
    place(chk_notify_, content_x + 24, content_top + 306, content_width - 48, 36);
    place(btn_save_settings_, content_x + 24, content_top + 378, 190, 44);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handle_message(uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::handle_message(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE:
            layout_controls();
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;

        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            limits->ptMinTrackSize.x = 920;
            limits->ptMinTrackSize.y = 740;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
            on_paint(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_LISTBOX && item->CtlID == ID_LIST_PROFILES) {
                const bool selected = (item->itemState & ODS_SELECTED) != 0;
                HBRUSH item_brush = CreateSolidBrush(selected ? kPanelRaised : kPanel);
                FillRect(item->hDC, &item->rcItem, item_brush);
                DeleteObject(item_brush);
                wchar_t text[512]{};
                SendMessageW(item->hwndItem, LB_GETTEXT, item->itemID, reinterpret_cast<LPARAM>(text));
                draw_text(item->hDC, font_body_, selected ? kAccent : kText, text,
                    RECT{ item->rcItem.left + 14, item->rcItem.top, item->rcItem.right - 12, item->rcItem.bottom },
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                return TRUE;
            }
            if (item && item->CtlType == ODT_BUTTON) {
                bool checked = false;
                switch (item->CtlID) {
                    case ID_CHK_STARTUP: checked = start_with_windows_; break;
                    case ID_CHK_MIN_TRAY: checked = minimize_to_tray_; break;
                    case ID_CHK_CLOSE_TRAY: checked = close_to_tray_; break;
                    case ID_CHK_NOTIFY: checked = show_notifications_; break;
                    default: break;
                }
                draw_owner_button(*item, checked);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC control_dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(control_dc, kText);
            SetBkColor(control_dc, kPanel);
            return reinterpret_cast<LRESULT>(card_brush_);
        }

        case WM_CTLCOLORSCROLLBAR:
            return reinterpret_cast<LRESULT>(sidebar_brush_);

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Check if click was inside sidebar navigation tabs
            if (x < 232 && y >= 132 && y < 132 + 6 * 50) {
                int clicked_tab = (y - 132) / 50;
                if (clicked_tab >= 0 && clicked_tab <= 5) {
                    switch_tab(static_cast<NavTab>(clicked_tab));
                    return 0;
                }
            }
            break;
        }

        case WM_DEVICECHANGE: {
            MC_DEBUG("Received WM_DEVICECHANGE event (0x{:X})", wParam);
            if (device_service_) {
                device_service_->refresh_devices();
            }
            return TRUE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);

            if (id >= ID_CHK_STARTUP && id <= ID_CHK_NOTIFY) {
                if (id == ID_CHK_STARTUP) start_with_windows_ = !start_with_windows_;
                else if (id == ID_CHK_MIN_TRAY) minimize_to_tray_ = !minimize_to_tray_;
                else if (id == ID_CHK_CLOSE_TRAY) close_to_tray_ = !close_to_tray_;
                else show_notifications_ = !show_notifications_;
                InvalidateRect(reinterpret_cast<HWND>(lParam), nullptr, TRUE);
            } else if (id == ID_BTN_REFRESH || id == tray::TrayIcon::ID_TRAY_REFRESH) {
                if (device_service_) {
                    device_service_->refresh_devices();
                }
                query_memory_stats();
                set_status_message(L"Devices refreshed successfully");
            }
            // DPI Controls
            else if (id >= ID_BTN_DPI_400 && id <= ID_BTN_DPI_6400) {
                int val = 800;
                if (id == ID_BTN_DPI_400) val = 400;
                else if (id == ID_BTN_DPI_800) val = 800;
                else if (id == ID_BTN_DPI_1200) val = 1200;
                else if (id == ID_BTN_DPI_1600) val = 1600;
                else if (id == ID_BTN_DPI_2400) val = 2400;
                else if (id == ID_BTN_DPI_3200) val = 3200;
                else if (id == ID_BTN_DPI_6400) val = 6400;

                staged_dpi_ = val;
                SetWindowTextW(edit_dpi_custom_, std::to_wstring(val).c_str());
                if (device_service_) {
                    auto res = device_service_->set_dpi(val);
                    set_status_message(res ? std::format(L"Sensor DPI set to {}", val) : L"Failed: " + infra::hid::WindowsHidEnumerator::utf8_to_wide(res.error().message), !res);
                }
            } else if (id == ID_BTN_DPI_MINUS) {
                staged_dpi_ = (std::max)(50, staged_dpi_ - 50);
                SetWindowTextW(edit_dpi_custom_, std::to_wstring(staged_dpi_).c_str());
                InvalidateRect(hwnd_, nullptr, TRUE);
            } else if (id == ID_BTN_DPI_PLUS) {
                staged_dpi_ = (std::min)(26000, staged_dpi_ + 50);
                SetWindowTextW(edit_dpi_custom_, std::to_wstring(staged_dpi_).c_str());
                InvalidateRect(hwnd_, nullptr, TRUE);
            } else if (id == ID_BTN_DPI_RESET) {
                staged_dpi_ = 800;
                sync_dpi_controls_from_staged();
                if (device_service_) {
                    auto res = device_service_->set_dpi(800);
                    set_status_message(res ? L"DPI reset to 800" : L"Failed to reset DPI", !res);
                }
                InvalidateRect(hwnd_, nullptr, TRUE);
            } else if (id == ID_BTN_DPI_APPLY) {
                wchar_t txt[32] = {0};
                GetWindowTextW(edit_dpi_custom_, txt, 31);
                try {
                    int custom = std::stoi(txt);
                    if (device_service_) {
                        auto res = device_service_->set_dpi(custom);
                        set_status_message(res ? std::format(L"Applied {} DPI to mouse sensor", custom) : L"Rejected: " + infra::hid::WindowsHidEnumerator::utf8_to_wide(res.error().message), !res);
                    }
                } catch (...) {
                    set_status_message(L"Invalid numeric DPI input", true);
                }
            }
            // Polling Rate Controls
            else if (id >= ID_BTN_RATE_125 && id <= ID_BTN_RATE_1000) {
                int rate = 1000;
                if (id == ID_BTN_RATE_125) rate = 125;
                else if (id == ID_BTN_RATE_250) rate = 250;
                else if (id == ID_BTN_RATE_500) rate = 500;
                else if (id == ID_BTN_RATE_1000) rate = 1000;

                if (device_service_) {
                    auto res = device_service_->set_polling_rate(rate);
                    set_status_message(res ? std::format(L"Hardware polling rate set to {} Hz", rate) : L"Failed: " + infra::hid::WindowsHidEnumerator::utf8_to_wide(res.error().message), !res);
                }
            }
            // Profile Controls
            else if (id == ID_BTN_PROF_SWITCH) {
                int sel = static_cast<int>(SendMessageW(list_profiles_, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(cached_profiles_.size())) {
                    const auto& target_p = cached_profiles_[sel];
                    if (device_service_) {
                        auto res = device_service_->set_polling_rate(target_p.polling_rate);
                        if (!target_p.dpi_stages.empty()) {
                            device_service_->set_dpi(target_p.dpi_stages[0].dpi);
                        }
                        set_status_message(std::format(L"Switched to profile: {}", infra::hid::WindowsHidEnumerator::utf8_to_wide(target_p.name)));
                    }
                    refresh_profile_list();
                }
            } else if (id == ID_BTN_PROF_NEW) {
                if (profile_service_) {
                    std::vector<core::domain::DpiStage> stages = {{0, 800, true, 0x00FF00}, {1, 1600, true, 0xFF0000}};
                    std::string p_name = "Profile " + std::to_string(cached_profiles_.size() + 1);
                    auto res = profile_service_->create_profile(p_name, "Ajazz AJ159", stages, 1000);
                    set_status_message(res ? L"Created new profile" : L"Failed to create profile", !res);
                    refresh_profile_list();
                }
            } else if (id == ID_BTN_PROF_DUPLICATE) {
                int sel = static_cast<int>(SendMessageW(list_profiles_, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(cached_profiles_.size())) {
                    if (profile_service_) {
                        const auto& p = cached_profiles_[sel];
                        auto res = profile_service_->create_profile(p.name + " (Copy)", p.device_identifier, p.dpi_stages, p.polling_rate);
                        set_status_message(res ? L"Profile duplicated" : L"Failed to duplicate profile", !res);
                        refresh_profile_list();
                    }
                }
            } else if (id == ID_BTN_PROF_DELETE) {
                int sel = static_cast<int>(SendMessageW(list_profiles_, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(cached_profiles_.size())) {
                    if (profile_service_) {
                        auto res = profile_service_->delete_profile(cached_profiles_[sel].id);
                        set_status_message(res ? L"Profile deleted" : L"Failed to delete profile", !res);
                        refresh_profile_list();
                    }
                }
            } else if (id == ID_BTN_PROF_EXPORT) {
                int sel = static_cast<int>(SendMessageW(list_profiles_, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(cached_profiles_.size())) {
                    auto exp_res = profile_service_->export_profile(cached_profiles_[sel].id);
                    if (exp_res) {
                        wchar_t file_path[MAX_PATH] = L"profile.json";
                        OPENFILENAMEW ofn{};
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = hwnd_;
                        ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
                        ofn.lpstrFile = file_path;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                        if (GetSaveFileNameW(&ofn)) {
                            std::ofstream out(file_path);
                            out << exp_res.value();
                            set_status_message(L"Exported profile to JSON successfully");
                        }
                    }
                }
            } else if (id == ID_BTN_PROF_IMPORT) {
                wchar_t file_path[MAX_PATH] = {0};
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd_;
                ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = file_path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    std::ifstream in(file_path);
                    if (in) {
                        std::stringstream ss;
                        ss << in.rdbuf();
                        auto imp_res = profile_service_->import_profile(ss.str());
                        set_status_message(imp_res ? L"Imported profile successfully" : L"Failed to import: " + infra::hid::WindowsHidEnumerator::utf8_to_wide(imp_res.error().message), !imp_res);
                        refresh_profile_list();
                    }
                }
            } else if (id == ID_BTN_SAVE_SETTINGS) {
                save_settings_from_ui();
            }
            // Tray menu commands
            else if (id == tray::TrayIcon::ID_TRAY_SHOW) {
                show_window_from_tray();
            } else if (id == tray::TrayIcon::ID_TRAY_EXIT) {
                exiting_ = true;
                DestroyWindow(hwnd_);
            } else if (id >= tray::TrayIcon::ID_TRAY_DPI_400 && id <= tray::TrayIcon::ID_TRAY_DPI_6400) {
                int val = 800;
                if (id == tray::TrayIcon::ID_TRAY_DPI_400) val = 400;
                else if (id == tray::TrayIcon::ID_TRAY_DPI_800) val = 800;
                else if (id == tray::TrayIcon::ID_TRAY_DPI_1200) val = 1200;
                else if (id == tray::TrayIcon::ID_TRAY_DPI_1600) val = 1600;
                else if (id == tray::TrayIcon::ID_TRAY_DPI_2400) val = 2400;
                else if (id == tray::TrayIcon::ID_TRAY_DPI_3200) val = 3200;
                else if (id == tray::TrayIcon::ID_TRAY_DPI_6400) val = 6400;

                staged_dpi_ = val;
                sync_dpi_controls_from_staged();
                if (device_service_) {
                    auto res = device_service_->set_dpi(val);
                    set_status_message(res ? std::format(L"Sensor DPI set to {}", val) : L"Failed: " + infra::hid::WindowsHidEnumerator::utf8_to_wide(res.error().message), !res);
                }
                if (show_notifications_ && tray_icon_) {
                    tray_icon_->show_notification(L"DPI Preset Selected", std::format(L"Sensor DPI set to {}", val));
                }
                InvalidateRect(hwnd_, nullptr, TRUE);
            } else if (id >= tray::TrayIcon::ID_TRAY_RATE_125 && id <= tray::TrayIcon::ID_TRAY_RATE_1000) {
                int rate = 1000;
                if (id == tray::TrayIcon::ID_TRAY_RATE_125) rate = 125;
                else if (id == tray::TrayIcon::ID_TRAY_RATE_250) rate = 250;
                else if (id == tray::TrayIcon::ID_TRAY_RATE_500) rate = 500;
                else if (id == tray::TrayIcon::ID_TRAY_RATE_1000) rate = 1000;

                if (device_service_) {
                    auto res = device_service_->set_polling_rate(rate);
                    set_status_message(res ? std::format(L"Hardware polling rate set to {} Hz", rate) : L"Failed: " + infra::hid::WindowsHidEnumerator::utf8_to_wide(res.error().message), !res);
                }
                if (show_notifications_ && tray_icon_) {
                    tray_icon_->show_notification(L"Polling Rate Selected", std::format(L"Hardware polling rate: {} Hz", rate));
                }
                InvalidateRect(hwnd_, nullptr, TRUE);
            }
            return 0;
        }

        case WM_HSCROLL: {
            if (reinterpret_cast<HWND>(lParam) == slider_dpi_) {
                int pos = static_cast<int>(SendMessageW(slider_dpi_, TBM_GETPOS, 0, 0));
                staged_dpi_ = (std::clamp)(pos * 50, 50, 26000);
                if (edit_dpi_custom_) {
                    SetWindowTextW(edit_dpi_custom_, std::to_wstring(staged_dpi_).c_str());
                }
                InvalidateRect(hwnd_, nullptr, TRUE);
                return 0;
            }
            break;
        }

        case WM_HOTKEY: {
            if (wParam == ID_HOTKEY_DPI_CYCLE) {
                cycle_dpi_stage();
                return 0;
            } else if (wParam == ID_HOTKEY_DPI_UP) {
                staged_dpi_ = (std::min)(26000, staged_dpi_ + 50);
                sync_dpi_controls_from_staged();
                if (device_service_) {
                    [[maybe_unused]] auto res = device_service_->set_dpi(staged_dpi_);
                }
                if (show_notifications_ && tray_icon_) {
                    tray_icon_->show_notification(L"DPI Hotkey (+50)", std::format(L"Sensor DPI: {}", staged_dpi_));
                }
                set_status_message(std::format(L"Hotkey: DPI increased to {}", staged_dpi_));
                InvalidateRect(hwnd_, nullptr, TRUE);
                return 0;
            } else if (wParam == ID_HOTKEY_DPI_DOWN) {
                staged_dpi_ = (std::max)(50, staged_dpi_ - 50);
                sync_dpi_controls_from_staged();
                if (device_service_) {
                    [[maybe_unused]] auto res = device_service_->set_dpi(staged_dpi_);
                }
                if (show_notifications_ && tray_icon_) {
                    tray_icon_->show_notification(L"DPI Hotkey (-50)", std::format(L"Sensor DPI: {}", staged_dpi_));
                }
                set_status_message(std::format(L"Hotkey: DPI decreased to {}", staged_dpi_));
                InvalidateRect(hwnd_, nullptr, TRUE);
                return 0;
            }
            break;
        }

        case tray::TrayIcon::WM_TRAYICON: {
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                int cur_dpi = staged_dpi_;
                int cur_rate = 1000;
                if (device_service_) {
                    auto act = device_service_->get_active_device();
                    if (act) {
                        cur_dpi = act->current_dpi();
                        cur_rate = act->current_polling_rate();
                    }
                }
                tray_icon_->show_menu(cur_dpi, cur_rate);
            } else if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                if (IsWindowVisible(hwnd_)) {
                    hide_window_to_tray();
                } else {
                    show_window_from_tray();
                }
            }
            return 0;
        }

        case WM_SYSCOMMAND: {
            if (wParam == SC_MINIMIZE && minimize_to_tray_) {
                hide_window_to_tray();
                return 0;
            }
            break;
        }

        case WM_CLOSE: {
            if (!exiting_ && close_to_tray_) {
                hide_window_to_tray();
                return 0;
            }
            exiting_ = true;
            DestroyWindow(hwnd_);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hwnd_, uMsg, wParam, lParam);
}

void MainWindow::on_paint(HDC hdc) {
    RECT client_rc;
    GetClientRect(hwnd_, &client_rc);
    int width = client_rc.right - client_rc.left;
    int height = client_rc.bottom - client_rc.top;

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP old_bmp = static_cast<HBITMAP>(SelectObject(mem_dc, mem_bmp));

    FillRect(mem_dc, &client_rc, bg_brush_);
    SetBkMode(mem_dc, TRANSPARENT);

    render_sidebar(mem_dc, height);

    RECT topbar{ 232, 0, width, 88 };
    FillRect(mem_dc, &topbar, header_brush_);
    SelectObject(mem_dc, border_pen_);
    MoveToEx(mem_dc, topbar.left, topbar.bottom - 1, nullptr);
    LineTo(mem_dc, topbar.right, topbar.bottom - 1);
    draw_text(mem_dc, font_mono_, kMuted, L"DEVICE CONTROL  /  WINDOWS", RECT{ 260, 28, 520, 52 });

    const int page_x = 260;
    const int page_y = 116;
    const int page_w = (std::max)(320, width - page_x - 32);
    const int page_h = (std::max)(260, height - page_y - 24);

    render_workspace(mem_dc, page_x, page_y, page_w, page_h);

    BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

void MainWindow::render_sidebar(HDC mem_dc, int height) {
    RECT sb_rc{ 0, 0, 232, height };
    FillRect(mem_dc, &sb_rc, sidebar_brush_);

    SelectObject(mem_dc, border_pen_);
    MoveToEx(mem_dc, 231, 0, nullptr);
    LineTo(mem_dc, 231, height);

    RECT mark{ 22, 24, 58, 60 };
    HBRUSH mark_brush = CreateSolidBrush(kAccent);
    HGDIOBJ old_mark_brush = SelectObject(mem_dc, mark_brush);
    HGDIOBJ old_mark_pen = SelectObject(mem_dc, GetStockObject(NULL_PEN));
    RoundRect(mem_dc, mark.left, mark.top, mark.right, mark.bottom, 10, 10);
    SelectObject(mem_dc, old_mark_brush);
    SelectObject(mem_dc, old_mark_pen);
    DeleteObject(mark_brush);
    draw_text(mem_dc, font_bold_, kCanvas, L"M", RECT{ 22, 31, 58, 55 }, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    draw_text(mem_dc, font_bold_, kText, L"MouseControl", RECT{ 70, 25, 218, 48 });
    draw_text(mem_dc, font_mono_, kMuted, L"PRECISION SUITE  /  1.0", RECT{ 70, 47, 220, 65 });
    draw_text(mem_dc, font_mono_, kMuted, L"YOUR DEVICES", RECT{ 22, 101, 210, 121 });

    const wchar_t* tab_names[] = {
        L"Dashboard",
        L"DPI tuning",
        L"Polling rate",
        L"Profiles",
        L"Device details",
        L"Preferences"
    };

    int tab_y = 132;
    for (int i = 0; i < 6; ++i) {
        bool is_active = (static_cast<int>(active_tab_) == i);
        RECT item_rc{ 12, tab_y, 220, tab_y + 42 };

        if (is_active) {
            draw_panel(mem_dc, item_rc, card_active_brush_, border_pen_, 9);
            RECT marker{ item_rc.left, item_rc.top + 10, item_rc.left + 3, item_rc.bottom - 10 };
            HBRUSH marker_brush = CreateSolidBrush(kAccent);
            FillRect(mem_dc, &marker, marker_brush);
            DeleteObject(marker_brush);
            draw_text(mem_dc, font_bold_, kAccent, tab_names[i], RECT{ 30, tab_y + 10, 210, tab_y + 34 });
        } else {
            draw_text(mem_dc, font_body_, kMuted, tab_names[i], RECT{ 30, tab_y + 10, 210, tab_y + 34 });
        }
        tab_y += 50;
    }

    query_memory_stats();
    RECT mem_rc{ 14, height - 94, 218, height - 14 };
    draw_panel(mem_dc, mem_rc, card_brush_, border_pen_, 10);
    std::wstring mem_str = std::format(L"RAM: {:.1f} MB", ram_working_set_kb_ / 1024.0f);
    draw_text(mem_dc, font_mono_, kAccent, mem_str.c_str(), RECT{ 26, mem_rc.top + 13, 208, mem_rc.top + 32 });
    draw_text(mem_dc, font_mono_, kMuted, L"BACKGROUND: IDLE", RECT{ 26, mem_rc.top + 44, 208, mem_rc.top + 64 });
}

void MainWindow::render_page_dashboard(HDC mem_dc, int x, int y, int width, [[maybe_unused]] int height) {
    // Title
    RECT t_rc{ x, y, x + width - 180, y + 30 };
    SelectObject(mem_dc, font_header_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Dashboard \u2014 Device Overview", -1, &t_rc, DT_LEFT | DT_TOP);

    auto active_dev = device_service_ ? device_service_->get_active_device() : std::nullopt;

    // 1. Hero Device Card
    int card_top = y + 45;
    RECT card_rc{ x, card_top, x + width, card_top + 160 };
    SelectObject(mem_dc, card_brush_);
    SelectObject(mem_dc, border_pen_);
    RoundRect(mem_dc, card_rc.left, card_rc.top, card_rc.right, card_rc.bottom, 10, 10);

    if (active_dev) {
        const auto& dev = active_dev.value();
        bool is_ajazz = (dev.adapter_name() == "AjazzAdapter");

        // Status badge
        SelectObject(mem_dc, font_bold_);
        SetTextColor(mem_dc, is_ajazz ? RGB(63, 185, 80) : RGB(88, 166, 255));
        RECT badge_rc{ card_rc.left + 20, card_rc.top + 16, card_rc.right - 20, card_rc.top + 38 };
        std::wstring title = L"\u25CF " + infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.name());
        DrawTextW(mem_dc, title.c_str(), -1, &badge_rc, DT_LEFT | DT_TOP);

        SelectObject(mem_dc, font_body_);
        SetTextColor(mem_dc, RGB(139, 148, 158));

        std::wstring line1 = std::format(L"Manufacturer: {}    |    Connection: {}    |    Adapter: {}",
            infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.manufacturer()),
            infra::hid::WindowsHidEnumerator::utf8_to_wide(std::string(core::connection_type_to_string(dev.connection_type()))),
            infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.adapter_name()));
        RECT l1_rc{ card_rc.left + 20, card_rc.top + 46, card_rc.right - 20, card_rc.top + 66 };
        DrawTextW(mem_dc, line1.c_str(), -1, &l1_rc, DT_LEFT | DT_TOP);

        // Telemetry stats row
        RECT stat1{ card_rc.left + 20, card_rc.top + 80, card_rc.left + 200, card_rc.top + 140 };
        SetTextColor(mem_dc, RGB(240, 246, 252));
        SelectObject(mem_dc, font_large_stat_);
        std::wstring dpi_val = std::format(L"{}", dev.current_dpi());
        DrawTextW(mem_dc, dpi_val.c_str(), -1, &stat1, DT_LEFT | DT_TOP);

        RECT lbl1{ card_rc.left + 20, card_rc.top + 122, card_rc.left + 200, card_rc.top + 140 };
        SelectObject(mem_dc, font_mono_);
        SetTextColor(mem_dc, RGB(139, 148, 158));
        DrawTextW(mem_dc, L"ACTIVE SENSOR DPI", -1, &lbl1, DT_LEFT | DT_TOP);

        RECT stat2{ card_rc.left + 220, card_rc.top + 80, card_rc.left + 420, card_rc.top + 140 };
        SelectObject(mem_dc, font_large_stat_);
        SetTextColor(mem_dc, RGB(240, 246, 252));
        std::wstring rate_val = std::format(L"{} Hz", dev.current_polling_rate());
        DrawTextW(mem_dc, rate_val.c_str(), -1, &stat2, DT_LEFT | DT_TOP);

        RECT lbl2{ card_rc.left + 220, card_rc.top + 122, card_rc.left + 420, card_rc.top + 140 };
        SelectObject(mem_dc, font_mono_);
        SetTextColor(mem_dc, RGB(139, 148, 158));
        DrawTextW(mem_dc, L"REPORT RATE", -1, &lbl2, DT_LEFT | DT_TOP);

        // Battery
        RECT stat3{ card_rc.left + 440, card_rc.top + 80, card_rc.left + 640, card_rc.top + 140 };
        SelectObject(mem_dc, font_large_stat_);
        if (dev.battery_level() >= 0) {
            SetTextColor(mem_dc, RGB(63, 185, 80));
            std::wstring bat_val = std::format(L"{}%", dev.battery_level());
            DrawTextW(mem_dc, bat_val.c_str(), -1, &stat3, DT_LEFT | DT_TOP);
        } else {
            SetTextColor(mem_dc, RGB(139, 148, 158));
            DrawTextW(mem_dc, L"N/A", -1, &stat3, DT_LEFT | DT_TOP);
        }

        RECT lbl3{ card_rc.left + 440, card_rc.top + 122, card_rc.left + 640, card_rc.top + 140 };
        SelectObject(mem_dc, font_mono_);
        SetTextColor(mem_dc, RGB(139, 148, 158));
        DrawTextW(mem_dc, L"BATTERY TELEMETRY", -1, &lbl3, DT_LEFT | DT_TOP);
    } else {
        SelectObject(mem_dc, font_bold_);
        SetTextColor(mem_dc, RGB(210, 153, 34)); // Amber
        RECT msg{ card_rc.left + 20, card_rc.top + 20, card_rc.right - 20, card_rc.top + 50 };
        DrawTextW(mem_dc, L"No Supported Gaming Mouse Currently Active", -1, &msg, DT_LEFT | DT_TOP);
    }

    // 2. Status message banner
    int status_top = card_top + 175;
    RECT sb_box{ x, status_top, x + width, status_top + 40 };
    SelectObject(mem_dc, card_brush_);
    SelectObject(mem_dc, border_pen_);
    RoundRect(mem_dc, sb_box.left, sb_box.top, sb_box.right, sb_box.bottom, 6, 6);

    SelectObject(mem_dc, font_body_);
    SetTextColor(mem_dc, status_is_error_ ? RGB(248, 81, 73) : RGB(63, 185, 80));
    RECT sb_txt{ sb_box.left + 16, sb_box.top + 10, sb_box.right - 16, sb_box.bottom - 8 };
    DrawTextW(mem_dc, (L"Status: " + status_message_).c_str(), -1, &sb_txt, DT_LEFT | DT_TOP);

    // 3. Quick Stage Overview Grid
    int grid_top = status_top + 55;
    SelectObject(mem_dc, font_bold_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    RECT grid_lbl{ x, grid_top, x + width, grid_top + 22 };
    DrawTextW(mem_dc, L"Configured DPI Stages (Ajazz AJ159 On-Board Memory)", -1, &grid_lbl, DT_LEFT | DT_TOP);

    int box_y = grid_top + 30;
    int box_w = (width - 50) / 6;
    int default_dpi_stages[] = { 400, 800, 1200, 1600, 2400, 3200 };

    for (int i = 0; i < 6; ++i) {
        int cur_dpi = (active_dev && active_dev->current_dpi() > 0) ? active_dev->current_dpi() : 800;
        bool is_cur = (cur_dpi == default_dpi_stages[i]);

        RECT s_box{ x + i * (box_w + 10), box_y, x + i * (box_w + 10) + box_w, box_y + 80 };
        SelectObject(mem_dc, is_cur ? card_active_brush_ : card_brush_);
        SelectObject(mem_dc, is_cur ? green_pen_ : border_pen_);
        RoundRect(mem_dc, s_box.left, s_box.top, s_box.right, s_box.bottom, 8, 8);

        SelectObject(mem_dc, font_mono_);
        SetTextColor(mem_dc, is_cur ? RGB(63, 185, 80) : RGB(139, 148, 158));
        std::wstring stg_lbl = std::format(L"STAGE {}", i + 1);
        RECT sl{ s_box.left + 8, s_box.top + 10, s_box.right - 8, s_box.top + 28 };
        DrawTextW(mem_dc, stg_lbl.c_str(), -1, &sl, DT_CENTER | DT_TOP);

        SelectObject(mem_dc, font_bold_);
        SetTextColor(mem_dc, RGB(240, 246, 252));
        std::wstring dpi_num = std::format(L"{}", default_dpi_stages[i]);
        RECT sn{ s_box.left + 8, s_box.top + 36, s_box.right - 8, s_box.bottom - 10 };
        DrawTextW(mem_dc, dpi_num.c_str(), -1, &sn, DT_CENTER | DT_TOP);
    }
}

void MainWindow::render_page_dpi(HDC mem_dc, int x, int y, int width, [[maybe_unused]] int height) {
    RECT t_rc{ x, y, x + width, y + 30 };
    SelectObject(mem_dc, font_header_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"DPI Sensitivity Management", -1, &t_rc, DT_LEFT | DT_TOP);

    auto active_dev = device_service_ ? device_service_->get_active_device() : std::nullopt;
    int cur_dpi = active_dev ? active_dev->current_dpi() : 800;

    // Active Sensor DPI Card
    int card_top = y + 45;
    RECT card_rc{ x, card_top, x + width, card_top + 140 };
    SelectObject(mem_dc, card_brush_);
    SelectObject(mem_dc, border_pen_);
    RoundRect(mem_dc, card_rc.left, card_rc.top, card_rc.right, card_rc.bottom, 10, 10);

    RECT lbl_rc{ card_rc.left + 24, card_rc.top + 18, card_rc.left + 300, card_rc.top + 40 };
    SelectObject(mem_dc, font_mono_);
    SetTextColor(mem_dc, RGB(139, 148, 158));
    DrawTextW(mem_dc, L"CURRENT SENSOR RESOLUTION", -1, &lbl_rc, DT_LEFT | DT_TOP);

    RECT stat_rc{ card_rc.left + 24, card_rc.top + 46, card_rc.left + 300, card_rc.top + 95 };
    SelectObject(mem_dc, font_large_stat_);
    SetTextColor(mem_dc, RGB(88, 166, 255));
    std::wstring dpi_txt = std::format(L"{} DPI", cur_dpi);
    DrawTextW(mem_dc, dpi_txt.c_str(), -1, &stat_rc, DT_LEFT | DT_TOP);

    RECT bounds_rc{ card_rc.left + 24, card_rc.top + 104, card_rc.right - 24, card_rc.bottom - 10 };
    SelectObject(mem_dc, font_body_);
    SetTextColor(mem_dc, RGB(139, 148, 158));
    DrawTextW(mem_dc, L"Supported Hardware Range: 50 \u2014 26,000 DPI  |  Adjustment Step Size: 50 DPI", -1, &bounds_rc, DT_LEFT | DT_TOP);

    // Section 2: Quick Presets Label
    int preset_lbl_y = card_top + 160;
    RECT p_lbl{ x, preset_lbl_y, x + width, preset_lbl_y + 24 };
    SelectObject(mem_dc, font_bold_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Hardware DPI Presets (Click to dispatch to sensor):", -1, &p_lbl, DT_LEFT | DT_TOP);

    // Section 3: Fine Adjustment Stepper Label
    int step_lbl_y = preset_lbl_y + 80;
    RECT s_lbl{ x, step_lbl_y, x + width, step_lbl_y + 24 };
    DrawTextW(mem_dc, L"Fine Numeric Adjustment:", -1, &s_lbl, DT_LEFT | DT_TOP);
}

void MainWindow::render_page_polling_rate(HDC mem_dc, int x, int y, int width, [[maybe_unused]] int height) {
    RECT t_rc{ x, y, x + width, y + 30 };
    SelectObject(mem_dc, font_header_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Polling Rate & Report Frequency", -1, &t_rc, DT_LEFT | DT_TOP);

    auto active_dev = device_service_ ? device_service_->get_active_device() : std::nullopt;
    int cur_rate = active_dev ? active_dev->current_polling_rate() : 1000;

    int card_top = y + 45;
    RECT card_rc{ x, card_top, x + width, card_top + 100 };
    SelectObject(mem_dc, card_brush_);
    SelectObject(mem_dc, border_pen_);
    RoundRect(mem_dc, card_rc.left, card_rc.top, card_rc.right, card_rc.bottom, 10, 10);

    RECT lbl_rc{ card_rc.left + 24, card_rc.top + 16, card_rc.left + 400, card_rc.top + 36 };
    SelectObject(mem_dc, font_mono_);
    SetTextColor(mem_dc, RGB(139, 148, 158));
    DrawTextW(mem_dc, L"ACTIVE USB / WIRELESS REPORT RATE", -1, &lbl_rc, DT_LEFT | DT_TOP);

    RECT stat_rc{ card_rc.left + 24, card_rc.top + 40, card_rc.left + 400, card_rc.bottom - 10 };
    SelectObject(mem_dc, font_large_stat_);
    SetTextColor(mem_dc, RGB(63, 185, 80));
    std::wstring rate_txt = std::format(L"{} Hz ({:.1f} ms delay)", cur_rate, 1000.0f / cur_rate);
    DrawTextW(mem_dc, rate_txt.c_str(), -1, &stat_rc, DT_LEFT | DT_TOP);

    int opts_lbl_y = card_top + 120;
    RECT o_lbl{ x, opts_lbl_y, x + width, opts_lbl_y + 24 };
    SelectObject(mem_dc, font_bold_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Available Hardware Rates (Click to configure):", -1, &o_lbl, DT_LEFT | DT_TOP);
}

void MainWindow::render_page_profiles(HDC mem_dc, int x, int y, int width, [[maybe_unused]] int height) {
    RECT t_rc{ x, y, x + width, y + 30 };
    SelectObject(mem_dc, font_header_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Device Profiles & SQLite Storage", -1, &t_rc, DT_LEFT | DT_TOP);

    RECT desc_rc{ x, y + 32, x + width, y + 54 };
    SelectObject(mem_dc, font_body_);
    SetTextColor(mem_dc, RGB(139, 148, 158));
    DrawTextW(mem_dc, L"Profiles persist locally in SQLite database (mousecontrol.db) with JSON import/export support.", -1, &desc_rc, DT_LEFT | DT_TOP);

    RECT list_lbl{ x, y + 65, x + 350, y + 85 };
    SelectObject(mem_dc, font_bold_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Saved Profiles:", -1, &list_lbl, DT_LEFT | DT_TOP);
}

void MainWindow::render_page_device_info(HDC mem_dc, int x, int y, int width, [[maybe_unused]] int height) {
    RECT t_rc{ x, y, x + width, y + 30 };
    SelectObject(mem_dc, font_header_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Device Information & HID Interface Inspector", -1, &t_rc, DT_LEFT | DT_TOP);

    // Table of devices
    int tbl_top = y + 45;
    RECT tbl_box{ x, tbl_top, x + width, y + height };
    SelectObject(mem_dc, card_brush_);
    SelectObject(mem_dc, border_pen_);
    RoundRect(mem_dc, tbl_box.left, tbl_box.top, tbl_box.right, tbl_box.bottom, 8, 8);

    int row_y = tbl_box.top + 14;
    SelectObject(mem_dc, font_mono_);

    for (size_t i = 0; i < devices_.size() && row_y + 45 < tbl_box.bottom; ++i) {
        const auto& dev = devices_[i];
        bool is_aj = (dev.adapter_name() == "AjazzAdapter");

        RECT item_rc{ tbl_box.left + 16, row_y, tbl_box.right - 16, row_y + 40 };
        SetTextColor(mem_dc, is_aj ? RGB(63, 185, 80) : RGB(201, 209, 217));

        std::wstring line1 = std::format(L"[{}] {} \u2014 {} ({})",
            is_aj ? L"AJAZZ" : L"GENERIC",
            infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.name()),
            infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.manufacturer()),
            infra::hid::WindowsHidEnumerator::utf8_to_wide(std::string(core::connection_type_to_string(dev.connection_type()))));
        DrawTextW(mem_dc, line1.c_str(), -1, &item_rc, DT_LEFT | DT_TOP);

        SetTextColor(mem_dc, RGB(139, 148, 158));
        std::wstring line2 = std::format(L"    Interfaces: {} | Adapter: {}",
            dev.interfaces().size(),
            infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.adapter_name()));

        if (!dev.interfaces().empty()) {
            const auto& iface = dev.interfaces().front();
            line2 += std::format(L" | VID: 0x{:04X} PID: 0x{:04X} | UsagePage: 0x{:04X} Usage: 0x{:04X}",
                iface.vendor_id, iface.product_id, iface.usage_page, iface.usage);
        }

        RECT item_rc2{ tbl_box.left + 16, row_y + 18, tbl_box.right - 16, row_y + 36 };
        DrawTextW(mem_dc, line2.c_str(), -1, &item_rc2, DT_LEFT | DT_TOP);

        row_y += 42;
    }
}

void MainWindow::render_page_settings(HDC mem_dc, int x, int y, int width, [[maybe_unused]] int height) {
    RECT t_rc{ x, y, x + width, y + 30 };
    SelectObject(mem_dc, font_header_);
    SetTextColor(mem_dc, RGB(240, 246, 252));
    DrawTextW(mem_dc, L"Application & System Integration Preferences", -1, &t_rc, DT_LEFT | DT_TOP);

    RECT desc_rc{ x, y + 32, x + width, y + 54 };
    SelectObject(mem_dc, font_body_);
    SetTextColor(mem_dc, RGB(139, 148, 158));
    DrawTextW(mem_dc, L"Configure Windows background behavior, tray minimization, and notification options.", -1, &desc_rc, DT_LEFT | DT_TOP);

    int card_top = y + 70;
    RECT card_rc{ x, card_top, x + width, card_top + 280 };
    SelectObject(mem_dc, card_brush_);
    SelectObject(mem_dc, border_pen_);
    RoundRect(mem_dc, card_rc.left, card_rc.top, card_rc.right, card_rc.bottom, 10, 10);
}

void MainWindow::render_workspace(HDC dc, int x, int y, int width, int height) {
    const wchar_t* titles[] = {
        L"Overview", L"DPI tuning", L"Polling rate", L"Profiles", L"Device details", L"Preferences"
    };
    const wchar_t* subtitles[] = {
        L"Your mouse, tuned to your play style.",
        L"Find the sensitivity that feels right.",
        L"Choose how often your mouse reports movement.",
        L"Save, switch and share your setups.",
        L"Hardware identity and connection diagnostics.",
        L"Make MouseControl work the way you do."
    };
    const size_t tab = static_cast<size_t>(active_tab_);
    draw_text(dc, font_header_, kText, titles[tab], RECT{ x, y, x + width, y + 38 });
    draw_text(dc, font_body_, kMuted, subtitles[tab], RECT{ x, y + 42, x + width, y + 64 });

    auto active = device_service_ ? device_service_->get_active_device() : std::nullopt;
    auto panel = [&](RECT rect, bool raised = false) {
        draw_panel(dc, rect, raised ? card_active_brush_ : card_brush_, border_pen_);
    };

    if (active_tab_ == NavTab::Dashboard) {
        const int top = y + 82;
        panel(RECT{ x, top, x + width, top + 224 });
        RECT accent{ x + 1, top + 18, x + 4, top + 206 };
        HBRUSH accent_brush = CreateSolidBrush(kAccent);
        FillRect(dc, &accent, accent_brush);
        DeleteObject(accent_brush);

        if (active) {
            const auto& dev = *active;
            const bool supported = dev.adapter_name() == "AjazzAdapter";
            draw_text(dc, font_bold_, kText, infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.name()).c_str(),
                RECT{ x + 24, top + 18, x + width - 24, top + 46 });
            draw_text(dc, font_body_, supported ? kGreen : kAccent, L"●  CONNECTED",
                RECT{ x + 24, top + 50, x + 180, top + 72 });
            const std::wstring details = std::format(L"{}   /   {}   /   {}",
                infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.manufacturer()),
                infra::hid::WindowsHidEnumerator::utf8_to_wide(std::string(core::connection_type_to_string(dev.connection_type()))),
                infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.adapter_name()));
            draw_text(dc, font_body_, kMuted, details.c_str(),
                RECT{ x + 24, top + 76, x + width - 24, top + 98 }, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            const int gap = 14;
            const int metric_width = (width - 48 - gap * 2) / 3;
            const std::array<std::wstring, 3> values{
                std::format(L"{}", dev.current_dpi()),
                std::format(L"{} Hz", dev.current_polling_rate()),
                dev.battery_level() >= 0 ? std::format(L"{}%", dev.battery_level()) : L"N/A"
            };
            const std::array<const wchar_t*, 3> labels{ L"SENSOR RESOLUTION", L"REPORT FREQUENCY", L"BATTERY LEVEL" };
            for (int i = 0; i < 3; ++i) {
                const int left = x + 24 + i * (metric_width + gap);
                const int metric_top = top + 116;
                panel(RECT{ left, metric_top, left + metric_width, metric_top + 88 }, true);
                draw_text(dc, font_mono_, kMuted, labels[static_cast<size_t>(i)],
                    RECT{ left + 14, metric_top + 12, left + metric_width - 8, metric_top + 30 });
                draw_text(dc, font_large_stat_, i == 2 ? kGreen : kText, values[static_cast<size_t>(i)].c_str(),
                    RECT{ left + 14, metric_top + 35, left + metric_width - 8, metric_top + 80 });
            }
        } else {
            draw_text(dc, font_bold_, kText, L"No supported mouse connected",
                RECT{ x + 26, top + 30, x + width - 24, top + 62 });
            draw_text(dc, font_body_, kMuted, L"Connect your mouse, then scan for devices to begin configuring it.",
                RECT{ x + 26, top + 70, x + width - 24, top + 96 });
        }

        const int status_top = top + 240;
        panel(RECT{ x, status_top, x + width, status_top + 48 });
        draw_text(dc, font_body_, status_is_error_ ? kRed : kGreen, (L"●  " + status_message_).c_str(),
            RECT{ x + 16, status_top + 14, x + width - 16, status_top + 36 },
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const int section_top = status_top + 76;
        draw_text(dc, font_bold_, kText, L"DPI stages", RECT{ x, section_top, x + width, section_top + 24 });
        draw_text(dc, font_body_, kMuted, L"Quick view of your sensor sensitivity presets.",
            RECT{ x, section_top + 25, x + width, section_top + 47 });
        const std::array<int, 6> defaults{ 400, 800, 1200, 1600, 2400, 3200 };
        const std::vector<core::domain::DpiStage> stages = active ? active->dpi_stages() : std::vector<core::domain::DpiStage>{};
        const int count = stages.empty() ? 6 : static_cast<int>((std::min)(stages.size(), defaults.size()));
        const int gap = 12;
        const int box_width = (width - gap * (count - 1)) / count;
        const int box_top = section_top + 58;
        const int current_dpi = active ? active->current_dpi() : staged_dpi_;
        for (int i = 0; i < count; ++i) {
            const int dpi = stages.empty() ? defaults[static_cast<size_t>(i)] : stages[static_cast<size_t>(i)].dpi;
            const bool current = dpi == current_dpi;
            RECT stage_box{ x + i * (box_width + gap), box_top, x + i * (box_width + gap) + box_width, box_top + 92 };
            draw_panel(dc, stage_box, current ? card_active_brush_ : card_brush_, current ? accent_pen_ : border_pen_, 12);
            draw_text(dc, font_mono_, current ? kAccent : kMuted, std::format(L"STAGE 0{}", i + 1).c_str(),
                RECT{ stage_box.left + 8, stage_box.top + 14, stage_box.right - 8, stage_box.top + 34 }, DT_CENTER | DT_TOP);
            draw_text(dc, font_bold_, kText, std::format(L"{} DPI", dpi).c_str(),
                RECT{ stage_box.left + 8, stage_box.top + 46, stage_box.right - 8, stage_box.top + 70 }, DT_CENTER | DT_TOP);
        }
        return;
    }

    if (active_tab_ == NavTab::Dpi) {
        auto dpi = active ? active->current_dpi() : staged_dpi_;
        const int top = y + 78;
        panel(RECT{ x, top, x + width, top + 190 });
        draw_text(dc, font_mono_, kMuted, L"CURRENT SENSOR RESOLUTION", RECT{ x + 22, top + 20, x + 420, top + 42 });
        draw_text(dc, font_large_stat_, kAccent, std::format(L"{} DPI", dpi).c_str(),
            RECT{ x + 22, top + 48, x + 360, top + 100 });
        draw_text(dc, font_body_, kMuted, L"50 — 26,000 DPI     •     50 DPI increments",
            RECT{ x + 24, top + 108, x + width - 24, top + 130 });

        RECT meter_bg{ x + 24, top + 151, x + width - 24, top + 159 };
        HBRUSH meter_brush = CreateSolidBrush(kBorder);
        FillRect(dc, &meter_bg, meter_brush);
        DeleteObject(meter_brush);
        const int meter_width = static_cast<int>((static_cast<double>((std::clamp)(dpi, 50, 26000) - 50) / 25950.0) * (width - 48));
        RECT meter{ meter_bg.left, meter_bg.top, meter_bg.left + (std::max)(8, meter_width), meter_bg.bottom };
        HBRUSH fill_brush = CreateSolidBrush(kAccent);
        FillRect(dc, &meter, fill_brush);
        DeleteObject(fill_brush);

        draw_text(dc, font_bold_, kText, L"Quick presets", RECT{ x, y + 278, x + width, y + 302 });
        draw_text(dc, font_body_, kMuted, L"Choose a starting point, then fine-tune below.",
            RECT{ x, y + 304, x + width, y + 326 });
        panel(RECT{ x, y + 392, x + width, y + 548 });
        draw_text(dc, font_bold_, kText, L"Fine adjustment", RECT{ x + 20, y + 408, x + width - 20, y + 432 });
        draw_text(dc, font_body_, kMuted, L"Adjust in 50 DPI steps or enter a custom sensor value.",
            RECT{ x + 20, y + 434, x + width - 20, y + 456 });
        return;
    }

    if (active_tab_ == NavTab::PollingRate) {
        const int top = y + 78;
        panel(RECT{ x, top, x + width, top + 188 });
        const int rate = active ? active->current_polling_rate() : 1000;
        draw_text(dc, font_mono_, kMuted, L"ACTIVE REPORT FREQUENCY", RECT{ x + 22, top + 22, x + 400, top + 44 });
        draw_text(dc, font_large_stat_, kAccent, std::format(L"{} Hz", rate).c_str(),
            RECT{ x + 22, top + 50, x + 400, top + 104 });
        draw_text(dc, font_body_, kMuted, std::format(L"One report every {:.1f} ms", 1000.0 / (std::max)(1, rate)).c_str(),
            RECT{ x + 24, top + 112, x + width - 24, top + 136 });

        draw_text(dc, font_bold_, kText, L"Polling presets", RECT{ x, y + 278, x + width, y + 302 });
        draw_text(dc, font_body_, kMuted, L"Higher rates can provide more responsive tracking.",
            RECT{ x, y + 304, x + width, y + 326 });
        const std::array<int, 4> rates{ 125, 250, 500, 1000 };
        for (size_t i = 0; i < rates.size(); ++i) {
            const int left = x + static_cast<int>(i) * (width / 4 + 4);
            RECT item{ left, y + 318, left + width / 4 - 10, y + 404 };
            panel(item, rates[i] == rate);
            draw_text(dc, font_bold_, rates[i] == rate ? kAccent : kText,
                std::format(L"{} Hz", rates[i]).c_str(), RECT{ item.left + 10, item.top + 20, item.right - 10, item.top + 48 }, DT_CENTER | DT_TOP);
            draw_text(dc, font_mono_, kMuted, std::format(L"{} ms", 1000 / rates[i]).c_str(),
                RECT{ item.left + 10, item.top + 52, item.right - 10, item.top + 70 }, DT_CENTER | DT_TOP);
        }
        return;
    }

    if (active_tab_ == NavTab::Profiles) {
        draw_text(dc, font_bold_, kText, L"Saved profiles", RECT{ x + 18, y + 96, x + width - 18, y + 122 });
        draw_text(dc, font_body_, kMuted, L"Stored locally • import and export as JSON",
            RECT{ x + 18, y + 124, x + width - 18, y + 146 });
        panel(RECT{ x, y + 78, x + width, y + height - 12 });
        return;
    }

    if (active_tab_ == NavTab::DeviceInfo) {
        const int top = y + 78;
        panel(RECT{ x, top, x + width, y + height - 12 });
        draw_text(dc, font_mono_, kMuted, L"DEVICE  /  CONNECTION  /  HARDWARE IDS",
            RECT{ x + 20, top + 18, x + width - 20, top + 40 });
        if (devices_.empty()) {
            draw_text(dc, font_bold_, kText, L"No devices discovered",
                RECT{ x + 20, top + 64, x + width - 20, top + 90 });
            draw_text(dc, font_body_, kMuted, L"Use Scan devices to inspect connected mouse hardware.",
                RECT{ x + 20, top + 96, x + width - 20, top + 120 });
            return;
        }
        int row_y = top + 58;
        for (const auto& dev : devices_) {
            if (row_y + 88 > y + height - 24) break;
            const bool supported = dev.adapter_name() == "AjazzAdapter";
            const auto details = std::format(L"{}   /   {}   /   {}",
                infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.manufacturer()),
                infra::hid::WindowsHidEnumerator::utf8_to_wide(std::string(core::connection_type_to_string(dev.connection_type()))),
                infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.adapter_name()));
            draw_text(dc, font_bold_, supported ? kGreen : kText,
                infra::hid::WindowsHidEnumerator::utf8_to_wide(dev.name()).c_str(),
                RECT{ x + 20, row_y, x + width - 20, row_y + 24 }, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
            draw_text(dc, font_body_, kMuted, details.c_str(),
                RECT{ x + 20, row_y + 26, x + width - 20, row_y + 48 }, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (!dev.interfaces().empty()) {
                const auto& iface = dev.interfaces().front();
                draw_text(dc, font_mono_, kMuted,
                    std::format(L"VID {:04X}   PID {:04X}   USAGE PAGE {:04X}   USAGE {:04X}",
                        iface.vendor_id, iface.product_id, iface.usage_page, iface.usage).c_str(),
                    RECT{ x + 20, row_y + 52, x + width - 20, row_y + 72 }, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            SelectObject(dc, border_pen_);
            MoveToEx(dc, x + 20, row_y + 82, nullptr);
            LineTo(dc, x + width - 20, row_y + 82);
            row_y += 100;
        }
        return;
    }

    if (active_tab_ == NavTab::Settings) {
        const int top = y + 78;
        panel(RECT{ x, top, x + width, top + 454 });
        draw_text(dc, font_bold_, kText, L"Startup & background behavior",
            RECT{ x + 22, top + 20, x + width - 22, top + 46 });
        draw_text(dc, font_body_, kMuted, L"Choose when the app launches and how it behaves in the system tray.",
            RECT{ x + 22, top + 48, x + width - 22, top + 72 });
        SelectObject(dc, border_pen_);
        MoveToEx(dc, x + 22, top + 98, nullptr);
        LineTo(dc, x + width - 22, top + 98);
        draw_text(dc, font_mono_, kMuted, L"APPLICATION", RECT{ x + 22, top + 112, x + width - 22, top + 132 });
    }
}

} // namespace mc::presentation::windows
