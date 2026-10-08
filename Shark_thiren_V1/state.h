#ifndef STATE_H
#define STATE_H
#include <Arduino.h>

struct SystemState {
    bool sdMounted = false;
    bool isSniffing = false;
    bool isScanningBLE = false; 
    bool isPlayingGame = false;
    bool showingSysInfo = false;
    bool showingSdInfo = false;
    bool inSettingsMenu = false;
    bool inRomMenu = false;
    bool inWifiMenu = false;       // NEW: Wi-Fi Folder State
    bool inBluetoothMenu = false;  // NEW: Bluetooth Folder State
    bool isAppleSpoofing = false;
    
    int currentMenuIndex = 0;
    int settingsMenuIndex = 0;
    int romMenuIndex = 0;
    int wifiMenuIndex = 0;         // NEW: Wi-Fi Cursor
    int bluetoothMenuIndex = 0;    // NEW: Bluetooth Cursor
    
    int romCount = 0;
    String romList[30];
    String lastLogMessage = "";
    
    // Restored variables for your backend.cpp PCAP logging
    unsigned long packetsCaptured = 0;
    String latestMac = "";
};

extern SystemState sharkState;

#endif