#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "application/services/device_service.hpp"
#include "application/services/profile_service.hpp"
#include "core/domain/interfaces/i_settings_repository.hpp"
#include "presentation/system_tray/tray_icon.hpp"

namespace mc::presentation::windows {

enum class NavTab : uint8_t {
    Dashboard = 0,
    Dpi,
    PollingRate,
    Profiles,
    DeviceInfo,
    Settings
};

class MainWindow {
public:
    static constexpr const wchar_t* CLASS_NAME = L"MouseControl_MainWindowClass";

    // Control IDs
    static constexpr UINT ID_BTN_REFRESH         = 1001;

    // DPI Controls
    static constexpr UINT ID_BTN_DPI_400         = 2001;
    static constexpr UINT ID_BTN_DPI_800         = 2002;
    static constexpr UINT ID_BTN_DPI_1200        = 2003;
    static constexpr UINT ID_BTN_DPI_1600        = 2004;
    static constexpr UINT ID_BTN_DPI_2400        = 2005;
    static constexpr UINT ID_BTN_DPI_3200        = 2006;
    static constexpr UINT ID_BTN_DPI_6400        = 2007;
    static constexpr UINT ID_SLIDER_DPI          = 2008;
    static constexpr UINT ID_BTN_DPI_RESET       = 2009;
    static constexpr UINT ID_BTN_DPI_MINUS       = 2010;
    static constexpr UINT ID_BTN_DPI_PLUS        = 2011;
    static constexpr UINT ID_BTN_DPI_APPLY       = 2012;
    static constexpr UINT ID_EDIT_DPI_CUSTOM     = 2013;

    // Polling Rate Controls
    static constexpr UINT ID_BTN_RATE_125        = 2101;
    static constexpr UINT ID_BTN_RATE_250        = 2102;
    static constexpr UINT ID_BTN_RATE_500        = 2103;
    static constexpr UINT ID_BTN_RATE_1000       = 2104;

    // Profile Controls
    static constexpr UINT ID_BTN_PROF_SWITCH     = 2201;
    static constexpr UINT ID_BTN_PROF_NEW        = 2202;
    static constexpr UINT ID_BTN_PROF_DELETE     = 2203;
    static constexpr UINT ID_BTN_PROF_EXPORT     = 2204;
    static constexpr UINT ID_BTN_PROF_IMPORT     = 2205;
    static constexpr UINT ID_LIST_PROFILES       = 2206;
    static constexpr UINT ID_BTN_PROF_DUPLICATE  = 2207;

    // Settings Controls
    static constexpr UINT ID_CHK_STARTUP         = 2301;
    static constexpr UINT ID_CHK_MIN_TRAY        = 2302;
    static constexpr UINT ID_CHK_CLOSE_TRAY      = 2303;
    static constexpr UINT ID_CHK_NOTIFY          = 2304;
    static constexpr UINT ID_BTN_SAVE_SETTINGS   = 2305;

    // Global Hotkeys
    static constexpr UINT ID_HOTKEY_DPI_CYCLE    = 9001;
    static constexpr UINT ID_HOTKEY_DPI_UP       = 9002;
    static constexpr UINT ID_HOTKEY_DPI_DOWN     = 9003;

    MainWindow(
        HINSTANCE hInstance,
        std::shared_ptr<app::services::DeviceService> device_service,
        std::shared_ptr<app::services::ProfileService> profile_service,
        std::shared_ptr<core::domain::ISettingsRepository> settings_repo);

    ~MainWindow();

    bool create(const std::wstring& title, int width = 1000, int height = 720);
    void show(int nCmdShow);
    [[nodiscard]] HWND get_hwnd() const noexcept { return hwnd_; }

