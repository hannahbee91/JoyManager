#include "BleManager.h"
#include <iostream>

BleManager::BleManager() {
}

BleManager::~BleManager() {
    disconnect();
}

void BleManager::initialize() {
    adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        std::cerr << "No Bluetooth adapters found" << std::endl;
        return;
    }
    selectedAdapter = adapters[0];
}

void BleManager::startScan(DeviceFoundCallback callback) {
    if (!selectedAdapter.initialized()) return;

    try {
        peripherals.clear(); // Clear old results
        onDeviceFound = callback;
        selectedAdapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
            if (peripheral.is_connectable()) {
                peripherals[peripheral.address()] = peripheral;
                if (onDeviceFound) {
                    onDeviceFound(peripheral.identifier(), peripheral.address());
                }
            }
        });
        
        selectedAdapter.scan_start();
    } catch (const std::exception& e) {
        std::cerr << "Exception in startScan: " << e.what() << std::endl;
    }
}

void BleManager::stopScan() {
    if (selectedAdapter.initialized()) {
        try {
            selectedAdapter.scan_stop();
        } catch (...) {
            // Ignore if scan was not running
        }
    }
}

bool BleManager::connect(const std::string& address) {
    if (!selectedAdapter.initialized()) return false;
    
    stopScan();
    
    auto it = peripherals.find(address);
    if (it == peripherals.end()) return false;

    try {
        selectedPeripheral = it->second;
        selectedPeripheral.connect();
        
        if (selectedPeripheral.is_connected()) {
            // Subscribe to RX
            selectedPeripheral.notify(Pixl::SERVICE_UUID, Pixl::TX_CHAR_UUID, [this](SimpleBLE::ByteArray bytes) {
                std::vector<uint8_t> data(bytes.begin(), bytes.end());
                if (onDataReceived) {
                    onDataReceived(data);
                }
            });
            
            selectedPeripheral.set_callback_on_disconnected([this]() {
                if (onDisconnected) {
                    onDisconnected();
                }
            });
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in connect: " << e.what() << std::endl;
    }

    return false;
}

void BleManager::disconnect() {
    if (selectedPeripheral.initialized() && selectedPeripheral.is_connected()) {
        selectedPeripheral.disconnect();
    }
}

bool BleManager::isConnected() {
    return selectedPeripheral.initialized() && selectedPeripheral.is_connected();
}

void BleManager::sendCommand(Pixl::Command cmd, const std::vector<uint8_t>& payload) {
    if (!isConnected()) return;

    auto packet = Pixl::Protocol::createPacket(cmd, payload);
    
    // Send to TX Characteristic
    selectedPeripheral.write_request(Pixl::SERVICE_UUID, Pixl::RX_CHAR_UUID, 
                                     std::string(packet.begin(), packet.end()));
}

void BleManager::setDataReceivedCallback(DataReceivedCallback callback) {
    onDataReceived = callback;
}

void BleManager::setDisconnectedCallback(DisconnectedCallback callback) {
    onDisconnected = callback;
}
