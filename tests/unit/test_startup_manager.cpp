#include <gtest/gtest.h>
#include "infrastructure/windows/startup_manager.hpp"

using namespace mc::infra::windows;

TEST(StartupManagerTest, ExecutablePathNotEmpty) {
    auto path = StartupManager::get_executable_path();
    EXPECT_FALSE(path.empty());
    EXPECT_NE(path.find(L".exe"), std::wstring::npos);
}

TEST(StartupManagerTest, EnableAndDisableStartupRegistry) {
    // Save original state
    bool originally_enabled = StartupManager::is_startup_enabled();

    // Enable startup with minimized flag
    bool set_res = StartupManager::set_startup_enabled(true, true);
    EXPECT_TRUE(set_res);
    EXPECT_TRUE(StartupManager::is_startup_enabled());

    // Disable startup
    bool unset_res = StartupManager::set_startup_enabled(false);
    EXPECT_TRUE(unset_res);
    EXPECT_FALSE(StartupManager::is_startup_enabled());

    // Restore original state if it was originally enabled
    if (originally_enabled) {
        StartupManager::set_startup_enabled(true);
    }
}
