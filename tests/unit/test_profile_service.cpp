#include <gtest/gtest.h>
#include "infrastructure/database/sqlite_database.hpp"
#include "infrastructure/database/sqlite_profile_repository.hpp"
#include "infrastructure/serialization/profile_serializer.hpp"
#include "application/services/profile_service.hpp"

using namespace mc::core;
using namespace mc::core::domain;
using namespace mc::infra::database;
using namespace mc::infra::serialization;
using namespace mc::app::services;
using namespace mc::app::use_cases;

TEST(ProfileSerializerTest, RoundtripJson) {
    MouseProfile p;
    p.name = "CS2";
    p.device_identifier = "Ajazz AJ159";
    p.polling_rate = 1000;
    p.active_stage_index = 0;
    p.dpi_stages = {
        {0, 400, true, 0x00FF00},
        {1, 800, true, 0x00FF00}
    };

    std::string json_str = ProfileSerializer::to_json(p);
    EXPECT_FALSE(json_str.empty());

    auto parsed_res = ProfileSerializer::from_json(json_str);
    ASSERT_TRUE(parsed_res.has_value());

    const auto& parsed = parsed_res.value();
    EXPECT_EQ(parsed.name, "CS2");
    EXPECT_EQ(parsed.polling_rate, 1000);
    ASSERT_EQ(parsed.dpi_stages.size(), 2);
    EXPECT_EQ(parsed.dpi_stages[0].dpi, 400);
    EXPECT_EQ(parsed.dpi_stages[1].dpi, 800);
}

TEST(ProfileSerializerTest, ParseSpecExample) {
    // Exact schema from PROJECT_SPEC.md line 365:
    std::string spec_json = R"({
        "name": "CS2",
        "dpi_stages": [400, 800],
        "active_dpi": 400,
        "polling_rate": 1000
    })";

    auto parsed_res = ProfileSerializer::from_json(spec_json);
    ASSERT_TRUE(parsed_res.has_value());

    const auto& parsed = parsed_res.value();
    EXPECT_EQ(parsed.name, "CS2");
    EXPECT_EQ(parsed.polling_rate, 1000);
    ASSERT_EQ(parsed.dpi_stages.size(), 2);
    EXPECT_EQ(parsed.dpi_stages[0].dpi, 400);
    EXPECT_EQ(parsed.dpi_stages[1].dpi, 800);
    EXPECT_EQ(parsed.active_stage_index, 0);
}

TEST(ProfileServiceTest, CreateAndExportImport) {
    auto db = std::make_shared<SqliteDatabase>(":memory:");
    ASSERT_TRUE(db->initialize().has_value());

    auto repo = std::make_shared<SqliteProfileRepository>(db);
    auto create_uc = std::make_shared<CreateProfileUseCase>(repo);
    auto update_uc = std::make_shared<UpdateProfileUseCase>(repo);
    auto delete_uc = std::make_shared<DeleteProfileUseCase>(repo);
    auto switch_uc = std::make_shared<SwitchProfileUseCase>(repo);
    auto get_uc    = std::make_shared<GetProfilesUseCase>(repo);

    ProfileService service(create_uc, update_uc, delete_uc, switch_uc, get_uc);

    std::vector<DpiStage> stages = {{0, 800, true, 0x00FF00}, {1, 1600, true, 0xFF0000}};
    auto create_res = service.create_profile("Valorant", "Ajazz AJ159", stages, 1000);
    ASSERT_TRUE(create_res.has_value());

    std::string prof_id = create_res.value().id;
    EXPECT_FALSE(prof_id.empty());

    // Export to JSON
    auto export_res = service.export_profile(prof_id);
    ASSERT_TRUE(export_res.has_value());
    EXPECT_NE(export_res.value().find("Valorant"), std::string::npos);

    // Import from JSON
    auto import_res = service.import_profile(export_res.value());
    ASSERT_TRUE(import_res.has_value());
    EXPECT_EQ(import_res.value().name, "Valorant");
    EXPECT_NE(import_res.value().id, prof_id); // new distinct ID
}

TEST(ProfileServiceTest, CapabilityValidation) {
    MouseCapabilities caps;
    caps.supports_dpi = true;
    caps.min_dpi = 100;
    caps.max_dpi = 16000;
    caps.dpi_step = 50;
    caps.supports_polling_rate = true;
    caps.supported_polling_rates = {125, 250, 500, 1000};

    MouseProfile valid_p;
    valid_p.dpi_stages = {{0, 400, true, 0}, {1, 800, true, 0}};
    valid_p.polling_rate = 1000;
    EXPECT_TRUE(ProfileService::validate_profile(valid_p, caps).has_value());

    // Invalid DPI below min
    MouseProfile invalid_dpi = valid_p;
    invalid_dpi.dpi_stages = {{0, 50, true, 0}};
    EXPECT_FALSE(ProfileService::validate_profile(invalid_dpi, caps).has_value());

    // Invalid Polling rate
    MouseProfile invalid_rate = valid_p;
    invalid_rate.polling_rate = 750;
    EXPECT_FALSE(ProfileService::validate_profile(invalid_rate, caps).has_value());
}
