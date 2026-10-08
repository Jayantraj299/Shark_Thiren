#include "tui.h"
#include "state.h"
#include "chip8.h" 
#include "mascot.h" 
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SD.h> 

extern uint8_t delay_timer;
extern uint8_t sound_timer; 

// Wi-Fi Externs
extern void startSniffing();
extern void stopSniffing();
extern void hopChannel(); 
extern struct WiFiNode {
    char ssid[33];
    int rssi;
    uint8_t channel;
} scannedNetworks[];
extern int networkCount;

// BLE Recon Externs
extern void startBLEScanner();
extern void stopBLEScanner();
extern struct BLETracker {
    char mac[18];
    char name[30];
    int rssi;
    bool suspicious;
} bleNetworks[];
extern int bleCount;

// Apple Spoofer Externs
extern void startAppleSpoofer();
extern void stopAppleSpoofer();

#define TFT_CS 10
#define TFT_DC 9   
#define TFT_RST -1 

const byte ROWS = 3; 
const byte COLS = 3; 

byte rowPins[ROWS] = {20, 1, 2}; 
byte colPins[COLS] = {3, 21, 8}; 

const char* keyMap[ROWS][COLS] = {
  {"LEFT",   "UP",    "A"}, 
  {"SELECT", "RIGHT", "B"}, 
  {"DOWN",   "D",     "C"}  
};

bool currentKeyState[ROWS][COLS];
bool lastKeyState[ROWS][COLS];
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 30; 

bool inDpadTester = false;
String lastTestedKey = "";
int lastTestedRow = -1;
int lastTestedCol = -1;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// --- MENU STRUCTURE ---
const int MAX_MENU_ITEMS = 5;
String menuItems[MAX_MENU_ITEMS] = {"Wi-Fi", "Bluetooth", "Applications", "Games", "Settings"};

const int MAX_WIFI_ITEMS = 2;
String wifiItems[MAX_WIFI_ITEMS] = {"<- BACK", "Wi-Fi Sniffer"};

const int MAX_BT_ITEMS = 3; 
String btItems[MAX_BT_ITEMS] = {"<- BACK", "BLE Recon", "Apple Spoofer"};

const int MAX_SETTINGS_ITEMS = 6; 
String settingsItems[MAX_SETTINGS_ITEMS] = {"<- BACK", "Set Time", "SD Card", "System Info", "SD Info", "D-Pad Tester"};

int lastMenuIndex = -1;
bool forceRedraw = true; 
uint8_t old_gfx[2048]; 

void drawStatusBar() {
    tft.fillRect(0, 0, 128, 12, ST77XX_BLACK); 
    tft.setTextSize(1);
    tft.setCursor(2, 2);
    
    if (sharkState.sdMounted) {
        tft.setTextColor(ST77XX_GREEN);
        tft.print("[SD]");
    } else {
        tft.setTextColor(ST77XX_RED);
        tft.print("[NO SD]");
    }
    
    if (sharkState.isSniffing) {
        tft.setTextColor(ST77XX_RED);
        tft.print(" [REC]");
    } else if (sharkState.isScanningBLE) {
        tft.setTextColor(ST77XX_BLUE);
        tft.print(" [BLE]");
    } else if (sharkState.isAppleSpoofing) {
        tft.setTextColor(ST77XX_GREEN);
        tft.print(" [SPF]");
    } else {
        tft.setTextColor(ST77XX_CYAN);
        tft.print(" [IDLE]");
    }
    
    int rawValue = analogRead(0); 
    int voltage_x100 = (rawValue * 660) / 4095;
    int batteryPercent = 0;
    
    if (voltage_x100 >= 420) batteryPercent = 100;
    else if (voltage_x100 <= 320) batteryPercent = 0;
    else batteryPercent = ((voltage_x100 - 320) * 100) / (420 - 320);

    if (batteryPercent > 50) tft.setTextColor(ST77XX_GREEN);
    else if (batteryPercent > 20) tft.setTextColor(ST77XX_YELLOW);
    else tft.setTextColor(ST77XX_RED);
    
    tft.setCursor(90, 2); 
    tft.print("[");
    if (batteryPercent < 100) tft.print(" ");
    if (batteryPercent < 10) tft.print(" ");
    tft.print(batteryPercent);
    tft.print("%]");
    
    tft.drawLine(0, 12, 128, 12, ST77XX_WHITE); 
}

