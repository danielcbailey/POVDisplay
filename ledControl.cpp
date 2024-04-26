#include "videoFileReading.h"
#include "pico/stdlib.h"
#include "pico/types.h"
#include "hardware.h"
#include "LEDController.hpp"
#include <stdio.h>

uint32_t timeBetweenPackets(uint32_t frameTime, uint32_t numPackets) {
    return (frameTime << 5) / numPackets; // the x32 is to allow for fractional microseconds
}

unsigned char* currGroupBuffers[4] = {nullptr, nullptr, nullptr, nullptr};
int currGroupPacketPos[4] = {0, 0, 0, 0};
uint32_t groupTimeBetweenPackets[4] = {0, 0, 0, 0};
uint64_t groupNextPacketTime[4] = {0, 0, 0, 0};
int groupPacketLength[4] = {0, 0, 0, 0};
int numPacketsRetrieved[4] = {0, 0, 0, 0};

unsigned char iRefs[16] = {
    20,
    20,
    20,
    19, // L4
    19,
    18,
    17,
    15, // L8
    15,
    17,
    18,
    19, // L12
    19,
    20,
    20,
    20 // L16
};

// unsigned char iRefs[16] = {
//     30,
//     30,
//     29,
//     29, // L4
//     28,
//     26,
//     24,
//     20, // L8
//     20,
//     24,
//     26,
//     28, // L12
//     29,
//     29,
//     30,
//     30 // L16
// };

float channelScalars[3] = {
    1.7, // Red
    0.4, // Green
    0.8 // Blue
};

unsigned char temp2;

inline uint64_t abs(uint64_t a) {
    return a < 0 ? -a : a;
}

void updateGroupBuffers(uint32_t frameTime);

uint32_t frameTimeBuffer[15]; // circular buffer of frame times
uint32_t frameTimeBufferPos = 0;

uint64_t timeAround = 0;

