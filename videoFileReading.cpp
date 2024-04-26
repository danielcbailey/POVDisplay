#include "ff.h"
#include "f_util.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "videoFileReading.h"
#include <stdio.h> // FOR TESTING ONLY

FIL fil;

unsigned char frameBuffers[2][73728];
volatile int bufferLengths[4]; // in number of bursts
volatile unsigned char* groupBuffers[4] = {NULL, NULL, NULL, NULL};
int frameBufferFilled = 0;
volatile int lastBufferFilled[4] = {0, 0, 0, 0}; // if equal to buffersGiven, then should start filling the next buffer

uint32_t nextFrame = 0x8;
int32_t frameNumber = -1;
uint32_t groupNumPackets[4] = {0, 0, 0, 0}; // number of packets in the group for this frame
volatile bool fetchFrame = true;

uint32_t numberFrames;

uint32_t fetchTime = 0;

void loadNewFrame();

void runFileReader(const char* filename) {
    FRESULT res = f_open(&fil, filename, FA_READ);
    if (FR_OK != res && FR_EXIST != res)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(res), res);

    // checking file format
    char buffer[4];
    UINT bytesRead;
    res = f_read(&fil, buffer, sizeof(buffer), &bytesRead);
    if (FR_OK != res)
        panic("f_read() error: %s (%d)\n", FRESULT_str(res), res);

    if (buffer[0] != 'C' || buffer[1] != 'R' || buffer[2] != 'V' || buffer[3] != 0) {
        panic("Invalid file format. Expected .crv\n"); // CRV stands for Compressed Rotational Video
    }

    // reading the number of frames
    uint32_t numFrames;
    res = f_read(&fil, &numFrames, sizeof(numFrames), &bytesRead);
    if (FR_OK != res)
        panic("f_read() error: %s (%d)\n", FRESULT_str(res), res);

    numberFrames = numFrames;

    while (true) {
        while (!fetchFrame) {
            // busy waiting
        }

        // loading the next frame
        uint32_t startTime = time_us_32();
        loadNewFrame();
        fetchTime = time_us_32() - startTime;
    }
}

void loadNewFrame() {
    // seeking the file to the next frame
    f_lseek(&fil, nextFrame);
    UINT bytesRead;
    uint32_t group2Offset;
    f_read(&fil, &group2Offset, sizeof(group2Offset), &bytesRead);
    uint32_t group3Offset;
    f_read(&fil, &group3Offset, sizeof(group3Offset), &bytesRead);
    uint32_t group4Offset;
    f_read(&fil, &group4Offset, sizeof(group4Offset), &bytesRead);
    uint32_t frameLength;
    f_read(&fil, &frameLength, sizeof(frameLength), &bytesRead);

    // reading the frame
    int bufToUse = (frameBufferFilled + 1) % 2;
    f_read(&fil, frameBuffers[bufToUse], frameLength - 0x10, &bytesRead);

    // updating the group variables
    groupBuffers[0] = frameBuffers[bufToUse] + 0x14; // the +4 is to skip over the segment count (useless now)
    groupBuffers[1] = frameBuffers[bufToUse] + group2Offset + 0x4 - 0x10;
    groupBuffers[2] = frameBuffers[bufToUse] + group3Offset + 0x4 - 0x10;
    groupBuffers[3] = frameBuffers[bufToUse] + group4Offset + 0x4 - 0x10;

    groupNumPackets[0] = (group2Offset - 0x14) / 8;
    groupNumPackets[1] = (group3Offset - group2Offset - 0x4) / 8;
    groupNumPackets[2] = (group4Offset - group3Offset - 0x4) / 8;
    groupNumPackets[3] = (frameLength - group4Offset - 0x14) / 8;

    frameNumber++;
    frameBufferFilled = bufToUse;
    fetchFrame = false; // reset the flag
    if (frameNumber >= numberFrames) {
        frameNumber = -1;
        nextFrame = 0x8;
    } else {
        nextFrame = nextFrame + frameLength;
    }
}

uint32_t getBufCalls = 0;
uint32_t timeGetLastCalled[4] = {0, 0, 0, 0};
uint32_t timeDiffBetweenLastCalled[4] = {0, 0, 0, 0};

GroupBufferInfo getGroupBuffers() {
    // while (buffersGiven[groupNumber] == lastBufferFilled[groupNumber]) {
    //     // busy waiting
    // }
    __dsb();
    GroupBufferInfo ret;
    ret.group1Buf = (unsigned char*)groupBuffers[0];
    ret.group2Buf = (unsigned char*)groupBuffers[1];
    ret.group3Buf = (unsigned char*)groupBuffers[2];
    ret.group4Buf = (unsigned char*)groupBuffers[3];
    ret.group1BufLength = groupNumPackets[0];
    ret.group2BufLength = groupNumPackets[1];
    ret.group3BufLength = groupNumPackets[2];
    ret.group4BufLength = groupNumPackets[3];
    fetchFrame = true; // signal to load a new frame
    __dmb();
    return ret;
}