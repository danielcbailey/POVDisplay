#ifndef LED_CONTROLLER_INCLUDED
#define LED_CONTROLLER_INCLUDED

#include "hardware/gpio.h"
#include "spi.pio.h"

class LEDController {
    private:
        static int offset;
        PIO pio_inst;
        uint pio_sm;

    public:
        LEDController(uint mosiPin, uint sckPin, uint csPin);

        // expects data to have 8 bytes of data
        void sendData(uint8_t* data);

        const static unsigned char NOP_UPPER = 0x00; // sets the mode 1 register to the default value
        const static unsigned char NOP_LOWER = 0x00;
};

#endif // LED_CONTROLLER_INCLUDED