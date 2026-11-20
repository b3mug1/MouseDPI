#include "discover_devices_use_case.hpp"
#include "infrastructure/logging/logger.hpp"
#include <map>

namespace mc::app::use_cases {

DiscoverDevicesUseCase::DiscoverDevicesUseCase(
    std::shared_ptr<core::domain::IDeviceEnumerator> enumerator,
    std::shared_ptr<infra::adapters::AdapterRegistry> registry)
    : enumerator_(std::move(enumerator)), registry_(std::move(registry)) {}

core::Result<std::vector<core::domain::MouseDevice>> DiscoverDevicesUseCase::execute() {
    auto enum_res = enumerator_->enumerate_devices();
    if (!enum_res) {
        return enum_res.error();
    }

    const auto& raw_devices = enum_res.value();

    // Group interfaces by physical device (using VID + PID + Serial/Address)
    struct DeviceKey {
        uint16_t vid;
        uint16_t pid;
        core::ConnectionType conn_type;

        bool operator<(const DeviceKey& other) const {
            if (vid != other.vid) return vid < other.vid;
            if (pid != other.pid) return pid < other.pid;
            return conn_type < other.conn_type;
        }
    };

    std::map<DeviceKey, std::vector<core::domain::DeviceInfo>> grouped;
    for (const auto& dev : raw_devices) {
        // Group all devices
        grouped[{dev.vendor_id, dev.product_id, dev.connection_type}].push_back(dev);
    }

    std::vector<core::domain::MouseDevice> result;

    for (const auto& [key, ifaces] : grouped) {
        if (ifaces.empty()) continue;

        // Determine device display name and manufacturer
        std::string name = "Generic HID Device";
        std::string mfg = "Unknown";
        bool has_mouse_interface = false;
        std::string adapter_name = "GenericAdapter";

        for (const auto& iface : ifaces) {
            if (!iface.product_name.empty()) {
                name = iface.product_name;
            }
            if (!iface.manufacturer.empty()) {
                mfg = iface.manufacturer;
            }
            if (iface.is_mouse) {
                has_mouse_interface = true;
            }

            auto matched = registry_->identify_adapter_name(iface);
            if (matched != "GenericAdapter") {
                adapter_name = std::string(matched);
            }
        }

        // If it's an Ajazz device, format friendly names if empty from Windows driver
        if (key.vid == 0x248A) {
            mfg = "Ajazz";
            if (key.pid == 0x8266) {
                name = "Ajazz AJ159 Pro (BLE)";
            } else if (key.pid == 0x5D2E) {
                name = "Ajazz AJ159 (2.4G Wireless Receiver)";
            }
        }

        // Only create MouseDevice for mice or devices matched by non-generic adapter
        if (has_mouse_interface || adapter_name == "AjazzAdapter") {
            core::domain::MouseDevice mouse(name, mfg, key.conn_type);
            mouse.set_adapter_name(adapter_name);

            // Populate capabilities
            auto adapter = registry_->create_adapter_for(ifaces.front());
            if (adapter) {
                if (auto conn_res = adapter->connect(ifaces.front()); conn_res) {
                    auto caps = adapter->get_capabilities();
                    if (caps) {
                        mouse.set_capabilities(caps.value());
                    }
                }
            }

            for (const auto& iface : ifaces) {
                mouse.add_interface(iface);
            }

            result.push_back(std::move(mouse));
        }
    }

    MC_INFO("Discovered {} logical mouse device(s)", result.size());
    return result;
}

} // namespace mc::app::use_cases
