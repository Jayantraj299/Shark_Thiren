#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

struct BLETracker {
    char mac[18];
    char name[30];
    int rssi;
    bool suspicious;
};

#define MAX_BLE_DEVICES 15
BLETracker bleNetworks[MAX_BLE_DEVICES];
int bleCount = 0;
BLEScan* pBLEScan;
bool bleInitialized = false;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String mac = advertisedDevice.getAddress().toString().c_str();
        String name = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "<Hidden>";
        int rssi = advertisedDevice.getRSSI();

        // SKIMMER DETECT: Flag generic serial modules often wired to skimmers
        bool isSus = false;
        if (name.indexOf("HC-0") >= 0 || name.indexOf("JDY-") >= 0 || name.indexOf("BT04") >= 0) {
            isSus = true;
        }

        bool exists = false;
        for (int i = 0; i < bleCount; i++) {
            if (String(bleNetworks[i].mac) == mac) {
                exists = true;
                bleNetworks[i].rssi = rssi;
                break;
            }
        }

        if (!exists && bleCount < MAX_BLE_DEVICES) {
            strncpy(bleNetworks[bleCount].mac, mac.c_str(), 17);
            bleNetworks[bleCount].mac[17] = '\0';
            strncpy(bleNetworks[bleCount].name, name.c_str(), 29);
            bleNetworks[bleCount].name[29] = '\0';
            bleNetworks[bleCount].rssi = rssi;
            bleNetworks[bleCount].suspicious = isSus;
            bleCount++;
        }
    }
};

void startBLEScanner() {
    if (!bleInitialized) {
        BLEDevice::init(""); // Load Bluetooth into memory dynamically
        pBLEScan = BLEDevice::getScan(); 
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
        pBLEScan->setActiveScan(true); 
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99); 
        bleInitialized = true;
    }
    bleCount = 0;
    pBLEScan->start(0, nullptr, false); 
}

void stopBLEScanner() {
    pBLEScan->stop();
    pBLEScan->clearResults(); 
}