#include <gtest/gtest.h>
#include "core/types.hpp"

using namespace mc::core;

TEST(ResultTest, SuccessValue) {
    Result<int> res(42);
    EXPECT_TRUE(res.has_value());
    EXPECT_TRUE(static_cast<bool>(res));
    EXPECT_EQ(res.value(), 42);
    EXPECT_EQ(res.value_or(10), 42);
}

TEST(ResultTest, ErrorHandling) {
    Result<int> res(ErrorCode::DeviceNotFound, "Test device error", 0x123);
    EXPECT_FALSE(res.has_value());
    EXPECT_FALSE(static_cast<bool>(res));
    EXPECT_EQ(res.error().code, ErrorCode::DeviceNotFound);
    EXPECT_EQ(res.error().system_code, 0x123);
    EXPECT_EQ(res.value_or(99), 99);
}

TEST(ResultTest, VoidResultSuccess) {
    auto res = Result<void>::ok();
    EXPECT_TRUE(res.has_value());
}

TEST(ResultTest, VoidResultError) {
    Result<void> res(ErrorCode::AccessDenied, "Cannot open device");
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code, ErrorCode::AccessDenied);
}

TEST(ResultTest, MonadicMap) {
    Result<int> res(5);
    auto mapped = res.map([](int x) { return std::to_string(x * 2); });
    EXPECT_TRUE(mapped.has_value());
    EXPECT_EQ(mapped.value(), "10");
}
