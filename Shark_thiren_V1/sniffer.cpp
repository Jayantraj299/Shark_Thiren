#include <Arduino.h>
#include <esp_wifi.h>

struct WiFiNode {
    char ssid[33];
    int rssi;
    uint8_t channel;
};

#define MAX_NETWORKS 15
WiFiNode scannedNetworks[MAX_NETWORKS];
int networkCount = 0;

typedef struct {
    struct {
        unsigned int version:2;
        unsigned int type:2;
        unsigned int subtype:4;
        unsigned int to_ds:1;
        unsigned int from_ds:1;
        unsigned int more_frag:1;
        unsigned int retry:1;
        unsigned int pwr_mgt:1;
        unsigned int more_data:1;
        unsigned int wep:1;
        unsigned int order:1;
    } frame_control;
    uint16_t duration;
    uint8_t addr1[6]; 
    uint8_t addr2[6]; 
    uint8_t addr3[6]; 
    uint16_t sequence_control;
} __attribute__((packed)) wifi_mac_header_t;

void wifi_sniffer_packet_handler(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return; 

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
    int payload_offset = sizeof(wifi_mac_header_t) + 12; 
    
    if (pkt->rx_ctrl.sig_len > payload_offset) {
        uint8_t *payload = (uint8_t*)pkt->payload;
        uint8_t *tag = payload + payload_offset;
        uint8_t *end = payload + pkt->rx_ctrl.sig_len;

        while (tag + 1 < end) {
            uint8_t tag_id = *tag;
            uint8_t tag_len = *(tag + 1);

            if (tag + 2 + tag_len > end) break; 

            if (tag_id == 0) { 
                if (tag_len > 0 && tag_len <= 32) {
                    char ssid[33];
                    int cleanIndex = 0;
                    
                    for (int j = 0; j < tag_len; j++) {
                        char c = tag[2 + j];
                        if (c >= 32 && c <= 126) {
                            ssid[cleanIndex++] = c;
                        }
                    }
                    ssid[cleanIndex] = '\0';

                    if (cleanIndex > 0) {
                        bool exists = false;
                        for (int i = 0; i < networkCount; i++) {
                            if (strcmp(scannedNetworks[i].ssid, ssid) == 0) {
                                exists = true;
                                scannedNetworks[i].rssi = pkt->rx_ctrl.rssi;
                                break;
                            }
                        }

                        if (!exists && networkCount < MAX_NETWORKS) {
                            strncpy(scannedNetworks[networkCount].ssid, ssid, 32);
                            scannedNetworks[networkCount].ssid[32] = '\0';
                            scannedNetworks[networkCount].rssi = pkt->rx_ctrl.rssi;
                            networkCount++;
                        }
                    }
                }
                break;
            }
            tag += 2 + tag_len; 
        }
    }
}

void startSniffing() {
    esp_wifi_set_mode(WIFI_MODE_STA); 
    esp_wifi_start(); 
    networkCount = 0;
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
}

void stopSniffing() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop(); 
    esp_wifi_set_mode(WIFI_MODE_NULL); 
}

void hopChannel() {
    static uint8_t ch = 1;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    ch++;
    if (ch > 13) ch = 1;
}