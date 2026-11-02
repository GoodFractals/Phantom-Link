#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "oled.h" // Added OLED driver

// 1. Define our custom packet structure
typedef struct {
    uint8_t  sync_word[2];
    uint8_t  length;
    char     payload[32];
} PhantomPacket;

// 2. XOR Cipher
void xor_cipher(char *data, int len, uint8_t key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

int main() {
    stdio_init_all();
    oled_init(); // Initialize OLED
    oled_print("STARTING...");
    
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!stdio_usb_connected() && (to_ms_since_boot(get_absolute_time()) - start < 3000)) tight_loop_contents();
    sleep_ms(1000); 
    printf("\n--- Phantom Link: Encrypted TX ---\n");

    if (cyw43_arch_init()) return -1;
    cyw43_arch_enable_sta_mode();

    printf("Connecting to PhantomLink AP...\n");
    while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_JOIN) {
        cyw43_arch_wifi_connect_async("PhantomLink", "password123", CYW43_AUTH_WPA2_AES_PSK);
        cyw43_arch_poll();
        sleep_ms(1000);
    }
    printf("Connected!\n");

    // Force static IP
    struct netif *n = netif_default;
    if (n != NULL) {
        ip_addr_t ip, mask, gw;
        IP4_ADDR(&ip, 192, 168, 4, 2);      // Force TX IP
        IP4_ADDR(&mask, 255, 255, 255, 0);
        IP4_ADDR(&gw, 192, 168, 4, 1);      // Target RX IP
        
        netif_set_addr(n, &ip, &mask, &gw);
        netif_set_up(n);
        netif_set_link_up(n);
        printf("TX IP assigned: %s\n", ip4addr_ntoa(&n->ip_addr));
    }

    struct udp_pcb *pcb = udp_new();
    ip_addr_t dest_ip;
    IP4_ADDR(&dest_ip, 192, 168, 4, 1);

    uint8_t SECRET_KEY = 0x5A; 
    char *message = "ATTACK AT DAWN";

      // Give the RX time to start its AP and UDP listener
    sleep_ms(3000); 
    
    while (true) {
        PhantomPacket packet;
        
        // Show on OLED before sending
        oled_print(message);

        // Build packet
        packet.sync_word[0] = 0xAB;
        packet.sync_word[1] = 0xCD;
        packet.length = strlen(message);
        strncpy(packet.payload, message, sizeof(packet.payload));

        // Encrypt
        xor_cipher(packet.payload, packet.length, SECRET_KEY);

        printf("Sending encrypted packet...\n");
        
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet), PBUF_RAM);
        if (p != NULL) {
            memcpy(p->payload, &packet, sizeof(packet));
            udp_sendto(pcb, p, &dest_ip, 4444);
            pbuf_free(p);
        }
        
        cyw43_arch_poll();
        sleep_ms(2000); 
    }
    return 0;
}