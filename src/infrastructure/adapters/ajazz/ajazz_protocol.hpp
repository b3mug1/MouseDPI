#pragma once

#include <cstdint>
#include <array>
#include <span>
#include <optional>
#include "core/types.hpp"

namespace mc::infra::adapters::ajazz {

constexpr size_t AJAZZ_PACKET_SIZE = 64;
constexpr uint8_t AJAZZ_REPORT_ID  = 0x05;

enum class AjazzCommand : uint8_t {
    Dpi         = 0x21,
    PollingRate = 0x22,
    Battery     = 0x40
};

enum class DpiSubCommand : uint8_t {
    SetStage       = 0x00,
    SetActiveStage = 0x01,
    GetStages      = 0x02
};

enum class PollingRateSubCommand : uint8_t {
    SetRate = 0x00,
    GetRate = 0x01
};

using AjazzPacket = std::array<uint8_t, AJAZZ_PACKET_SIZE>;

class AjazzProtocol {
public:
    // Calculates checksum: (sum of bytes 1 through 62) mod 256
    [[nodiscard]] static uint8_t calculate_checksum(std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept {
        uint32_t sum = 0;
        for (size_t i = 1; i < 63; ++i) {
            sum += packet[i];
        }
        return static_cast<uint8_t>(sum & 0xFF);
    }

    [[nodiscard]] static bool verify_checksum(std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept {
        return packet[63] == calculate_checksum(packet);
    }

    // Packet Builders
    [[nodiscard]] static core::Result<AjazzPacket> build_set_dpi_stage(
        int stage_idx, int dpi, uint8_t r = 0, uint8_t g = 255, uint8_t b = 0) noexcept;

    [[nodiscard]] static core::Result<AjazzPacket> build_set_active_stage(int stage_idx) noexcept;

    [[nodiscard]] static AjazzPacket build_query_dpi() noexcept;

    [[nodiscard]] static core::Result<AjazzPacket> build_set_polling_rate(int rate_hz) noexcept;

    [[nodiscard]] static AjazzPacket build_query_polling_rate() noexcept;

    [[nodiscard]] static AjazzPacket build_query_battery() noexcept;

    // Packet Parsers
    [[nodiscard]] static core::Result<int> parse_polling_rate_response(
        std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept;

    [[nodiscard]] static core::Result<int> parse_active_dpi_stage_response(
        std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept;

    [[nodiscard]] static core::Result<int> parse_battery_response(
        std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept;
};

} // namespace mc::infra::adapters::ajazz
