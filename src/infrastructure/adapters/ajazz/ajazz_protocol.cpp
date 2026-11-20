#include "ajazz_protocol.hpp"

namespace mc::infra::adapters::ajazz {

core::Result<AjazzPacket> AjazzProtocol::build_set_dpi_stage(
    int stage_idx, int dpi, uint8_t r, uint8_t g, uint8_t b) noexcept {
    if (stage_idx < 1 || stage_idx > 6) {
        return core::Result<AjazzPacket>(
            core::ErrorCode::InvalidParameter, "DPI stage index must be between 1 and 6");
    }
    if (dpi < 50 || dpi > 26000) {
        return core::Result<AjazzPacket>(
            core::ErrorCode::InvalidParameter, "DPI value out of sensor range [50, 26000]");
    }
    if (dpi % 50 != 0) {
        return core::Result<AjazzPacket>(
            core::ErrorCode::InvalidParameter, "DPI value must be a multiple of 50");
    }

    AjazzPacket packet{};
    packet[0] = AJAZZ_REPORT_ID;
    packet[1] = static_cast<uint8_t>(AjazzCommand::Dpi);
    packet[2] = static_cast<uint8_t>(DpiSubCommand::SetStage);
    packet[3] = 0x06; // Length
    packet[4] = static_cast<uint8_t>(stage_idx);
    packet[5] = static_cast<uint8_t>((dpi >> 8) & 0xFF);
    packet[6] = static_cast<uint8_t>(dpi & 0xFF);
    packet[7] = r;
    packet[8] = g;
    packet[9] = b;
    packet[63] = calculate_checksum(packet);

    return packet;
}

core::Result<AjazzPacket> AjazzProtocol::build_set_active_stage(int stage_idx) noexcept {
    if (stage_idx < 1 || stage_idx > 6) {
        return core::Result<AjazzPacket>(
            core::ErrorCode::InvalidParameter, "Active stage index must be between 1 and 6");
    }

    AjazzPacket packet{};
    packet[0] = AJAZZ_REPORT_ID;
    packet[1] = static_cast<uint8_t>(AjazzCommand::Dpi);
    packet[2] = static_cast<uint8_t>(DpiSubCommand::SetActiveStage);
    packet[3] = 0x01; // Length
    packet[4] = static_cast<uint8_t>(stage_idx);
    packet[63] = calculate_checksum(packet);

    return packet;
}

AjazzPacket AjazzProtocol::build_query_dpi() noexcept {
    AjazzPacket packet{};
    packet[0] = AJAZZ_REPORT_ID;
    packet[1] = static_cast<uint8_t>(AjazzCommand::Dpi);
    packet[2] = static_cast<uint8_t>(DpiSubCommand::GetStages);
    packet[3] = 0x00;
    packet[63] = calculate_checksum(packet);
    return packet;
}

core::Result<AjazzPacket> AjazzProtocol::build_set_polling_rate(int rate_hz) noexcept {
    if (rate_hz != 125 && rate_hz != 250 && rate_hz != 500 && rate_hz != 1000 &&
        rate_hz != 2000 && rate_hz != 4000 && rate_hz != 8000) {
        return core::Result<AjazzPacket>(
            core::ErrorCode::InvalidParameter, "Unsupported polling rate. Allowed: 125, 250, 500, 1000, 2000, 4000, 8000 Hz");
    }

    AjazzPacket packet{};
    packet[0] = AJAZZ_REPORT_ID;
    packet[1] = static_cast<uint8_t>(AjazzCommand::PollingRate);
    packet[2] = static_cast<uint8_t>(PollingRateSubCommand::SetRate);
    packet[3] = 0x02; // Length
    packet[4] = static_cast<uint8_t>((rate_hz >> 8) & 0xFF);
    packet[5] = static_cast<uint8_t>(rate_hz & 0xFF);
    packet[63] = calculate_checksum(packet);

    return packet;
}

AjazzPacket AjazzProtocol::build_query_polling_rate() noexcept {
    AjazzPacket packet{};
    packet[0] = AJAZZ_REPORT_ID;
    packet[1] = static_cast<uint8_t>(AjazzCommand::PollingRate);
    packet[2] = static_cast<uint8_t>(PollingRateSubCommand::GetRate);
    packet[3] = 0x00;
    packet[63] = calculate_checksum(packet);
    return packet;
}

AjazzPacket AjazzProtocol::build_query_battery() noexcept {
    AjazzPacket packet{};
    packet[0] = AJAZZ_REPORT_ID;
    packet[1] = static_cast<uint8_t>(AjazzCommand::Battery);
    packet[2] = 0x00;
    packet[3] = 0x00;
    packet[63] = calculate_checksum(packet);
    return packet;
}

core::Result<int> AjazzProtocol::parse_polling_rate_response(
    std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept {
    if (packet[0] != AJAZZ_REPORT_ID) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Invalid report ID in response");
    }
    if (packet[1] != static_cast<uint8_t>(AjazzCommand::PollingRate)) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Mismatched command ID in polling rate response");
    }

    int rate = (static_cast<int>(packet[4]) << 8) | static_cast<int>(packet[5]);
    return rate;
}

core::Result<int> AjazzProtocol::parse_active_dpi_stage_response(
    std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept {
    if (packet[0] != AJAZZ_REPORT_ID) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Invalid report ID in response");
    }
    if (packet[1] != static_cast<uint8_t>(AjazzCommand::Dpi)) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Mismatched command ID in DPI response");
    }

    int stage = static_cast<int>(packet[4]);
    return stage;
}

core::Result<int> AjazzProtocol::parse_battery_response(
    std::span<const uint8_t, AJAZZ_PACKET_SIZE> packet) noexcept {
    if (packet[0] != AJAZZ_REPORT_ID) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Invalid report ID in response");
    }
    if (packet[1] != static_cast<uint8_t>(AjazzCommand::Battery)) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Mismatched command ID in battery response");
    }

    int level = static_cast<int>(packet[4]);
    if (level == 0xFF) {
        return core::Result<int>(core::ErrorCode::NotSupported, "Device battery telemetry offline");
    }
    if (level < 0 || level > 100) {
        return core::Result<int>(core::ErrorCode::InvalidData, "Battery percentage out of range [0, 100]");
    }
    return level;
}

} // namespace mc::infra::adapters::ajazz
