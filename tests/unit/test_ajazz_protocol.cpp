#include <gtest/gtest.h>
#include "infrastructure/adapters/ajazz/ajazz_protocol.hpp"

using namespace mc::core;
using namespace mc::infra::adapters::ajazz;

TEST(AjazzProtocolTest, ChecksumVerification) {
    // Reference frame from protocol reverse engineering:
    // Set 1000 Hz: 05 22 00 02 03 E8 00 ... 00 [CS]
    auto res = AjazzProtocol::build_set_polling_rate(1000);
    ASSERT_TRUE(res.has_value());
    const auto& packet = res.value();

    EXPECT_EQ(packet[0], 0x05);
    EXPECT_EQ(packet[1], 0x22);
    EXPECT_EQ(packet[2], 0x00);
    EXPECT_EQ(packet[3], 0x02);
    EXPECT_EQ(packet[4], 0x03);
    EXPECT_EQ(packet[5], 0xE8);

    uint8_t expected_cs = (0x22 + 0x00 + 0x02 + 0x03 + 0xE8) & 0xFF;
    EXPECT_EQ(packet[63], expected_cs);
    EXPECT_TRUE(AjazzProtocol::verify_checksum(packet));
}

TEST(AjazzProtocolTest, BuildDpiStageValidation) {
    // Valid 1600 DPI, stage 2
    auto res = AjazzProtocol::build_set_dpi_stage(2, 1600, 0, 0, 255);
    ASSERT_TRUE(res.has_value());
    const auto& packet = res.value();

    EXPECT_EQ(packet[0], 0x05);
    EXPECT_EQ(packet[1], 0x21);
    EXPECT_EQ(packet[2], 0x00);
    EXPECT_EQ(packet[3], 0x06);
    EXPECT_EQ(packet[4], 0x02); // Stage index
    EXPECT_EQ(packet[5], 0x06); // High byte (1600 = 0x0640)
    EXPECT_EQ(packet[6], 0x40); // Low byte
    EXPECT_EQ(packet[7], 0x00); // Red
    EXPECT_EQ(packet[8], 0x00); // Green
    EXPECT_EQ(packet[9], 0xFF); // Blue
    EXPECT_TRUE(AjazzProtocol::verify_checksum(packet));

    // Invalid DPI bounds
    EXPECT_FALSE(AjazzProtocol::build_set_dpi_stage(1, 30));    // < 50
    EXPECT_FALSE(AjazzProtocol::build_set_dpi_stage(1, 30000)); // > 26000
    EXPECT_FALSE(AjazzProtocol::build_set_dpi_stage(1, 825));   // Not multiple of 50
    EXPECT_FALSE(AjazzProtocol::build_set_dpi_stage(0, 800));   // Stage < 1
    EXPECT_FALSE(AjazzProtocol::build_set_dpi_stage(7, 800));   // Stage > 6
}

TEST(AjazzProtocolTest, BuildSetActiveStage) {
    auto res = AjazzProtocol::build_set_active_stage(3);
    ASSERT_TRUE(res.has_value());
    const auto& packet = res.value();

    EXPECT_EQ(packet[0], 0x05);
    EXPECT_EQ(packet[1], 0x21);
    EXPECT_EQ(packet[2], 0x01);
    EXPECT_EQ(packet[3], 0x01);
    EXPECT_EQ(packet[4], 0x03);
    EXPECT_TRUE(AjazzProtocol::verify_checksum(packet));

    EXPECT_FALSE(AjazzProtocol::build_set_active_stage(0));
    EXPECT_FALSE(AjazzProtocol::build_set_active_stage(8));
}

TEST(AjazzProtocolTest, BuildPollingRateRates) {
    const std::vector<int> valid_rates = {125, 250, 500, 1000};
    for (int rate : valid_rates) {
        auto res = AjazzProtocol::build_set_polling_rate(rate);
        ASSERT_TRUE(res.has_value());
        EXPECT_TRUE(AjazzProtocol::verify_checksum(res.value()));
    }

    // Invalid rate
    EXPECT_FALSE(AjazzProtocol::build_set_polling_rate(333));
}

TEST(AjazzProtocolTest, ParseResponses) {
    // Mock polling rate response: 05 22 01 02 03 E8 ...
    AjazzPacket poll_resp{};
    poll_resp[0] = 0x05;
    poll_resp[1] = 0x22;
    poll_resp[2] = 0x01;
    poll_resp[3] = 0x02;
    poll_resp[4] = 0x03;
    poll_resp[5] = 0xE8; // 1000 Hz

    auto parsed_rate = AjazzProtocol::parse_polling_rate_response(poll_resp);
    ASSERT_TRUE(parsed_rate.has_value());
    EXPECT_EQ(parsed_rate.value(), 1000);

    // Mock active stage response: 05 21 01 01 04 ...
    AjazzPacket dpi_resp{};
    dpi_resp[0] = 0x05;
    dpi_resp[1] = 0x21;
    dpi_resp[2] = 0x01;
    dpi_resp[3] = 0x01;
    dpi_resp[4] = 0x04; // Stage 4

    auto parsed_stage = AjazzProtocol::parse_active_dpi_stage_response(dpi_resp);
    ASSERT_TRUE(parsed_stage.has_value());
    EXPECT_EQ(parsed_stage.value(), 4);
}

TEST(AjazzProtocolTest, BatteryQueriesAndResponses) {
    auto query = AjazzProtocol::build_query_battery();
    EXPECT_EQ(query[0], 0x05);
    EXPECT_EQ(query[1], 0x40);
    EXPECT_TRUE(AjazzProtocol::verify_checksum(query));

    // Mock battery response: 05 40 00 01 4E (78%) ...
    AjazzPacket bat_resp{};
    bat_resp[0] = 0x05;
    bat_resp[1] = 0x40;
    bat_resp[2] = 0x00;
    bat_resp[3] = 0x01;
    bat_resp[4] = 0x4E; // 78%

    auto parsed_bat = AjazzProtocol::parse_battery_response(bat_resp);
    ASSERT_TRUE(parsed_bat.has_value());
    EXPECT_EQ(parsed_bat.value(), 78);

    // Invalid battery response (> 100%)
    AjazzPacket invalid_bat = bat_resp;
    invalid_bat[4] = 105;
    EXPECT_FALSE(AjazzProtocol::parse_battery_response(invalid_bat).has_value());
}
