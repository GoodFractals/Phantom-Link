#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "oled.h" // Added OLED driver

// 1. Exact same struct as the TX
typedef struct __attribute__((packed)) {
    uint8_t  sync_word[2];
    uint8_t  length;
    char     payload[32];
} PhantomPacket;

// 2. Exact same XOR Cipher
void xor_cipher(char *data, int len, uint8_t key) {
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

// 3. UDP Callback
static void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        // DEBUG: Print the exact size of the incoming packet
        printf("Received UDP packet! Length: %d bytes\n", p->len);
        
        if (p->len == sizeof(PhantomPacket)) {
            PhantomPacket packet;
            memcpy(&packet, p->payload, sizeof(packet));

            if (packet.sync_word[0] == 0xAB && packet.sync_word[1] == 0xCD) {
                xor_cipher(packet.payload, packet.length, 0x5A);
                packet.payload[packet.length] = '\0'; 
                printf("Decrypted: %s\n", packet.payload);
                oled_print(packet.payload);
            } else {
                printf("Bad Sync Word!\n");
            }
        }
        pbuf_free(p);
    }
}

int main() {
    stdio_init_all();
    oled_init(); // Initialize OLED
    oled_print("WAITING...");
    
    while (!stdio_usb_connected()) tight_loop_contents();
    sleep_ms(1000); 
    printf("\n--- Phantom Link: Encrypted RX ---\n");

    if (cyw43_arch_init()) return -1;
    
    // Start AP
    cyw43_arch_enable_ap_mode("PhantomLink", "password123", CYW43_AUTH_WPA2_AES_PSK);
    printf("AP Active. IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    // Setup UDP Listener
    struct udp_pcb *pcb = udp_new();
    if (pcb != NULL) {
        udp_bind(pcb, IP_ADDR_ANY, 4444);
        udp_recv(pcb, udp_receive_callback, NULL);
        printf("Listening for Encrypted UDP on port 4444...\n");
    }

    while (true) {
        cyw43_arch_poll();
        sleep_ms(10);
    }
    return 0;
}