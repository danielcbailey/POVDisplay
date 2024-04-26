#include <stdio.h>
#include "pico/stdlib.h"

#include "LEDController.hpp"
#include "videoFileReading.h"
#include "hardware.h"
#include "hw_config.h" // SD card
#include "f_util.h"
#include "ff.h"
#include "pico/multicore.h"

void displayOnLEDs();

int main() {

    // Initialize chosen serial port
    stdio_init_all();
    sd_card_t* pSD = sd_get_by_num(0);
    FRESULT res = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (res != FR_OK) {
        panic("Error mounting SD card: %d\n", res);
    }

    printf("Starting...\n");

    // start the displayOnLEDs function on core 1
    multicore_launch_core1(displayOnLEDs);

    runFileReader("video.crv");
}