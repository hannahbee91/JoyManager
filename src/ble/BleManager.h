#pragma once

#include <simpleble/SimpleBLE.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include "PixlProtocol.h"

class BleManager {
public:
    using DeviceFoundCallback = std::function<void(const std::string& name, const std::string& address)>;
    using DataReceivedCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using DisconnectedCallback = std::function<void()>;

    BleManager();
    ~BleManager();

    void initialize();
    void startScan(DeviceFoundCallback callback);
    void stopScan();
    
    bool connect(const std::string& address);
    void disconnect();
    bool isConnected();

    void sendCommand(Pixl::Command cmd, const std::vector<uint8_t>& payload = {});
    
    void setDataReceivedCallback(DataReceivedCallback callback);
    void setDisconnectedCallback(DisconnectedCallback callback);

private:
    std::vector<SimpleBLE::Adapter> adapters;
    SimpleBLE::Adapter selectedAdapter;
    SimpleBLE::Peripheral selectedPeripheral;
    std::map<std::string, SimpleBLE::Peripheral> peripherals; // Map address -> peripheral
    
    DeviceFoundCallback onDeviceFound;
    DataReceivedCallback onDataReceived;
    DisconnectedCallback onDisconnected;
};
