#include <gtest/gtest.h>
#include "core/domain/entities/device_info.hpp"

using namespace mc::core;
using namespace mc::core::domain;

TEST(DeviceInfoTest, VidPidFormatting) {
    DeviceInfo dev{};
    dev.vendor_id = 0x248A;
    dev.product_id = 0x8266;
    EXPECT_EQ(dev.format_vid_pid(), "VID: 248A, PID: 8266");
}

TEST(DeviceInfoTest, UsageFormatting) {
    DeviceInfo dev{};
    dev.usage_page = 0x0001;
    dev.usage = 0x0002;
    EXPECT_EQ(dev.format_usage(), "UsagePage: 0x0001, Usage: 0x0002");
}

TEST(DeviceInfoTest, ConnectionTypeConversion) {
    EXPECT_EQ(connection_type_to_string(ConnectionType::WiredUsb), "Wired USB");
    EXPECT_EQ(connection_type_to_string(ConnectionType::Wireless24G), "2.4 GHz Wireless");
    EXPECT_EQ(connection_type_to_string(ConnectionType::BluetoothLE), "Bluetooth LE");
    EXPECT_EQ(connection_type_to_string(ConnectionType::Unknown), "Unknown");
}
