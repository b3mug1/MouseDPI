#include <gtest/gtest.h>
#include "infrastructure/adapters/adapter_registry.hpp"
#include "infrastructure/adapters/ajazz/ajazz_adapter.hpp"

using namespace mc::core;
using namespace mc::core::domain;
using namespace mc::infra::adapters;

TEST(AdapterRegistryTest, MatchesAjazzByVidPid) {
    AdapterRegistry registry;

    DeviceInfo ajazz_ble{};
    ajazz_ble.vendor_id = 0x248A;
    ajazz_ble.product_id = 0x8266;
    ajazz_ble.connection_type = ConnectionType::BluetoothLE;

    EXPECT_EQ(registry.identify_adapter_name(ajazz_ble), "AjazzAdapter");

    DeviceInfo ajazz_dongle{};
    ajazz_dongle.vendor_id = 0x248A;
    ajazz_dongle.product_id = 0x5D2E;
    ajazz_dongle.connection_type = ConnectionType::Wireless24G;

    EXPECT_EQ(registry.identify_adapter_name(ajazz_dongle), "AjazzAdapter");
}

TEST(AdapterRegistryTest, FallbackToGenericAdapter) {
    AdapterRegistry registry;

    DeviceInfo other{};
    other.vendor_id = 0x1234;
    other.product_id = 0x5678;

    EXPECT_EQ(registry.identify_adapter_name(other), "GenericAdapter");
}

TEST(AjazzAdapterTest, ConnectionAndCapsReadOnly) {
    ajazz::AjazzAdapter adapter;

    DeviceInfo ajazz_dev{};
    ajazz_dev.vendor_id = 0x248A;
    ajazz_dev.product_id = 0x8266;
    ajazz_dev.connection_type = ConnectionType::BluetoothLE;

    auto conn_res = adapter.connect(ajazz_dev);
    ASSERT_TRUE(conn_res.has_value());
    EXPECT_TRUE(adapter.is_connected());

    auto caps = adapter.get_capabilities();
    ASSERT_TRUE(caps.has_value());
    EXPECT_EQ(caps.value().min_dpi, 50);
    EXPECT_EQ(caps.value().max_dpi, 26000);
    // DPI & Polling Rate control are enabled in Phase 3
    EXPECT_TRUE(caps.value().supports_dpi);
    EXPECT_TRUE(caps.value().supports_polling_rate);

    // Validation catches invalid parameters before hardware transmission
    auto invalid_dpi = adapter.set_dpi(33); // below min
    EXPECT_FALSE(invalid_dpi.has_value());
    EXPECT_EQ(invalid_dpi.error().code, ErrorCode::InvalidParameter);

    auto invalid_step = adapter.set_dpi(825); // not multiple of 50
    EXPECT_FALSE(invalid_step.has_value());
    EXPECT_EQ(invalid_step.error().code, ErrorCode::InvalidParameter);

    auto invalid_rate = adapter.set_polling_rate(350); // invalid polling rate
    EXPECT_FALSE(invalid_rate.has_value());
    EXPECT_EQ(invalid_rate.error().code, ErrorCode::InvalidParameter);

    adapter.disconnect();
    EXPECT_FALSE(adapter.is_connected());
}
