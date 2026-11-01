#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"

// Callback function when a UDP packet is received
static void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        // Print the payload!
        printf("Received: %s\n", (char *)p->payload);
        pbuf_free(p);
    }
}

int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) tight_loop_contents();
    sleep_ms(1000); 
    printf("\n--- Phantom Link: UDP RX ---\n");

    if (cyw43_arch_init()) return -1;
    
    // Start Access Point
    cyw43_arch_enable_ap_mode("PhantomLink", "password123", CYW43_AUTH_WPA2_AES_PSK);
    printf("AP Active. IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));

    // Create a UDP socket
    struct udp_pcb *pcb = udp_new();
    if (pcb != NULL) {
        // Bind it to port 4444
        udp_bind(pcb, IP_ADDR_ANY, 4444);
        // Set the callback
        udp_recv(pcb, udp_receive_callback, NULL);
        printf("Listening for UDP packets on port 4444...\n");
    } else {
        printf("Failed to create UDP PCB\n");
    }

        uint32_t last_print = 0;
    while (true) {
        cyw43_arch_poll();
        
        // Print a heartbeat every 5 seconds
        if (to_ms_since_boot(get_absolute_time()) - last_print > 5000) {
            printf("Waiting for UDP data...\n");
            last_print = to_ms_since_boot(get_absolute_time());
        }
        
        sleep_ms(10);
    }
    return 0;
}