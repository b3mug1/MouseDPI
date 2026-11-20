cmake_minimum_required(VERSION 3.20)

project(MouseControl
    VERSION 0.1.0
    DESCRIPTION "Universal Gaming Mouse Management Software"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler configurations for MSVC
if(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Gy>
        $<$<CONFIG:Release>:/GL>
    )
    add_link_options(
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
    )
endif()

# Find dependencies via vcpkg
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Common core library
add_library(mousecontrol_core STATIC
    src/infrastructure/logging/logger.cpp
    src/infrastructure/hid/windows_hid_enumerator.cpp
    src/infrastructure/hid/windows_device_notifier.cpp
    src/infrastructure/windows/startup_manager.cpp
    src/infrastructure/adapters/ajazz/ajazz_protocol.cpp
    src/infrastructure/adapters/ajazz/ajazz_adapter.cpp
    src/infrastructure/adapters/generic/generic_adapter.cpp
    src/infrastructure/adapters/adapter_registry.cpp
    src/infrastructure/database/sqlite_database.cpp
    src/infrastructure/database/sqlite_profile_repository.cpp
    src/infrastructure/database/sqlite_settings_repository.cpp
    src/infrastructure/serialization/profile_serializer.cpp
    src/application/use_cases/discover_devices_use_case.cpp
    src/application/use_cases/get_device_info_use_case.cpp
    src/application/use_cases/get_device_capabilities_use_case.cpp
    src/application/use_cases/set_dpi_use_case.cpp
    src/application/use_cases/set_polling_rate_use_case.cpp
    src/application/use_cases/manage_profile_use_cases.cpp
    src/application/use_cases/manage_settings_use_cases.cpp
    src/application/services/device_service.cpp
    src/application/services/profile_service.cpp
)

target_include_directories(mousecontrol_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mousecontrol_core PUBLIC
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    setupapi.lib
    hid.lib
    user32.lib
    gdi32.lib
    psapi.lib
    advapi32.lib
    windowsapp.lib
)

# 1. Main Windows GUI application
add_executable(MouseControl WIN32
    src/main.cpp
    src/presentation/windows/main_window.cpp
    src/presentation/system_tray/tray_icon.cpp
)

target_link_libraries(MouseControl PRIVATE
    mousecontrol_core
    shell32.lib
    comctl32.lib
)

# 2. Standalone CLI Device Inspector
add_executable(device_inspector
    src/tools/device_inspector/inspector_main.cpp
)

target_link_libraries(device_inspector PRIVATE
    mousecontrol_core
)

# 2b. Hardware probe
add_executable(hardware_probe
    src/tools/hardware_probe/probe_main.cpp
)

target_link_libraries(hardware_probe PRIVATE
    mousecontrol_core
)

# 2c. BLE Probe
add_executable(ble_probe
    src/tools/hardware_probe/ble_probe.cpp
)

target_link_libraries(ble_probe PRIVATE
    windowsapp.lib
)

# 3. Unit Tests
enable_testing()

add_executable(mousedpi_tests
    tests/unit/test_main.cpp
    tests/unit/test_result.cpp
    tests/unit/test_device_info.cpp
    tests/unit/test_adapter_registry.cpp
    tests/unit/test_ajazz_protocol.cpp
    tests/unit/test_sqlite_repositories.cpp
    tests/unit/test_profile_service.cpp
    tests/unit/test_use_cases.cpp
    tests/unit/test_startup_manager.cpp
)

target_link_libraries(mousedpi_tests PRIVATE
    mousecontrol_core
    GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(mousedpi_tests)

# 4. Installation & Packaging (Phase 9 - Release)
include(GNUInstallDirs)

install(TARGETS MouseControl device_inspector
    RUNTIME DESTINATION bin
)

if(WIN32)
    # Package runtime DLL dependencies from build directory
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/Release/fmt.dll"
        "${CMAKE_CURRENT_BINARY_DIR}/Release/spdlog.dll"
        "${CMAKE_CURRENT_BINARY_DIR}/Release/sqlite3.dll"
        DESTINATION bin
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "MouseControl")
set(CPACK_PACKAGE_VENDOR "MouseControl")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight Universal Gaming Mouse Management Software for Windows 11")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "ZIP")
include(CPack)
