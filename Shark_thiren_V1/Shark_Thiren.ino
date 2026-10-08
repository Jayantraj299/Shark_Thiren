#include <Arduino.h>
#include "state.h"
#include "tui.h"
#include <esp_wifi.h>
#include <esp_netif.h>

SystemState sharkState; 

extern void initBLE(); 

void setup() {
    Serial.begin(115200);
    
    // ONLY initialize the base netif config, DO NOT turn on the radio yet
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL); 

    // Initialize the UI and hardware pins
    initTUI();

    // Launch the UI Task on Core 0 (ESP32-C3 is single-core!)
    xTaskCreatePinnedToCore(
        tuiTask, 
        "TUI Task", 
        8192, 
        NULL, 
        1, 
        NULL,
        0
    );
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
