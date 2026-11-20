#include <gtest/gtest.h>
#include "application/use_cases/set_dpi_use_case.hpp"
#include "application/use_cases/set_polling_rate_use_case.hpp"
#include "application/use_cases/get_device_capabilities_use_case.hpp"
#include "core/domain/interfaces/i_mouse_adapter.hpp"
#include "infrastructure/adapters/generic/generic_adapter.hpp"
#include "infrastructure/adapters/adapter_registry.hpp"

using namespace mc::core;
using namespace mc::core::domain;
using namespace mc::app::use_cases;
using namespace mc::infra::adapters;

namespace {

class MockMouseAdapter : public IMouseAdapter {
public:
    [[nodiscard]] std::string_view get_adapter_name() const noexcept override {
        return "MockMouseAdapter";
    }

    [[nodiscard]] bool matches(const DeviceInfo&) const override {
        return true;
    }

    Result<bool> connect(const DeviceInfo&) override {
        connected_ = true;
        return true;
    }

    [[nodiscard]] Result<MouseCapabilities> get_capabilities() override {
        MouseCapabilities caps;
        caps.supports_dpi = true;
        caps.min_dpi = 50;
        caps.max_dpi = 26000;
        caps.dpi_step = 50;
        caps.supports_polling_rate = true;
        caps.supported_polling_rates = {125, 250, 500, 1000};
        return caps;
    }

    [[nodiscard]] Result<int> get_dpi() override {
        return current_dpi_;
    }

    Result<void> set_dpi(int dpi) override {
        if (!connected_) return Result<void>(ErrorCode::DeviceNotFound, "Mock not connected");
        current_dpi_ = dpi;
        return Result<void>::ok();
    }

    [[nodiscard]] Result<int> get_polling_rate() override {
        return current_rate_;
    }

    Result<void> set_polling_rate(int rate) override {
        if (!connected_) return Result<void>(ErrorCode::DeviceNotFound, "Mock not connected");
        current_rate_ = rate;
        return Result<void>::ok();
    }

    void disconnect() override {
        connected_ = false;
    }

    [[nodiscard]] bool is_connected() const noexcept override {
        return connected_;
    }

private:
    bool connected_{false};
    int current_dpi_{800};
    int current_rate_{1000};
};

} // namespace

TEST(UseCasesTest, SetDpiSuccessAndValidation) {
    auto adapter = std::make_unique<MockMouseAdapter>();

    DeviceInfo dev{};
    dev.vendor_id = 0x248A;
    dev.product_id = 0x8266;
    ASSERT_TRUE(adapter->connect(dev).has_value());

    SetDPIUseCase set_dpi_uc;

    // Successful setting of valid DPI
    EXPECT_TRUE(set_dpi_uc.execute(adapter.get(), 1600).has_value());
    EXPECT_EQ(adapter->get_dpi().value(), 1600);

    EXPECT_TRUE(set_dpi_uc.execute(adapter.get(), 800).has_value());
    EXPECT_EQ(adapter->get_dpi().value(), 800);

    // Validation catches invalid parameters
    EXPECT_FALSE(set_dpi_uc.execute(adapter.get(), 30).has_value());    // < min
    EXPECT_FALSE(set_dpi_uc.execute(adapter.get(), 30000).has_value()); // > max
    EXPECT_FALSE(set_dpi_uc.execute(adapter.get(), 825).has_value());   // step size violation

    adapter->disconnect();
    // Rejects when disconnected
    EXPECT_FALSE(set_dpi_uc.execute(adapter.get(), 800).has_value());
}

TEST(UseCasesTest, SetPollingRateSuccessAndValidation) {
    auto adapter = std::make_unique<MockMouseAdapter>();

    DeviceInfo dev{};
    dev.vendor_id = 0x248A;
    dev.product_id = 0x8266;
    ASSERT_TRUE(adapter->connect(dev).has_value());

    SetPollingRateUseCase set_poll_uc;

    // Successful setting of valid rate
    EXPECT_TRUE(set_poll_uc.execute(adapter.get(), 500).has_value());
    EXPECT_EQ(adapter->get_polling_rate().value(), 500);

    EXPECT_TRUE(set_poll_uc.execute(adapter.get(), 1000).has_value());
    EXPECT_EQ(adapter->get_polling_rate().value(), 1000);

    // Rejection of unsupported rate
    EXPECT_FALSE(set_poll_uc.execute(adapter.get(), 750).has_value());

    adapter->disconnect();
    EXPECT_FALSE(set_poll_uc.execute(adapter.get(), 1000).has_value());
}

TEST(UseCasesTest, GenericAdapterUnsupportedCapabilities) {
    auto registry = std::make_shared<AdapterRegistry>();
    GetDeviceCapabilitiesUseCase get_caps_uc(registry);

    DeviceInfo generic_dev{};
    generic_dev.vendor_id = 0x1111;
    generic_dev.product_id = 0x2222;

    auto caps_res = get_caps_uc.execute(generic_dev);
    ASSERT_TRUE(caps_res.has_value());

    const auto& caps = caps_res.value();
    // Generic adapter supports discovery only; cannot modify hardware settings
    EXPECT_FALSE(caps.supports_dpi);
    EXPECT_FALSE(caps.supports_polling_rate);

    generic::GenericAdapter gen_adapter;
    ASSERT_TRUE(gen_adapter.connect(generic_dev).has_value());

    SetDPIUseCase set_dpi_uc;
    auto dpi_res = set_dpi_uc.execute(&gen_adapter, 800);
    EXPECT_FALSE(dpi_res.has_value());
    EXPECT_EQ(dpi_res.error().code, ErrorCode::NotSupported);
}
