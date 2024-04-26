#include "hardware.h"

#include "hw_config.h"
#include "diskio.h"
#include "ff.h"

static spi_t spi = {
    .hw_inst = spi1,
    .miso_gpio = 12,
    .mosi_gpio = 11,
    .sck_gpio = 10,
    .baud_rate = 25 * 1000 * 1000, // was 25 * ...
};

static sd_card_t sd_card {
    .pcName = "0:",
    .spi = &spi,
    .ss_gpio = 9,
    .use_card_detect = false,
    .card_detected_true = 0,
};

size_t sd_get_num() {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return &sd_card;
    } else {
        return NULL;
    }
}

size_t spi_get_num() {
    return 1;
}

spi_t *spi_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return &spi;
    } else {
        return NULL;
    }
}