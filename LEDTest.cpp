// CONVERT TO MAIN.CPP FOR USE.

#include <stdio.h>
#include "pico/stdlib.h"

#include "LEDController.hpp"
#include "hardware.h"
#include "hw_config.h" // SD card
#include "f_util.h"
#include "ff.h"

unsigned char pwmBuffers[16][24];
unsigned char iRefs[16] = {
    80,
    80,
    80,
    80, // L4
    80,
    80,
    80,
    80, // L8
    80,
    80,
    80,
    80, // L12
    80,
    80,
    80,
    80 // L16
};

int main() {

    // Initialize chosen serial port
    stdio_init_all();
    sd_card_t* pSD = sd_get_by_num(0);
    FRESULT res = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (res != FR_OK) {
        panic("Error mounting SD card: %d\n", res);
    }

    FIL fil;
    res = f_open(&fil, "test.txt", FA_READ);
    if (FR_OK != res && FR_EXIST != res)
        panic("f_open(%s) error: %s (%d)\n", "test.txt", FRESULT_str(res), res);

    char buffer[256];
    UINT bytesRead;
    res = f_read(&fil, buffer, sizeof(buffer), &bytesRead);
    if (FR_OK != res)
        panic("f_read() error: %s (%d)\n", FRESULT_str(res), res);

    f_close(&fil);

    printf("Testing SD card...\n");
    buffer[bytesRead] = '\0'; // Null-terminate the string
    printf("%s\n", buffer);


    printf("Starting...\n");

    // Testing the LEDs
    LEDController group1(GROUP1_DATA_PIN, GROUP1_CLOCK_PIN, GROUP1_CHIP_SELECT_PIN);
    LEDController group2(GROUP2_DATA_PIN, GROUP2_CLOCK_PIN, GROUP2_CHIP_SELECT_PIN);
    LEDController group3(GROUP3_DATA_PIN, GROUP3_CLOCK_PIN, GROUP3_CHIP_SELECT_PIN);
    LEDController group4(GROUP4_DATA_PIN, GROUP4_CLOCK_PIN, GROUP4_CHIP_SELECT_PIN);
    LEDController* groups[4] = {&group1, &group2, &group3, &group4};

    // Resetting the LEDs
    gpio_init(LED_RESET_PIN);
    gpio_set_dir(LED_RESET_PIN, GPIO_OUT);
    gpio_put(LED_RESET_PIN, 0);
    sleep_ms(10);
    gpio_put(LED_RESET_PIN, 1);
    sleep_ms(10);

    // initializing the chips
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 24; j++) {
            unsigned char buffer[16] = {
                (0x28 + j) << 1, iRefs[4*i + 3],
                (0x28 + j) << 1, iRefs[4*i + 2],
                (0x28 + j) << 1, iRefs[4*i + 1],
                (0x28 + j) << 1, iRefs[4*i]
            };

            groups[i]->sendData(buffer);
            sleep_us(150);
        }
    }

    // Loop forever
    int i = 0;
    int offset = 0;
    while (true) {
        // making the LEDs move

        // updating the buffer
        for (int j = 0; j < 128; j++) {
            for (int k = 0; k < 3; k++) {
                pwmBuffers[j / 8][21-(3*(j%8)) + k] = k == offset && j == i ? 0x41 : 0x00;
            }
        }

        // sending the buffers
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 24; k++) {
                unsigned char buffer[16] = {
                    (0x10 + k) << 1, pwmBuffers[4*j+3][k],
                    (0x10 + k) << 1, pwmBuffers[4*j+2][k],
                    (0x10 + k) << 1, pwmBuffers[4*j+1][k],
                    (0x10 + k) << 1, pwmBuffers[4*j][k]
                };

                groups[j]->sendData(buffer);
                sleep_us(150);
            }
        }

        // updating the index
        i++;
        //printf("i: %d\n", i);
        //sleep_ms(100);
        if (i >= 128) {
            i = 0;
            offset++;
            if (offset == 3) {
                offset = 0;
            }
        }
    }
}