#include "adapter_registry.hpp"
#include "ajazz/ajazz_adapter.hpp"
#include "generic/generic_adapter.hpp"

namespace mc::infra::adapters {

AdapterRegistry::AdapterRegistry() {
    // Register adapters in priority order: specialized first, generic fallback last
    register_adapter([]() -> std::unique_ptr<core::domain::IMouseAdapter> {
        return std::make_unique<ajazz::AjazzAdapter>();
    });

    register_adapter([]() -> std::unique_ptr<core::domain::IMouseAdapter> {
        return std::make_unique<generic::GenericAdapter>();
    });
}

void AdapterRegistry::register_adapter(AdapterFactory factory) {
    factories_.push_back(std::move(factory));
}

std::unique_ptr<core::domain::IMouseAdapter> AdapterRegistry::create_adapter_for(
    const core::domain::DeviceInfo& device) const {
    for (const auto& factory : factories_) {
        auto adapter = factory();
        if (adapter && adapter->matches(device)) {
            return adapter;
        }
    }
    return std::make_unique<generic::GenericAdapter>();
}

std::string_view AdapterRegistry::identify_adapter_name(
    const core::domain::DeviceInfo& device) const {
    for (const auto& factory : factories_) {
        auto adapter = factory();
        if (adapter && adapter->matches(device)) {
            return adapter->get_adapter_name();
        }
    }
    return "GenericAdapter";
}

} // namespace mc::infra::adapters
