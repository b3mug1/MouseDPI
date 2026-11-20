#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "core/domain/interfaces/i_mouse_adapter.hpp"
#include "core/domain/entities/device_info.hpp"

namespace mc::infra::adapters {

class AdapterRegistry {
public:
    using AdapterFactory = std::function<std::unique_ptr<core::domain::IMouseAdapter>()>;

    AdapterRegistry();

    void register_adapter(AdapterFactory factory);

    [[nodiscard]] std::unique_ptr<core::domain::IMouseAdapter> create_adapter_for(
        const core::domain::DeviceInfo& device) const;

    [[nodiscard]] std::string_view identify_adapter_name(
        const core::domain::DeviceInfo& device) const;

private:
    std::vector<AdapterFactory> factories_;
};

} // namespace mc::infra::adapters