    void show_window_from_tray();
    void hide_window_to_tray();
    void trim_working_set();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT handle_message(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void on_paint(HDC hdc);
    void switch_tab(NavTab tab);
    void update_controls_visibility();
    void query_memory_stats();
    void set_status_message(std::wstring msg, bool is_error = false);

    // Page renderers
    void render_sidebar(HDC mem_dc, int height);
    void render_page_dashboard(HDC mem_dc, int x, int y, int width, int height);
    void render_page_dpi(HDC mem_dc, int x, int y, int width, int height);
    void render_page_polling_rate(HDC mem_dc, int x, int y, int width, int height);
    void render_page_profiles(HDC mem_dc, int x, int y, int width, int height);
    void render_page_device_info(HDC mem_dc, int x, int y, int width, int height);
    void render_page_settings(HDC mem_dc, int x, int y, int width, int height);

    void refresh_profile_list();
    void load_settings_into_ui();
    void save_settings_from_ui();
    void sync_dpi_controls_from_staged();
    void cycle_dpi_stage();

    HINSTANCE hInstance_{nullptr};
    HWND hwnd_{nullptr};

    // Shared Header Controls
    HWND btn_refresh_{nullptr};

    // DPI Controls
    HWND btn_dpi_400_{nullptr};
    HWND btn_dpi_800_{nullptr};
    HWND btn_dpi_1200_{nullptr};
    HWND btn_dpi_1600_{nullptr};
    HWND btn_dpi_2400_{nullptr};
    HWND btn_dpi_3200_{nullptr};
    HWND btn_dpi_6400_{nullptr};
    HWND btn_dpi_minus_{nullptr};
    HWND btn_dpi_plus_{nullptr};
    HWND btn_dpi_apply_{nullptr};
    HWND btn_dpi_reset_{nullptr};
    HWND slider_dpi_{nullptr};
    HWND edit_dpi_custom_{nullptr};

    // Polling Rate Controls
    HWND btn_rate_125_{nullptr};
    HWND btn_rate_250_{nullptr};
    HWND btn_rate_500_{nullptr};
    HWND btn_rate_1000_{nullptr};

    // Profile Controls
    HWND list_profiles_{nullptr};
    HWND btn_prof_switch_{nullptr};
    HWND btn_prof_new_{nullptr};
    HWND btn_prof_duplicate_{nullptr};
    HWND btn_prof_delete_{nullptr};
    HWND btn_prof_export_{nullptr};
    HWND btn_prof_import_{nullptr};

    // Settings Controls
    HWND chk_startup_{nullptr};
    HWND chk_min_tray_{nullptr};
    HWND chk_close_tray_{nullptr};
    HWND chk_notify_{nullptr};
    HWND btn_save_settings_{nullptr};

    std::shared_ptr<app::services::DeviceService> device_service_;
    std::shared_ptr<app::services::ProfileService> profile_service_;
    std::shared_ptr<core::domain::ISettingsRepository> settings_repo_;
    std::unique_ptr<tray::TrayIcon> tray_icon_;

    NavTab active_tab_{NavTab::Dashboard};
    std::vector<core::domain::MouseDevice> devices_;
    std::vector<core::domain::MouseProfile> cached_profiles_;

    size_t ram_working_set_kb_{0};
    size_t ram_peak_working_set_kb_{0};
    bool exiting_{false};
    bool minimize_to_tray_{true};
    bool close_to_tray_{true};
    bool show_notifications_{true};
    bool start_with_windows_{false};
    int current_stage_index_{1}; // 800 DPI default

    std::wstring status_message_{L"Ready"};
    bool status_is_error_{false};
    int staged_dpi_{800};

    // GDI Dark Theme Brushes, Pens, and Fonts
    HBRUSH bg_brush_{nullptr};         // #0E1116
    HBRUSH sidebar_brush_{nullptr};    // #161B22
    HBRUSH card_brush_{nullptr};       // #1F242C
    HBRUSH card_active_brush_{nullptr};// #262C36
    HBRUSH header_brush_{nullptr};
    HPEN border_pen_{nullptr};         // #2D333B
    HPEN accent_pen_{nullptr};         // #2F81F7
    HPEN green_pen_{nullptr};          // #3FB950
    HFONT font_title_{nullptr};
    HFONT font_header_{nullptr};
    HFONT font_bold_{nullptr};
    HFONT font_body_{nullptr};
    HFONT font_mono_{nullptr};
    HFONT font_large_stat_{nullptr};
};

} // namespace mc::presentation::windows
