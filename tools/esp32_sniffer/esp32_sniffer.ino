// Ticket 14 - no-PSK 802.11 passive capture.
// ESP32 in promiscuous mode; dumps raw 802.11 management/data frames as hex
// over USB serial. No association, no WPA keys, no injection.
//
// Target: TMOBILE-1219 (Phantom Link AP). We trigger a known-plaintext DATA
// frame from the PC with tools/frame_probe.c + tools/capture_probe.ps1, then
// look for that frame's bytes in the dump and confirm the plaintext is absent.

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>

#define SNIFF_CHANNEL 6   // Phantom Link AP default channel; change if needed.

static volatile uint32_t frames_seen = 0;
static volatile uint32_t data_seen = 0;
static volatile uint32_t mgmt_seen = 0;
static volatile bool capturing = false;

// Promiscuous callback runs in the WiFi task context; keep it short and lock-free.
static void IRAM_ATTR sniff_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;
    frames_seen++;
    if (type == WIFI_PKT_DATA) data_seen++;
    else mgmt_seen++;
    if (!capturing) return;

    const uint8_t *p = pkt->payload;
    int len = (int)pkt->rx_ctrl.sig_len;
    if (len > 256) len = 256;           // full frame incl. 802.11 header + payload
    if (len < 1) return;

    // Marker so the host parser can find frames in the serial stream.
    Serial.write((const uint8_t *)"FRAME ", 6);
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        Serial.write(hex[p[i] >> 4]);
        Serial.write(hex[p[i] & 0x0F]);
    }
    Serial.write('\n');
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("ESP32 SNIFFER READY");
    Serial.println("commands: C=start capture, S=stop, P=print stats, H=hop channel");

    // Core 4.x requires explicit esp_wifi_init before any wifi calls work.
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    Serial.printf("wifi_init=%d\n", err);
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    Serial.printf("set_mode=%d\n", err);
    delay(100);
    // Set callback first, then start, then set the channel.
    err = esp_wifi_set_promiscuous_rx_cb(&sniff_cb);
    Serial.printf("set_cb=%d\n", err);
    err = esp_wifi_start();
    Serial.printf("wifi_start=%d\n", err);
    wifi_promiscuous_filter_t filt = { .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT };
    err = esp_wifi_set_promiscuous_filter(&filt);
    Serial.printf("filter=%d\n", err);
    err = esp_wifi_set_promiscuous(true);
    Serial.printf("promisc=%d\n", err);
    err = esp_wifi_set_channel(SNIFF_CHANNEL, WIFI_SECOND_CHAN_NONE);
    Serial.printf("channel=%d\n", SNIFF_CHANNEL);
    Serial.printf("set_ch=%d\n", err);
}

void loop() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == 'C' || c == 'c') {
            frames_seen = 0;
            data_seen = 0;
            capturing = true;
            Serial.println("CAPTURE START");
        } else if (c == 'S' || c == 's') {
            capturing = false;
            Serial.println("CAPTURE STOP");
        } else if (c == 'P' || c == 'p') {
            Serial.printf("STATS frames=%u data=%u mgmt=%u capturing=%d\n",
                          (unsigned)frames_seen, (unsigned)data_seen, (unsigned)mgmt_seen, (int)capturing);
        } else if (c == 'H' || c == 'h') {
            static int ch = SNIFF_CHANNEL;
            ch = (ch % 11) + 1;
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            Serial.printf("CHANNEL %d\n", ch);
        } else if (c >= '1' && c <= '9') {
            int ch = c - '0';
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            Serial.printf("CHANNEL %d\n", ch);
        }
    }
    delay(10);
}
