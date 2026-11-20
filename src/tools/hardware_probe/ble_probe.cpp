#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

void print_bytes(const std::string& prefix, IBuffer const& buffer) {
    auto reader = DataReader::FromBuffer(buffer);
    std::cout << prefix << " (" << buffer.Length() << " bytes): ";
    for (uint32_t i = 0; i < buffer.Length(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(reader.ReadByte()) << " ";
    }
    std::cout << std::dec << std::endl;
}

std::string read_string(IBuffer const& buffer) {
    auto reader = DataReader::FromBuffer(buffer);
    std::vector<uint8_t> data(buffer.Length());
    reader.ReadBytes(data);
    return std::string(data.begin(), data.end());
}

int main() {
    init_apartment();
    std::cout << "====================================================" << std::endl;
    std::cout << "  Ajazz AJ159 BLE Hardware Protocol Explorer" << std::endl;
    std::cout << "====================================================" << std::endl;

    uint64_t addr = 0xD109BD99A86FULL;
    std::cout << "Connecting to device 0x" << std::hex << addr << std::dec << "..." << std::endl;

    auto ble_device = BluetoothLEDevice::FromBluetoothAddressAsync(addr).get();
    if (!ble_device) {
        std::cerr << "Failed to find/connect to BLE device." << std::endl;
        return 1;
    }

    std::cout << "Device Name: " << to_string(ble_device.Name()) << std::endl;
    std::cout << "Connection Status: " << (ble_device.ConnectionStatus() == BluetoothConnectionStatus::Connected ? "Connected" : "Disconnected") << std::endl;

    auto services_result = ble_device.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
    if (services_result.Status() != GattCommunicationStatus::Success) {
        services_result = ble_device.GetGattServicesAsync(BluetoothCacheMode::Cached).get();
    }

    if (services_result.Status() != GattCommunicationStatus::Success) {
        std::cerr << "Failed to query GATT services. Status: " << static_cast<int>(services_result.Status()) << std::endl;
        return 1;
    }

    auto services = services_result.Services();
    std::cout << "\nDiscovered " << services.Size() << " GATT Services:" << std::endl;

    for (auto service : services) {
        guid service_uuid = service.Uuid();
        std::cout << "\n----------------------------------------------------" << std::endl;
        std::cout << "Service UUID: " << to_string(to_hstring(service_uuid)) << std::endl;

        auto chars_result = service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached).get();
        if (chars_result.Status() != GattCommunicationStatus::Success) {
            chars_result = service.GetCharacteristicsAsync(BluetoothCacheMode::Cached).get();
        }

        if (chars_result.Status() == GattCommunicationStatus::Success) {
            for (auto ch : chars_result.Characteristics()) {
                guid char_uuid = ch.Uuid();
                auto props = ch.CharacteristicProperties();
                std::cout << "  Characteristic: " << to_string(to_hstring(char_uuid)) 
                          << " [Props: 0x" << std::hex << static_cast<uint32_t>(props) << std::dec << "]" << std::endl;

                if ((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Read)) != 0) {
                    try {
                        auto read_res = ch.ReadValueAsync(BluetoothCacheMode::Uncached).get();
                        if (read_res.Status() == GattCommunicationStatus::Success) {
                            auto val = read_res.Value();
                            print_bytes("    Value", val);

                            // Decode specific well-known values
                            if (char_uuid == GattCharacteristicUuids::BatteryLevel()) {
                                auto r = DataReader::FromBuffer(val);
                                std::cout << "    >>> Battery Level: " << static_cast<int>(r.ReadByte()) << "%" << std::endl;
                            }
                            // String characteristics (e.g. Model, Manufacturer, Serial, etc.)
                            if (val.Length() > 0 && val.Length() < 64) {
                                std::string str = read_string(val);
                                bool is_printable = true;
                                for (char c : str) {
                                    if (c != 0 && (c < 32 || c > 126)) { is_printable = false; break; }
                                }
                                if (is_printable && !str.empty()) {
                                    std::cout << "    >>> String: \"" << str << "\"" << std::endl;
                                }
                            }
                        } else {
                            std::cout << "    Read failed with status: " << static_cast<int>(read_res.Status()) << std::endl;
                        }
                    } catch (const hresult_error& ex) {
                        std::cout << "    Exception reading: 0x" << std::hex << ex.code() << std::dec << " - " << to_string(ex.message()) << std::endl;
                    }
                }
            }
        }
    }

    std::cout << "\n====================================================" << std::endl;
    return 0;
}
