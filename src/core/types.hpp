#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <system_error>
#include <optional>
#include <format>
#include <functional>

namespace mc::core {

enum class ConnectionType : uint8_t {
    Unknown,
    WiredUsb,
    Wireless24G,
    BluetoothLE
};

[[nodiscard]] constexpr std::string_view connection_type_to_string(ConnectionType type) noexcept {
    switch (type) {
        case ConnectionType::WiredUsb:    return "Wired USB";
        case ConnectionType::Wireless24G: return "2.4 GHz Wireless";
        case ConnectionType::BluetoothLE: return "Bluetooth LE";
        case ConnectionType::Unknown:
        default:                         return "Unknown";
    }
}

enum class ErrorCode : uint16_t {
    Ok = 0,
    DeviceNotFound,
    DeviceOpenFailed,
    AccessDenied,
    InvalidParameter,
    NotSupported,
    IoError,
    Timeout,
    BufferTooSmall,
    EnumerationFailed,
    InvalidData,
    InvalidOperation,
    Unknown
};

[[nodiscard]] constexpr std::string_view error_code_to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:                return "Success";
        case ErrorCode::DeviceNotFound:    return "Device not found";
        case ErrorCode::DeviceOpenFailed:  return "Failed to open device";
        case ErrorCode::AccessDenied:      return "Access denied";
        case ErrorCode::InvalidParameter:  return "Invalid parameter";
        case ErrorCode::NotSupported:      return "Operation not supported";
        case ErrorCode::IoError:           return "I/O communication error";
        case ErrorCode::Timeout:           return "Operation timed out";
        case ErrorCode::BufferTooSmall:    return "Buffer too small";
        case ErrorCode::EnumerationFailed: return "Device enumeration failed";
        case ErrorCode::InvalidData:       return "Invalid or corrupted data";
        case ErrorCode::InvalidOperation:  return "Invalid operation";
        case ErrorCode::Unknown:
        default:                           return "Unknown error";
    }
}

struct Error {
    ErrorCode code{ErrorCode::Unknown};
    std::string message;
    uint32_t system_code{0};

    [[nodiscard]] std::string to_string() const {
        if (system_code != 0) {
            return std::format("{}: {} (sys code: 0x{:X})", 
                error_code_to_string(code), message, system_code);
        }
        return std::format("{}: {}", error_code_to_string(code), message);
    }
};

template <typename T>
class [[nodiscard]] Result {
public:
    Result(const T& val) : data_(val) {}
    Result(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>) : data_(std::move(val)) {}
    Result(Error err) : data_(std::move(err)) {}
    Result(ErrorCode code, std::string message = {}, uint32_t sys_code = 0)
        : data_(Error{code, std::move(message), sys_code}) {}

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] const T& value() const & {
        return std::get<T>(data_);
    }

    [[nodiscard]] T& value() & {
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(data_));
    }

    [[nodiscard]] const T& value_or(const T& default_val) const & noexcept {
        return has_value() ? std::get<T>(data_) : default_val;
    }

    [[nodiscard]] const Error& error() const & noexcept {
        return std::get<Error>(data_);
    }

    [[nodiscard]] Error& error() & noexcept {
        return std::get<Error>(data_);
    }

    template <typename F>
    auto map(F&& func) const -> Result<std::invoke_result_t<F, const T&>> {
        using ReturnType = std::invoke_result_t<F, const T&>;
        if (has_value()) {
            return Result<ReturnType>(func(value()));
        }
        return Result<ReturnType>(error());
    }

private:
    std::variant<T, Error> data_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() : error_(std::nullopt) {}
    Result(Error err) : error_(std::move(err)) {}
    Result(ErrorCode code, std::string message = {}, uint32_t sys_code = 0)
        : error_(Error{code, std::move(message), sys_code}) {}

    static Result<void> ok() noexcept {
        return Result<void>();
    }

    [[nodiscard]] bool has_value() const noexcept {
        return !error_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] const Error& error() const & noexcept {
        return *error_;
    }

    [[nodiscard]] Error& error() & noexcept {
        return *error_;
    }

private:
    std::optional<Error> error_;
};

} // namespace mc::core
