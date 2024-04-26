#include "LEDController.hpp"
#include "hardware/pio.h"

int LEDController::offset = -1;

LEDController::LEDController(uint mosiPin, uint sckPin, uint csPin) {
    PIO pio = pio0;
    float clkdiv = 8; // just a little under 10MHz.

    // Get the PIO state machine
    uint sm = pio_claim_unused_sm(pio, true);

    if (offset == -1) {
        offset = pio_add_program(pio, &spi_cpha0_cs_program);
    }

    // configuring PIO hardware
    pio_sm_config c = spi_cpha0_cs_program_get_default_config(offset);
    sm_config_set_out_pins(&c, mosiPin, 1);
    sm_config_set_sideset_pins(&c, sckPin);
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // double length tx for 8-word fifo
    sm_config_set_clkdiv(&c, clkdiv);

    pio_sm_set_pins_with_mask(pio, sm, (2u << sckPin), (3u << sckPin) | (1u << sckPin));
    pio_sm_set_pindirs_with_mask(pio, sm, (3u << sckPin) | (1u << mosiPin), (3u << sckPin) | (1u << mosiPin));
    pio_gpio_init(pio, mosiPin);
    pio_gpio_init(pio, sckPin);
    pio_gpio_init(pio, sckPin + 1);
    gpio_set_outover(sckPin, GPIO_OVERRIDE_NORMAL);

    uint entry_point = offset + spi_cpha0_cs_offset_entry_point;
    pio_sm_init(pio, sm, entry_point, &c);
    pio_sm_exec(pio, sm, pio_encode_set(pio_x, 8 - 2));
    pio_sm_exec(pio, sm, pio_encode_set(pio_y, 8 - 2));
    pio_sm_set_enabled(pio, sm, true);

    pio_inst = pio;
    pio_sm = sm;
}

void LEDController::sendData(uint8_t* data) {
    io_rw_8 *fifo = (io_rw_8*)&pio_inst->txf[pio_sm];
    for (int i = 0; i < 8; i++) {
        *fifo = data[i];
    }
}