void displayOnLEDs() {
    sleep_ms(5924); // give the SD card time to initialize

    // Initializing LEDs
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

    gpio_init(HALL_SENSOR_PIN);
    gpio_set_dir(HALL_SENSOR_PIN, GPIO_IN); // reads 1 when no magnet, 0 when magnet
    gpio_pull_up(HALL_SENSOR_PIN); // sensor is open-drain

    // initializing the chips
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 24; j++) {
            float colorScalar = channelScalars[j % 3];

            // unsigned char buffer[16] = {
            //     (0x28 + j) << 1, iRefs[4*i + 3] * scalar, 
            //     (0x28 + j) << 1, iRefs[4*i + 2] * scalar,
            //     (0x28 + j) << 1, iRefs[4*i + 1] * scalar,
            //     (0x28 + j) << 1, iRefs[4*i] * scalar
            // };

            unsigned char buffer[16];
            for (int k = 0; k < 4; k++) {
                buffer[2*k] = (0x28 + j) << 1;
                int ledIdx = (4*i + (3-k)) * 8 + (7-j / 3);
                float mirroredIdx = ledIdx > 63 ? 127 - ledIdx : ledIdx;
                float posScalar = 64.0f / (mirroredIdx - 67) + 21.0f;

                buffer[2*k + 1] = posScalar * colorScalar;
            }

            groups[i]->sendData(buffer);
            sleep_us(150);
        }
    }

    uint32_t frameTime = 41666; // 12 rotations per second
    uint64_t magnetFrameOnTime = time_us_64();

    // waiting for the bar to be in the right position
    uint64_t prevRotationStart = time_us_64();
    bool wasMagnet = gpio_get(HALL_SENSOR_PIN);
    int count = 0;
    while (true) {
        bool magnetReading = gpio_get(HALL_SENSOR_PIN);
        if (!magnetReading && magnetReading != wasMagnet) {
            magnetFrameOnTime = time_us_64();
        } else if (magnetReading != wasMagnet) {
            wasMagnet = !wasMagnet;
            count++;

            uint64_t currTime = time_us_64();
            uint64_t centerTime = (currTime - magnetFrameOnTime) * 5 / 6 + magnetFrameOnTime;
            uint32_t rotationTime = centerTime - prevRotationStart;
            prevRotationStart = centerTime;

            if (count >= 2) {
                frameTime = rotationTime / 2;
                break;
            }
        }
        wasMagnet = magnetReading;
    }

    for (int i = 0; 15 > i; i++) {
        frameTimeBuffer[i] = frameTime;
    }

    printf("Frame time stabilized. %d\n", frameTime);

    // initializing next burst times
    for (int i = 0; 4 > i; i++) {
        groupNextPacketTime[i] = time_us_64() << 5;
    }

    uint64_t prevTime = time_us_64();

    while (true) {
        uint64_t currTime = time_us_64();
        uint64_t currTimeX32 = currTime << 5;
        timeAround = currTime - prevTime;
        prevTime = currTime;
        bool magnetReading = gpio_get(HALL_SENSOR_PIN);
        if (!magnetReading && magnetReading != wasMagnet) {
            magnetFrameOnTime = time_us_64();
        } else if (magnetReading && magnetReading != wasMagnet) {
            uint64_t currTime = time_us_64();
            uint64_t centerTime = (currTime - magnetFrameOnTime) * 5 / 6 + magnetFrameOnTime;
            uint32_t rotationTime = centerTime - prevRotationStart;
            prevRotationStart = currTime;
            uint32_t immediateFrameTime = rotationTime / 2;

            // update frame time buffer
            // frameTimeBuffer[frameTimeBufferPos] = immediateFrameTime;
            // frameTimeBufferPos = (frameTimeBufferPos + 1) % 15;

            // // calculate the average frame time
            // frameTime = 0;
            // for (int i = 0; 15 > i; i++) {
            //     frameTime += frameTimeBuffer[i];
            // }

            // frameTime /= 15;
            frameTime = immediateFrameTime;
            
            // Dialating the frame time - if it hasn't finished the previous frame, it
            // will make the frame time shorter, and if it has, it will make the frame time longer
            // depending on the amount of error
            if (currGroupPacketPos[0] > groupPacketLength[0] / 2) {
                // still working through the previous frame
                float error = (groupPacketLength[0] - currGroupPacketPos[0] - 1) / (float)groupPacketLength[0];
                frameTime = frameTime / (1 + error/2);
            } else {
                // finished the previous frame - already started the next one
                float error = currGroupPacketPos[0] / (float)groupPacketLength[0];
                frameTime = frameTime * (1 + error);
            }

            // syncing buffers
            // if (currGroupPacketPos[0] > groupPacketLength[0] / 2) {
            //     // if more than half way along, advance to the next buffer, otherwise restart current
            //     updateGroupBuffers(frameTime);
            // } else {
            //     for (int i = 0; 4 > i; i++) {
            //         currGroupPacketPos[i] = 0;
            //     }
            // }
        }
        wasMagnet = magnetReading;

        for (int i = 0; 4 > i; i++) {
            // i is the group index
            if (currGroupPacketPos[i] >= groupPacketLength[i]) {
                // get new buffers
                updateGroupBuffers(frameTime);
            }

            // check if it is time to send the burst
            if (currTimeX32 >= groupNextPacketTime[i]) {
                unsigned char* buf = currGroupBuffers[i] + currGroupPacketPos[i] * 8;

                // TEMPORARY: fixing the burst
                // for (int j = 0; 8 > j; j+=2) {
                //     temp2 = ((buf[j] - 0x20) + 0x10) << 1;
                //     if (buf[j] > 0x4E || buf[j] < 0x20) {
                //         temp2 = 0x4E;
                //     }
                // }

                // send the packet
                groups[i]->sendData(buf);
                currGroupPacketPos[i]++;
                groupNextPacketTime[i] = groupNextPacketTime[i] + groupTimeBetweenPackets[i];
            }
        }
    }
}

void updateGroupBuffers(uint32_t frameTime) {
    GroupBufferInfo bufInfo = getGroupBuffers();
    currGroupBuffers[0] = bufInfo.group1Buf;
    currGroupBuffers[1] = bufInfo.group2Buf;
    currGroupBuffers[2] = bufInfo.group3Buf;
    currGroupBuffers[3] = bufInfo.group4Buf;
    groupPacketLength[0] = bufInfo.group1BufLength;
    groupPacketLength[1] = bufInfo.group2BufLength;
    groupPacketLength[2] = bufInfo.group3BufLength;
    groupPacketLength[3] = bufInfo.group4BufLength;

    for (int j = 0; 4 > j; j++) {
        currGroupPacketPos[j] = 0;
        groupTimeBetweenPackets[j] = timeBetweenPackets(frameTime, groupPacketLength[j]);
    }
}