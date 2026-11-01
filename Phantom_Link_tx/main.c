#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h" // Needed for IP assignment

int main() {
    stdio_init_all();
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!stdio_usb_connected() && (to_ms_since_boot(get_absolute_time()) - start < 3000)) tight_loop_contents();
    sleep_ms(1000); 
    printf("\n--- Phantom Link: UDP TX ---\n");

    if (cyw43_arch_init()) return -1;
    cyw43_arch_enable_sta_mode();

    printf("Connecting to PhantomLink AP...\n");
    
    // Connect to AP
    while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_JOIN) {
        cyw43_arch_wifi_connect_async("PhantomLink", "password123", CYW43_AUTH_WPA2_AES_PSK);
        cyw43_arch_poll();
        sleep_ms(1000);
        printf("Attempting connection...\n");
    }
    printf("Connected!\n");

    // --- NEW: FORCE A STATIC IP ---
    struct netif *n = netif_default;
    if (n != NULL) {
        ip_addr_t ip, mask, gw;
        IP4_ADDR(&ip, 192, 168, 4, 2);      // TX IP
        IP4_ADDR(&mask, 255, 255, 255, 0);  // Subnet mask
        IP4_ADDR(&gw, 192, 168, 4, 1);      // Gateway (The RX Pico)
        
        netif_set_addr(n, &ip, &mask, &gw);
        netif_set_up(n);
        printf("TX IP assigned: %s\n", ip4addr_ntoa(&n->ip_addr));
    }

    // Create UDP socket
    struct udp_pcb *pcb = udp_new();
    ip_addr_t dest_ip;
    IP4_ADDR(&dest_ip, 192, 168, 4, 1); // Target RX IP

    char *message = "HELLO FROM PHANTOM LINK";

    while (true) {
        printf("Sending UDP packet...\n");
        
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(message) + 1, PBUF_RAM);
        if (p != NULL) {
            memcpy(p->payload, message, strlen(message) + 1);
            udp_sendto(pcb, p, &dest_ip, 4444);
            pbuf_free(p);
        }
        
        cyw43_arch_poll();
        sleep_ms(1000); 
    }
    return 0;
}