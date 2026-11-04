#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "oled.h"
#include "my_ui.h"
#include "boot_ui.h" 

void drawBootScreen(void) {
    memcpy(oled_buffer, boot_image, 1024); // Make sure it says boot_image!
    oled_show();
}

int main() {
    stdio_init_all();
    oled_init();
    
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);

    // 1. Show the Dolphins
    drawBootScreen();
    sleep_ms(2500); 
    
    // 2. Blast the custom UI image to the screen
    // memcpy copies the 1024-byte image into our display buffer instantly.
    memcpy(oled_buffer, my_image, 1024);
    oled_show();

    while (true) {
        // We can add joystick logic back here later!
        sleep_ms(50);
    }
    return 0;
}