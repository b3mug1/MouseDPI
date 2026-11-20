#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <memory>
#include <string_view>
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/hid/windows_hid_enumerator.hpp"
#include "infrastructure/hid/windows_device_notifier.hpp"
#include "infrastructure/windows/startup_manager.hpp"
#include "infrastructure/adapters/adapter_registry.hpp"
#include "infrastructure/database/sqlite_database.hpp"
#include "infrastructure/database/sqlite_profile_repository.hpp"
#include "infrastructure/database/sqlite_settings_repository.hpp"
#include "application/use_cases/discover_devices_use_case.hpp"
#include "application/use_cases/set_dpi_use_case.hpp"
#include "application/use_cases/set_polling_rate_use_case.hpp"
#include "application/use_cases/manage_profile_use_cases.hpp"
#include "application/use_cases/manage_settings_use_cases.hpp"
#include "application/services/device_service.hpp"
#include "application/services/profile_service.hpp"
#include "presentation/windows/main_window.hpp"

int WINAPI wWinMain(
    HINSTANCE hInstance,
    [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] PWSTR pCmdLine,
    int nCmdShow) {

    // Modern per-monitor v2 DPI awareness for crystal-clear text rendering
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize structured logging system
    mc::infra::logging::Logger::init("logs/mousecontrol.log", true);
    MC_INFO("======================================================================");
    MC_INFO("MouseControl starting up (Phase 7 & 8 - Integration & Optimization)");
    MC_INFO("======================================================================");

    // Check command line flags
    bool start_minimized = false;
    if (pCmdLine) {
        std::wstring_view cmd(pCmdLine);
        if (cmd.find(L"--minimized") != std::wstring_view::npos || cmd.find(L"--tray") != std::wstring_view::npos) {
            start_minimized = true;
        }
    }

    // Phase 7: Single-Instance Application Mutex
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\MouseControl_SingleInstance_Mutex_v1");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MC_INFO("Another instance of MouseControl is already running. Focusing existing window.");
        HWND hExisting = FindWindowW(mc::presentation::windows::MainWindow::CLASS_NAME, nullptr);
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        if (hMutex) {
            CloseHandle(hMutex);
        }
        mc::infra::logging::Logger::shutdown();
        return 0;
    }

    // 1. Database & Persistence Layer (Phase 5)
    auto db = std::make_shared<mc::infra::database::SqliteDatabase>("mousecontrol.db");
    auto db_init_res = db->initialize();
    if (!db_init_res) {
        MC_WARN("SQLite database initialization warning: {}", db_init_res.error().to_string());
    }

    auto profile_repo  = std::make_shared<mc::infra::database::SqliteProfileRepository>(db);
    auto settings_repo = std::make_shared<mc::infra::database::SqliteSettingsRepository>(db);

    // 2. Profile Use Cases & Service
    auto create_prof_uc = std::make_shared<mc::app::use_cases::CreateProfileUseCase>(profile_repo);
    auto update_prof_uc = std::make_shared<mc::app::use_cases::UpdateProfileUseCase>(profile_repo);
    auto delete_prof_uc = std::make_shared<mc::app::use_cases::DeleteProfileUseCase>(profile_repo);
    auto switch_prof_uc = std::make_shared<mc::app::use_cases::SwitchProfileUseCase>(profile_repo);
    auto get_profs_uc   = std::make_shared<mc::app::use_cases::GetProfilesUseCase>(profile_repo);

    auto profile_service = std::make_shared<mc::app::services::ProfileService>(
        create_prof_uc, update_prof_uc, delete_prof_uc, switch_prof_uc, get_profs_uc);

    // Ensure initial default profile exists
    auto existing_profs = profile_service->get_profiles();
    if (existing_profs && existing_profs.value().empty()) {
        std::vector<mc::core::domain::DpiStage> default_stages = {
            {0, 400, true, 0xFF0000},
            {1, 800, true, 0x00FF00},
            {2, 1600, true, 0x0000FF},
            {3, 3200, true, 0xFFFF00}
        };
        auto def_res = profile_service->create_profile("Default", "Ajazz AJ159", default_stages, 1000);
        if (def_res) {
            MC_INFO("Created initial default profile (ID: {})", def_res.value().id);
        }
    }

    // 3. Infrastructure & Device Communication (Phase 1..4)
    auto enumerator = std::make_shared<mc::infra::hid::WindowsHidEnumerator>();
    auto registry   = std::make_shared<mc::infra::adapters::AdapterRegistry>();
    auto notifier   = std::make_shared<mc::infra::hid::WindowsDeviceNotifier>();

    auto discover_use_case         = std::make_shared<mc::app::use_cases::DiscoverDevicesUseCase>(enumerator, registry);
    auto set_dpi_use_case          = std::make_shared<mc::app::use_cases::SetDPIUseCase>();
    auto set_polling_rate_use_case = std::make_shared<mc::app::use_cases::SetPollingRateUseCase>();

    auto device_service = std::make_shared<mc::app::services::DeviceService>(
        discover_use_case, registry, set_dpi_use_case, set_polling_rate_use_case, notifier);

    // 4. Presentation Main Window (Phase 6 - Multi-Page Native UI)
    auto main_window = std::make_unique<mc::presentation::windows::MainWindow>(
        hInstance, device_service, profile_service, settings_repo);

    if (!main_window->create(L"MouseControl \u2014 Universal Gaming Mouse Management", 1020, 720)) {
        MC_CRITICAL("Failed to initialize main application window");
        mc::infra::logging::Logger::shutdown();
        return 1;
    }

    // Attach HWND to device notifier for WM_DEVICECHANGE listening
    notifier->set_hwnd(main_window->get_hwnd());
    device_service->initialize();

    // Display window or start minimized to tray
    if (start_minimized) {
        main_window->hide_window_to_tray();
        MC_INFO("Application launched minimized to System Tray (--minimized flag)");
    } else {
        main_window->show(nCmdShow);
        MC_INFO("Application window created and displayed successfully");
    }

    // Win32 event-driven message loop (Zero idle CPU usage)
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    MC_INFO("Application shutting down cleanly");
    main_window.reset();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    mc::infra::logging::Logger::shutdown();
    return static_cast<int>(msg.wParam);
}
