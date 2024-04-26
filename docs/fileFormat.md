# File Format of Video Data

The processor loads the data for the LEDs at any given time from the SD card.
The LEDs are in groups of four, and at any given transfer, one LED from each controller
must get updated. The timing of sending the data out can vary intra-frame and between frames.
This allows for variable resolution where it counts to get around the SD card limitations.

## Format Overview

The file is organized into the following portions:

* File header - details how many frames are in the file.
* Frame - the organizational unit for one complete revolution of the display. Frames contain four groups.
* Group - the organizational unit within one frame that details the data to be sent to one group of LEDs for that frame.
* Packet - the lowest-level container of data, corresponding to data samples and the rotational proportion they represent.

## File Header Format

| Offset | Field                           |
| ------ | -----                           |
| 0x0000 | File Format Identifier: "CRV\0" |
| 0x0004 | Number of frames: uint32        |
| 0x0008 | Start of frame 0                |

## Frame Format

| Offset | Field                      |
| ------ | -----                      |
| 0x0000 | Group 2 offset: n (uint32) |
| 0x0004 | Group 3 offset: m (uint32) |
| 0x0008 | Group 4 offset: p (uint32) |
| 0x000C | Frame length: uint32       |
| 0x0010 | Start of group 1           |
| n      | Start of group 2           |
| m      | Start of group 3           |
| p      | Start of group 4           |

## Group Format

| Offset | Field                     |
| ------ | -----                     |
| 0x0000 | Number of packets: uint32 |
| 0x0004 | Start of packet 0         |

## Packet Format

| Offset | Field                                   |
| ------ | -----                                   |
| 0x0000 | Start of burst 0                        |
| 0x0004 | Last bytes of burst 0                   |
| 0x0008 | Start of burst 1                        |
