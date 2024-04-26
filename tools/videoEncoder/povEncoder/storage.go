package povencoder

import (
	"encoding/binary"
	"os"
)

func SaveEncodedVideo(frames []*EncodedFrame, fileName string) error {
	fBuf := make([]byte, 0, 1024)

	// appending header
	fBuf = append(fBuf, 'C', 'R', 'V', 0)
	fBuf = binary.LittleEndian.AppendUint32(fBuf, uint32(len(frames)))

	for _, frame := range frames {
		// forming segments
		groupBufs := make([][]byte, len(frame.Segments[0].Groups))
		groupSegments := make([][][]byte, len(frame.Segments[0].Groups))
		for _, segment := range frame.Segments {
			for i, group := range segment.Groups {
				groupSegments[i] = append(groupSegments[i], group.Packets)
			}
		}

		for i, group := range groupSegments {
			groupBufs[i] = binary.LittleEndian.AppendUint32(groupBufs[i], uint32(len(group))) // number of "packets"

			for _, packet := range group {
				//groupBufs[i] = binary.LittleEndian.AppendUint16(groupBufs[i], uint16(512))           // rotational proportion, hard coded to 256 for now
				//groupBufs[i] = binary.LittleEndian.AppendUint16(groupBufs[i], uint16(len(packet)/8)) // number of bursts

				// copying the packet
				pCopy := make([]byte, len(packet))
				copy(pCopy, packet)
				for i := 1; i < len(pCopy); i += 2 {
					// applying color correction
					pCopy[i] = ledColorCorrection(pCopy[i])
				}

				groupBufs[i] = append(groupBufs[i], pCopy...)
			}
		}

		// appending frame
		offset2 := uint32(0x10 + len(groupBufs[0]))
		offset3 := offset2 + uint32(len(groupBufs[1]))
		offset4 := offset3 + uint32(len(groupBufs[2]))
		fBuf = binary.LittleEndian.AppendUint32(fBuf, offset2)
		fBuf = binary.LittleEndian.AppendUint32(fBuf, offset3)
		fBuf = binary.LittleEndian.AppendUint32(fBuf, offset4)
		fBuf = binary.LittleEndian.AppendUint32(fBuf, offset4+uint32(len(groupBufs[3]))) // appending the length of the frame

		for _, groupBuf := range groupBufs {
			fBuf = append(fBuf, groupBuf...)
		}
	}

	// creating the file
	return os.WriteFile(fileName, fBuf, 0644)
}

var interpTable = [16]byte{
	0,
	2,
	4,
	7,
	11,
	18,
	31,
	42,
	50,
	65,
	80,
	100,
	125,
	160,
	200,
	255,
}

func ledColorCorrection(c byte) byte {
	cFloat := float64(c) / 255
	cFloat *= 15

	// interpolate between the two closest values
	low := interpTable[int(cFloat)]
	if int(cFloat) == 15 {
		return 255
	}
	high := interpTable[int(cFloat)+1]

	// linear interpolation
	return byte(float64(low) + (cFloat-float64(int(cFloat)))*(float64(high)-float64(low)))
}
