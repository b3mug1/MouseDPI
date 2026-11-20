#pragma once

#include <stdexcept>
#include <string>

namespace mc::core::domain {

class DomainException : public std::runtime_error {
public:
    explicit DomainException(const std::string& message) : std::runtime_error(message) {}
};

class DeviceNotFoundException : public DomainException {
public:
    explicit DeviceNotFoundException(const std::string& message) : DomainException(message) {}
};

class DeviceCommunicationException : public DomainException {
public:
    explicit DeviceCommunicationException(const std::string& message) : DomainException(message) {}
};

class UnsupportedOperationException : public DomainException {
public:
    explicit UnsupportedOperationException(const std::string& message) : DomainException(message) {}
};

} // namespace mc::core::domain
