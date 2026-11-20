#include <gtest/gtest.h>
#include "infrastructure/database/sqlite_database.hpp"
#include "infrastructure/database/sqlite_profile_repository.hpp"
#include "infrastructure/database/sqlite_settings_repository.hpp"

using namespace mc::core;
using namespace mc::core::domain;
using namespace mc::infra::database;

class SqliteRepositoriesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use in-memory SQLite database for high-speed, isolated unit tests
        db_ = std::make_shared<SqliteDatabase>(":memory:");
        auto res = db_->initialize();
        ASSERT_TRUE(res.has_value());

        profile_repo_ = std::make_unique<SqliteProfileRepository>(db_);
        settings_repo_ = std::make_unique<SqliteSettingsRepository>(db_);
    }

    std::shared_ptr<SqliteDatabase> db_;
    std::unique_ptr<SqliteProfileRepository> profile_repo_;
    std::unique_ptr<SqliteSettingsRepository> settings_repo_;
};

TEST_F(SqliteRepositoriesTest, ProfileSaveAndGetById) {
    MouseProfile p;
    p.id = "test-prof-1";
    p.name = "Competitive FPS";
    p.device_identifier = "Ajazz-AJ159";
    p.polling_rate = 1000;
    p.active_stage_index = 1;
    p.dpi_stages = {
        {0, 400, true, 0xFF0000},
        {1, 800, true, 0x00FF00},
        {2, 1600, true, 0x0000FF}
    };

    auto save_res = profile_repo_->save(p);
    ASSERT_TRUE(save_res.has_value());

    auto get_res = profile_repo_->get_by_id("test-prof-1");
    ASSERT_TRUE(get_res.has_value());
    ASSERT_TRUE(get_res.value().has_value());

    const auto& loaded = get_res.value().value();
    EXPECT_EQ(loaded.id, "test-prof-1");
    EXPECT_EQ(loaded.name, "Competitive FPS");
    EXPECT_EQ(loaded.device_identifier, "Ajazz-AJ159");
    EXPECT_EQ(loaded.polling_rate, 1000);
    EXPECT_EQ(loaded.active_stage_index, 1);
    ASSERT_EQ(loaded.dpi_stages.size(), 3);
    EXPECT_EQ(loaded.dpi_stages[0].dpi, 400);
    EXPECT_EQ(loaded.dpi_stages[1].dpi, 800);
    EXPECT_EQ(loaded.dpi_stages[2].dpi, 1600);
}

TEST_F(SqliteRepositoriesTest, ProfileGetByDeviceAndGetAll) {
    MouseProfile p1;
    p1.id = "p1";
    p1.name = "Apex Legends";
    p1.device_identifier = "Ajazz-AJ159";
    p1.polling_rate = 1000;
    ASSERT_TRUE(profile_repo_->save(p1).has_value());

    MouseProfile p2;
    p2.id = "p2";
    p2.name = "Work / Desktop";
    p2.device_identifier = "Ajazz-AJ159";
    p2.polling_rate = 500;
    ASSERT_TRUE(profile_repo_->save(p2).has_value());

    MouseProfile p3;
    p3.id = "p3";
    p3.name = "Other Device Profile";
    p3.device_identifier = "Razer-Viper";
    p3.polling_rate = 1000;
    ASSERT_TRUE(profile_repo_->save(p3).has_value());

    auto dev_res = profile_repo_->get_by_device("Ajazz-AJ159");
    ASSERT_TRUE(dev_res.has_value());
    EXPECT_EQ(dev_res.value().size(), 2);

    auto all_res = profile_repo_->get_all();
    ASSERT_TRUE(all_res.has_value());
    EXPECT_EQ(all_res.value().size(), 3);
}

TEST_F(SqliteRepositoriesTest, ProfileUpdateAndRemove) {
    MouseProfile p;
    p.id = "p-update";
    p.name = "Original Name";
    p.polling_rate = 500;
    ASSERT_TRUE(profile_repo_->save(p).has_value());

    // Update
    p.name = "Renamed Profile";
    p.polling_rate = 1000;
    ASSERT_TRUE(profile_repo_->save(p).has_value());

    auto loaded = profile_repo_->get_by_id("p-update");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded.value()->name, "Renamed Profile");
    EXPECT_EQ(loaded.value()->polling_rate, 1000);

    // Remove
    ASSERT_TRUE(profile_repo_->remove("p-update").has_value());
    auto removed = profile_repo_->get_by_id("p-update");
    ASSERT_TRUE(removed.has_value());
    EXPECT_FALSE(removed.value().has_value());
}

TEST_F(SqliteRepositoriesTest, SettingsSaveAndLoad) {
    DeviceSettings s;
    s.start_with_windows = true;
    s.minimize_to_tray = true;
    s.close_to_tray = false;
    s.show_notifications = true;
    s.default_profile_id = "default-123";
    s.active_device_id = "device-456";

    ASSERT_TRUE(settings_repo_->save_settings(s).has_value());

    auto loaded_res = settings_repo_->load_settings();
    ASSERT_TRUE(loaded_res.has_value());

    const auto& loaded = loaded_res.value();
    EXPECT_TRUE(loaded.start_with_windows);
    EXPECT_TRUE(loaded.minimize_to_tray);
    EXPECT_FALSE(loaded.close_to_tray);
    EXPECT_TRUE(loaded.show_notifications);
    EXPECT_EQ(loaded.default_profile_id, "default-123");
    EXPECT_EQ(loaded.active_device_id, "device-456");
}