void initTUI() {
    for (int r = 0; r < ROWS; r++) {
        pinMode(rowPins[r], INPUT_PULLUP);
        for (int c = 0; c < COLS; c++) {
            currentKeyState[r][c] = false;
            lastKeyState[r][c] = false;
        }
    }
    
    for (int c = 0; c < COLS; c++) {
        pinMode(colPins[c], OUTPUT);
        digitalWrite(colPins[c], HIGH);
    }
    
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0); 
    tft.fillScreen(ST77XX_BLACK);
    forceRedraw = true;

    analogRead(0); 
}

void tuiTask(void *pvParameters) {
    while (true) {
        bool inputDetected = false;
        String newlyPressedKey = "";
        unsigned long currentMillis = millis();

        if ((currentMillis - lastDebounceTime) > debounceDelay) {
            for (int c = 0; c < COLS; c++) {
                pinMode(colPins[c], OUTPUT); 
                digitalWrite(colPins[c], LOW);
                delayMicroseconds(20); 

                for (int r = 0; r < ROWS; r++) {
                    currentKeyState[r][c] = (digitalRead(rowPins[r]) == LOW);

                    if (currentKeyState[r][c] != lastKeyState[r][c]) {
                        if (currentKeyState[r][c] == true) {
                            newlyPressedKey = String(keyMap[r][c]);
                            inputDetected = true;
                            
                            if (inDpadTester) {
                                lastTestedRow = r;
                                lastTestedCol = c;
                                lastTestedKey = newlyPressedKey;
                                forceRedraw = true;
                            }
                        }
                        lastKeyState[r][c] = currentKeyState[r][c];
                    }
                }
                pinMode(colPins[c], INPUT); 
            }
            lastDebounceTime = currentMillis;
        }
        
        if (inDpadTester) {
            if (inputDetected && newlyPressedKey == "SELECT") {
                vTaskDelay(pdMS_TO_TICKS(300));
                inDpadTester = false;
                sharkState.inSettingsMenu = true;
                forceRedraw = true;
                lastTestedRow = -1;
                lastTestedKey = "";
            }
        }
        else if (newlyPressedKey == "DOWN") {
            if (sharkState.showingSysInfo || sharkState.showingSdInfo || sharkState.isSniffing || sharkState.isScanningBLE || sharkState.isAppleSpoofing) {
                // Lock scrolling
            } else if (sharkState.inWifiMenu) {
                sharkState.wifiMenuIndex = (sharkState.wifiMenuIndex + 1) % MAX_WIFI_ITEMS;
                forceRedraw = true;
            } else if (sharkState.inBluetoothMenu) {
                sharkState.bluetoothMenuIndex = (sharkState.bluetoothMenuIndex + 1) % MAX_BT_ITEMS;
                forceRedraw = true;
            } else if (sharkState.inSettingsMenu) {
                sharkState.settingsMenuIndex = (sharkState.settingsMenuIndex + 1) % MAX_SETTINGS_ITEMS;
                forceRedraw = true;
            } else if (sharkState.inRomMenu) {
                sharkState.romMenuIndex = (sharkState.romMenuIndex + 1) % sharkState.romCount;
                forceRedraw = true;
            } else if (!sharkState.isPlayingGame) {
                sharkState.currentMenuIndex = (sharkState.currentMenuIndex + 1) % MAX_MENU_ITEMS;
            }
        }
        
        else if (newlyPressedKey == "UP") {
            if (sharkState.showingSysInfo || sharkState.showingSdInfo || sharkState.isSniffing || sharkState.isScanningBLE || sharkState.isAppleSpoofing) {
                // Lock scrolling
            } else if (sharkState.inWifiMenu) {
                sharkState.wifiMenuIndex = (sharkState.wifiMenuIndex - 1 + MAX_WIFI_ITEMS) % MAX_WIFI_ITEMS;
                forceRedraw = true;
            } else if (sharkState.inBluetoothMenu) {
                sharkState.bluetoothMenuIndex = (sharkState.bluetoothMenuIndex - 1 + MAX_BT_ITEMS) % MAX_BT_ITEMS;
                forceRedraw = true;
            } else if (sharkState.inSettingsMenu) {
                sharkState.settingsMenuIndex = (sharkState.settingsMenuIndex - 1 + MAX_SETTINGS_ITEMS) % MAX_SETTINGS_ITEMS;
                forceRedraw = true;
            } else if (sharkState.inRomMenu) {
                sharkState.romMenuIndex = (sharkState.romMenuIndex - 1 + sharkState.romCount) % sharkState.romCount;
                forceRedraw = true;
            } else if (!sharkState.isPlayingGame) {
                sharkState.currentMenuIndex = (sharkState.currentMenuIndex - 1 + MAX_MENU_ITEMS) % MAX_MENU_ITEMS;
            }
        }
        
        else if (newlyPressedKey == "SELECT") {
            if (sharkState.showingSysInfo || sharkState.showingSdInfo) {
                sharkState.showingSysInfo = false;
                sharkState.showingSdInfo = false;
                forceRedraw = true;
            }
            else if (sharkState.isSniffing) {
                stopSniffing();
                sharkState.isSniffing = false;
                forceRedraw = true;
            }
            else if (sharkState.isScanningBLE) {
                stopBLEScanner();
                sharkState.isScanningBLE = false;
                forceRedraw = true;
            }
            else if (sharkState.isAppleSpoofing) {
                stopAppleSpoofer();
                sharkState.isAppleSpoofing = false;
                forceRedraw = true;
            }
            else if (sharkState.isPlayingGame) {
                sharkState.isPlayingGame = false;
                forceRedraw = true;
            } 
            else if (sharkState.inWifiMenu) {
                if (sharkState.wifiMenuIndex == 0) {
                    sharkState.inWifiMenu = false;
                    forceRedraw = true;
                } else if (sharkState.wifiMenuIndex == 1) {
                    sharkState.isSniffing = true;
                    startSniffing();
                    forceRedraw = true;
                }
            }
            else if (sharkState.inBluetoothMenu) {
                if (sharkState.bluetoothMenuIndex == 0) {
                    sharkState.inBluetoothMenu = false;
                    forceRedraw = true;
                } else if (sharkState.bluetoothMenuIndex == 1) {
                    sharkState.isScanningBLE = true;
                    startBLEScanner();
                    forceRedraw = true;
                } else if (sharkState.bluetoothMenuIndex == 2) {
                    sharkState.isAppleSpoofing = true;
                    forceRedraw = true;
                }
            }
            else if (sharkState.inSettingsMenu) {
                switch(sharkState.settingsMenuIndex) {
                    case 0: 
                        sharkState.inSettingsMenu = false;
                        forceRedraw = true; 
                        break;
                    case 1: 
                        sharkState.lastLogMessage = "RTC Sync needed.";
                        break;
                    case 2: 
                        if (sharkState.sdMounted) {
                            SD.end(); 
                            sharkState.sdMounted = false;
                        } else {
                            if (SD.begin(7)) { sharkState.sdMounted = true; } 
                        }
                        forceRedraw = true;
                        break;
                    case 3: 
                        sharkState.showingSysInfo = true;
                        forceRedraw = true;
                        break;
                    case 4: 
                        sharkState.showingSdInfo = true;
                        forceRedraw = true;
                        break;
                    case 5: 
                        inDpadTester = true;
                        sharkState.inSettingsMenu = false;
                        forceRedraw = true;
                        break;
                }
            }
            else if (sharkState.inRomMenu) {
                if (sharkState.romMenuIndex == 0) { 
                    sharkState.inRomMenu = false;
                    forceRedraw = true;
                } else {
                    String targetFile = "/Shark_Thiren/games/" + sharkState.romList[sharkState.romMenuIndex];
                    if (loadROM(targetFile)) {
                        sharkState.isPlayingGame = true;
                        sharkState.inRomMenu = false;
                        tft.fillScreen(ST77XX_BLACK); 
                        memset(old_gfx, 255, 2048); 
                    } else {
                        sharkState.lastLogMessage = "ROM Load Failed!";
                        sharkState.inRomMenu = false;
                        forceRedraw = true;
                    }
                }
            } 
            else {
                // MAIN MENU ACTIONS
                if (sharkState.currentMenuIndex == 0) {
                    sharkState.inWifiMenu = true;
                    sharkState.wifiMenuIndex = 0;
                    forceRedraw = true;
                } 
                else if (sharkState.currentMenuIndex == 1) { 
                    sharkState.inBluetoothMenu = true;
                    sharkState.bluetoothMenuIndex = 0;
                    forceRedraw = true;
                }
                else if (sharkState.currentMenuIndex == 3) { 
                    sharkState.inRomMenu = true;
                    sharkState.romMenuIndex = 0;
                    sharkState.romCount = 0;
                    sharkState.romList[sharkState.romCount++] = "<- BACK"; 
                    
                    File dir = SD.open("/Shark_Thiren/games");
                    if (dir) {
                        while (true) {
                            File entry = dir.openNextFile();
                            if (!entry || sharkState.romCount >= 30) break;
                            if (!entry.isDirectory()) {
                                sharkState.romList[sharkState.romCount++] = String(entry.name());
                            }
                            entry.close();
                        }
                        dir.close();
                    }
                    forceRedraw = true;
                }
                else if (sharkState.currentMenuIndex == 4) {
                    sharkState.inSettingsMenu = true;
                    sharkState.settingsMenuIndex = 0;
                    forceRedraw = true;
                }
            }
        }

        // --- DRAWING & EXECUTION LOGIC ---
        
        if (sharkState.isSniffing) {
            if (forceRedraw) {
                tft.fillScreen(ST77XX_BLACK);
                drawStatusBar();
                
                tft.setCursor(0, 15);
                tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
                tft.print("--- WI-FI SNIFFER ---");
                
                tft.setCursor(0, 115);
                tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
                tft.print("[Press SEL to Stop]");
                
                forceRedraw = false;
            }

            // Dot animation logic (ticks every ~300ms)
            static int dotPhase = 0;
            String dots = (dotPhase == 0) ? "   " : ((dotPhase == 1) ? ".  " : ((dotPhase == 2) ? ".. " : "..."));
            dotPhase = (dotPhase + 1) % 4;

            tft.setCursor(0, 27);
            tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
            // Pad the string with spaces to act as an eraser over old characters
            char apStr[20];
            sprintf(apStr, "Found: %-3d APs%s", networkCount, dots.c_str());
            tft.print(apStr);

            int displayLimit = networkCount < 4 ? networkCount : 4;
            for (int i = 0; i < displayLimit; i++) {
                int yPos = 42 + (i * 16); 
                
                tft.setCursor(0, yPos);
                tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                String ssidStr = String(scannedNetworks[i].ssid);
                if (ssidStr.length() > 10) ssidStr = ssidStr.substring(0, 10);
                while (ssidStr.length() < 10) ssidStr += " "; // Eraser padding
                tft.print(ssidStr);
                
                tft.setCursor(80, yPos);
                tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
                char rssiStr[10];
                sprintf(rssiStr, "%-4ddB ", scannedNetworks[i].rssi);
                tft.print(rssiStr);
            }
            
            hopChannel(); 
            vTaskDelay(pdMS_TO_TICKS(300)); 
            continue;
        }
        else if (sharkState.isScanningBLE) {
            if (forceRedraw) {
                tft.fillScreen(ST77XX_BLACK);
                drawStatusBar();
                
                tft.setCursor(0, 15);
                tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
                tft.print("--- BLE RECON ---");
                
                tft.setCursor(0, 115);
                tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
                tft.print("[Press SEL to Stop]");
                
                forceRedraw = false;
            }

            // Dot animation logic (ticks every ~500ms)
            static int dotPhase = 0;
            String dots = (dotPhase == 0) ? "   " : ((dotPhase == 1) ? ".  " : ((dotPhase == 2) ? ".. " : "..."));
            dotPhase = (dotPhase + 1) % 4;

            tft.setCursor(0, 27);
            tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
            char targetStr[20];
            sprintf(targetStr, "Targets: %-3d%s", bleCount, dots.c_str());
            tft.print(targetStr);

            int displayLimit = bleCount < 4 ? bleCount : 4;
            for (int i = 0; i < displayLimit; i++) {
                int yPos = 42 + (i * 16); 
                
                tft.setCursor(0, yPos);
                if (bleNetworks[i].suspicious) {
                    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
                } else {
                    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                }
                
                String nameStr = String(bleNetworks[i].name);
                if (nameStr.length() > 9) nameStr = nameStr.substring(0, 9);
                while (nameStr.length() < 9) nameStr += " "; // Eraser padding
                tft.print(nameStr);
                
                tft.setCursor(65, yPos);
                tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
                char rssiStr[10];
                sprintf(rssiStr, "%-4ddB ", bleNetworks[i].rssi);
                tft.print(rssiStr);
            }
            
            vTaskDelay(pdMS_TO_TICKS(500)); 
            continue;
        }
        else if (sharkState.isAppleSpoofing) {
            if (forceRedraw) {
                tft.fillScreen(ST77XX_BLACK);
                drawStatusBar();
                
                tft.setCursor(0, 15);
                tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
                tft.print("--- APPLE SPOOFER ---");
                
                tft.setCursor(0, 45);
                tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                tft.print("Broadcasting Fake");
                
                tft.setCursor(0, 115);
                tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
                tft.print("[Press SEL to Stop]");
                
                forceRedraw = false;
            }

            // Dot animation logic decoupled from the 50ms broadcast delay
            static int tickCount = 0;
            static int dotPhase = 0;
            if (tickCount % 10 == 0) { // Updates the animation every ~500ms
                String dots = (dotPhase == 0) ? "   " : ((dotPhase == 1) ? ".  " : ((dotPhase == 2) ? ".. " : "..."));
                dotPhase = (dotPhase + 1) % 4;
                
                tft.setCursor(0, 60);
                tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                tft.print("Apple Devices" + dots);
            }
            tickCount++;

            startAppleSpoofer();
            vTaskDelay(pdMS_TO_TICKS(50)); 
            continue;
        }
        else if (sharkState.inWifiMenu) {
            if (forceRedraw || inputDetected) {
                if (forceRedraw) {
                    tft.fillScreen(ST77XX_BLACK);
                    drawStatusBar();
                    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
                    tft.setCursor(0, 15); 
                    tft.println("--- WI-FI ---");
                    tft.drawLine(0, 25, 128, 25, ST77XX_CYAN);
                    forceRedraw = false;
                }
                for (int i = 0; i < MAX_WIFI_ITEMS; i++) {
                    int yPos = 32 + (i * 15); 
                    tft.setCursor(0, yPos);
                    String displayText = wifiItems[i];
                    while (displayText.length() < 18) displayText += " "; 
                    if (i == sharkState.wifiMenuIndex) {
                        tft.fillRect(0, yPos - 2, 128, 15, ST77XX_CYAN); 
                        tft.setTextColor(ST77XX_BLACK);
                    } else {
                        tft.fillRect(0, yPos - 2, 128, 15, ST77XX_BLACK);
                        tft.setTextColor(ST77XX_WHITE);
                    }
                    tft.setCursor(10, yPos);
                    tft.print(displayText);
                }
            }
        }
        else if (sharkState.inBluetoothMenu) {
            if (forceRedraw || inputDetected) {
                if (forceRedraw) {
                    tft.fillScreen(ST77XX_BLACK);
                    drawStatusBar();
                    tft.setTextColor(ST77XX_BLUE, ST77XX_BLACK);
                    tft.setCursor(0, 15); 
                    tft.println("--- BLUETOOTH ---");
                    tft.drawLine(0, 25, 128, 25, ST77XX_BLUE);
                    forceRedraw = false;
                }
                for (int i = 0; i < MAX_BT_ITEMS; i++) {
                    int yPos = 32 + (i * 15); 
                    tft.setCursor(0, yPos);
                    String displayText = btItems[i];
                    while (displayText.length() < 18) displayText += " "; 
                    if (i == sharkState.bluetoothMenuIndex) {
                        tft.fillRect(0, yPos - 2, 128, 15, ST77XX_CYAN); 
                        tft.setTextColor(ST77XX_BLACK);
                    } else {
                        tft.fillRect(0, yPos - 2, 128, 15, ST77XX_BLACK);
                        tft.setTextColor(ST77XX_WHITE);
                    }
                    tft.setCursor(10, yPos);
                    tft.print(displayText);
                }
            }
        }
        else if (inDpadTester) {
            if (forceRedraw) {
                tft.fillScreen(ST77XX_BLACK);
                drawStatusBar();
                
                tft.setCursor(0, 15); 
                tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
                tft.println("--- D-PAD TESTER ---\n");
                
                tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                tft.println("Press any button.");
                tft.println("Find 'SELECT' to exit.\n");
                
                if (lastTestedRow != -1) {
                    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
                    tft.print("Matrix: [");
                    tft.print(lastTestedRow);
                    tft.print("][");
                    tft.print(lastTestedCol);
                    tft.println("]");
                    
                    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
                    tft.print("\nAction: ");
                    tft.println(lastTestedKey);
                }
                forceRedraw = false;
            }
        }
        else if (sharkState.showingSysInfo) {
            if (forceRedraw) {
                tft.fillScreen(ST77XX_BLACK);
                drawStatusBar();
                tft.setCursor(0, 15); 
                tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
                tft.println("--- SYSTEM INFO ---\n");
                tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                tft.print("CPU Freq: "); tft.print(ESP.getCpuFreqMHz()); tft.println(" MHz");
                tft.print("Free RAM: "); tft.print(ESP.getFreeHeap() / 1024); tft.println(" KB");
                tft.println("\n[Press SEL to Exit]");
                forceRedraw = false;
            }
        }
        else if (sharkState.showingSdInfo) {
            if (forceRedraw) {
                tft.fillScreen(ST77XX_BLACK);
                drawStatusBar();
                tft.setCursor(0, 15); 
                tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
                tft.println("--- SD CARD INFO ---\n");
                if (sharkState.sdMounted) {
                    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                    tft.print("Size:   "); tft.print((uint32_t)(SD.cardSize() / (1024 * 1024))); tft.println(" MB");
                } else {
                    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
                    tft.println("ERROR: No Card.");
                }
                tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
                tft.println("\n[Press SEL to Exit]");
                forceRedraw = false;
            }
        }
        else if (sharkState.isPlayingGame) {
            for (int i = 0; i < 16; i++) {
                keys[i] = 0; 
            }
            
            keys[0x2] = currentKeyState[0][1]; 
            keys[0x8] = currentKeyState[2][0]; 
            keys[0x4] = currentKeyState[0][0]; 
            keys[0x6] = currentKeyState[1][1]; 
            
            keys[0x5] = currentKeyState[0][2]; 
            keys[0xF] = currentKeyState[1][2]; 
            keys[0x7] = currentKeyState[2][2]; 
            keys[0x9] = currentKeyState[2][1]; 
            
            for(int i = 0; i < 8; i++) {
                emulateCycle();
            }

            if (delay_timer > 0) delay_timer--;
            if (sound_timer > 0) sound_timer--;

            if (drawFlag) {
                for (int i = 0; i < 2048; ++i) {
                    if (gfx[i] != old_gfx[i]) {
                        int x = i % 64;
                        int y = i / 64;
                        uint16_t color = (gfx[i] == 1) ? ST77XX_WHITE : ST77XX_BLACK;
                        tft.fillRect(x * 2, (y * 2) + 15, 2, 2, color); 
                        old_gfx[i] = gfx[i];
                    }
                }
                drawFlag = false;
            }
            
            vTaskDelay(pdMS_TO_TICKS(16)); 
            continue; 
        } 
        else if (sharkState.inSettingsMenu) {
            if (forceRedraw || inputDetected) {
                if (forceRedraw) {
                    tft.fillScreen(ST77XX_BLACK);
                    drawStatusBar();
                    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
                    tft.setCursor(0, 15); 
                    tft.println("--- SETTINGS ---");
                    tft.drawLine(0, 25, 128, 25, ST77XX_GREEN);
                    forceRedraw = false;
                }
                settingsItems[2] = sharkState.sdMounted ? "SD Card: MOUNTED" : "SD Card: EJECTED";
                
                int startIdx = sharkState.settingsMenuIndex - 3;
                if (startIdx < 0) startIdx = 0;
                if (startIdx > MAX_SETTINGS_ITEMS - 6) startIdx = MAX_SETTINGS_ITEMS - 6;

                for (int i = 0; i < 6; i++) {
                    int actualIdx = startIdx + i;
                    int yPos = 32 + (i * 15); 
                    tft.setCursor(0, yPos);
                    
                    if (actualIdx < MAX_SETTINGS_ITEMS) {
                        String displayText = settingsItems[actualIdx];
                        while (displayText.length() < 18) displayText += " "; 
                        if (actualIdx == sharkState.settingsMenuIndex) {
                            tft.fillRect(0, yPos - 2, 128, 15, ST77XX_CYAN); 
                            tft.setTextColor(ST77XX_BLACK);
                        } else {
                            tft.fillRect(0, yPos - 2, 128, 15, ST77XX_BLACK);
                            tft.setTextColor(ST77XX_WHITE);
                        }
                        tft.setCursor(10, yPos);
                        tft.print(displayText);
                    }
                }
            }
        }
        else if (sharkState.inRomMenu) {
            if (forceRedraw || inputDetected) {
                if (forceRedraw) {
                    tft.fillScreen(ST77XX_BLACK);
                    drawStatusBar();
                    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
                    tft.setCursor(0, 15); 
                    tft.println("--- SELECT ROM ---");
                    tft.drawLine(0, 25, 128, 25, ST77XX_GREEN);
                    forceRedraw = false;
                }
                
                int startIdx = sharkState.romMenuIndex - 3;
                if (startIdx < 0) startIdx = 0;

                for (int i = 0; i < 6; i++) {
                    int actualIdx = startIdx + i;
                    int yPos = 32 + (i * 15); 
                    tft.setCursor(0, yPos);
                    
                    if (actualIdx < sharkState.romCount) {
                        String displayText = sharkState.romList[actualIdx];
                        if (displayText.length() > 14) displayText = displayText.substring(0, 14);
                        while (displayText.length() < 16) displayText += " ";

                        if (actualIdx == sharkState.romMenuIndex) {
                            tft.fillRect(0, yPos - 2, 128, 15, ST77XX_CYAN); 
                            tft.setTextColor(ST77XX_BLACK);
                        } else {
                            tft.fillRect(0, yPos - 2, 128, 15, ST77XX_BLACK);
                            tft.setTextColor(ST77XX_WHITE);
                        }
                        tft.setCursor(15, yPos);
                        tft.print(displayText);
                    }
                }
            }
        } 
        else {
            if (forceRedraw || inputDetected) {
                if (forceRedraw) {
                    tft.fillScreen(ST77XX_BLACK);
                    drawStatusBar();
                    tft.drawRGBBitmap(0, 15, shark_bmp, shark_bmp_width, shark_bmp_height);
                    forceRedraw = false;
                    lastMenuIndex = -1; 
                }

                if (sharkState.currentMenuIndex != lastMenuIndex) {
                    int menuStartY = 92; 
                    int itemHeight = 15; 
                    
                    int startIdx = sharkState.currentMenuIndex - 2;
                    if (startIdx < 0) startIdx = 0;

                    for (int i = 0; i < 4; i++) {
                        int actualIdx = startIdx + i;
                        int yPos = menuStartY + (i * itemHeight);
                        
                        if (actualIdx < MAX_MENU_ITEMS) {
                            if (actualIdx == sharkState.currentMenuIndex) {
                                tft.fillRect(0, yPos, 128, itemHeight, ST77XX_CYAN); 
                                tft.setTextColor(ST77XX_BLACK);
                            } else {
                                tft.fillRect(0, yPos, 128, itemHeight, ST77XX_BLACK);
                                tft.setTextColor(ST77XX_WHITE);
                            }
                            tft.setCursor(15, yPos + 3); 
                            tft.print(menuItems[actualIdx]);
                        }
                    }
                    lastMenuIndex = sharkState.currentMenuIndex;
                }
            }
        }
        
        if (!sharkState.isPlayingGame && !sharkState.isSniffing && !sharkState.isScanningBLE && !sharkState.isAppleSpoofing && (inputDetected)) {
            drawStatusBar();
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    } 
}