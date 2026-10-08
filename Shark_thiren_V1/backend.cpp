// backend.cpp
#include "backend.h"
#include "state.h"
#include <SPI.h>
#include <SD.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>

#define SPI_SCK 4
#define SPI_MISO 5
#define SPI_MOSI 6
#define SD_CS 7
#define TFT_CS 10

// 1. PCAP Magic Numbers & Headers
const uint32_t PCAP_MAGIC = 0xa1b2c3d4;
File pcapFile;

// 2. The 802.11 MAC Header Overlay
// This tells our code exactly where the data lives in the raw radio waves
typedef struct {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t addr1[6];  // Destination MAC
  uint8_t addr2[6];  // Source MAC (The one we want!)
  uint8_t addr3[6];  // BSSID
  uint16_t seq_ctrl;
} mac_header_t;

// 3. FreeRTOS Queue for safe memory handling
QueueHandle_t packetQueue;

// The structure we pass through the queue
typedef struct {
  uint32_t length;
  uint8_t payload[128];  // Truncate large packets to save RAM
} packet_t;

// --- THE RADIO CALLBACK (Runs at lightning speed) ---
void IRAM_ATTR wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (sharkState.isSniffing) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;

    // Grab the packet and put it in our queue
    packet_t temp_pkt;
    temp_pkt.length = pkt->rx_ctrl.sig_len;

    // Copy up to 128 bytes (we only need the headers for parsing anyway)
    int copy_len = (temp_pkt.length > 128) ? 128 : temp_pkt.length;
    memcpy(temp_pkt.payload, pkt->payload, copy_len);

    // Send to queue without blocking (if queue is full, drop packet)
    xQueueSendFromISR(packetQueue, &temp_pkt, NULL);
    sharkState.packetsCaptured++;
  }
}

void initBackend() {
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
  delay(100);

  if (SD.begin(SD_CS)) {
    sharkState.sdMounted = true;
    sharkState.lastLogMessage = "SD Ready.";
  }

  // Create a queue that can hold 20 packets at a time
  packetQueue = xQueueCreate(20, sizeof(packet_t));

  nvs_flash_init();
  esp_netif_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
  esp_wifi_set_promiscuous(true);
}

// --- THE PROCESSOR TASK ---
void backendTask(void *pvParameters) {
  packet_t rx_pkt;
  bool isPcapOpen = false;
  int pcapSessionNumber = 0;
  int packetsSinceLastFlush = 0;

  while (true) {
    // 1. OPENING THE PCAP (The Bulletproof Header Fix)
    if (sharkState.isSniffing && sharkState.sdMounted && !isPcapOpen) {
      String fileName;

      do {
        fileName = "/Shark_Thiren/loot/cap_" + String(pcapSessionNumber) + ".pcap";
        pcapSessionNumber++;
      } while (SD.exists(fileName));

      pcapFile = SD.open(fileName, FILE_WRITE);

      if (pcapFile) {
        // Hardcoded Byte Array guarantees the ESP32 doesn't mangle the magic numbers
        const uint8_t pcap_global_header[24] = {
          0xD4, 0xC3, 0xB2, 0xA1,  // Magic Number (Little Endian)
          0x02, 0x00,              // Major Version 2
          0x04, 0x00,              // Minor Version 4
          0x00, 0x00, 0x00, 0x00,  // GMT to local tz
          0x00, 0x00, 0x00, 0x00,  // Sigfigs
          0xFF, 0xFF, 0x00, 0x00,  // Snaplen (65535 bytes)
          0x69, 0x00, 0x00, 0x00   // LinkType 105 (IEEE 802.11)
        };

        pcapFile.write(pcap_global_header, 24);
        pcapFile.flush();  // Force write immediately so the file is never 0 bytes!

        isPcapOpen = true;
        packetsSinceLastFlush = 0;
        sharkState.lastLogMessage = "Logging: " + fileName;
      }
    }
    // 2. CLOSING THE PCAP
    else if (!sharkState.isSniffing && isPcapOpen) {
      pcapFile.close();
      isPcapOpen = false;
      sharkState.lastLogMessage = "PCAP Saved.";
    }

    // 3. WRITING THE DATA
    if (xQueueReceive(packetQueue, &rx_pkt, 0)) {
      // Parse MAC for the Screen UI
      mac_header_t *mac_hdr = (mac_header_t *)rx_pkt.payload;
      char macStr[18];
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
               mac_hdr->addr2[0], mac_hdr->addr2[1], mac_hdr->addr2[2],
               mac_hdr->addr2[3], mac_hdr->addr2[4], mac_hdr->addr2[5]);
      sharkState.latestMac = String(macStr);

      // Write to SD Card
      if (isPcapOpen) {
        uint32_t ts_sec = millis() / 1000;
        uint32_t ts_usec = (millis() % 1000) * 1000;
        uint32_t incl_len = (rx_pkt.length > 128) ? 128 : rx_pkt.length;
        uint32_t orig_len = rx_pkt.length;

        // Format Packet Header cleanly into an array
        uint8_t pkt_head[16];
        memcpy(pkt_head, &ts_sec, 4);
        memcpy(pkt_head + 4, &ts_usec, 4);
        memcpy(pkt_head + 8, &incl_len, 4);
        memcpy(pkt_head + 12, &orig_len, 4);

        pcapFile.write(pkt_head, 16);
        pcapFile.write(rx_pkt.payload, incl_len);

        // Flush every 25 packets (more aggressive saving)
        packetsSinceLastFlush++;
        if (packetsSinceLastFlush >= 25) {
          pcapFile.flush();
          packetsSinceLastFlush = 0;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}