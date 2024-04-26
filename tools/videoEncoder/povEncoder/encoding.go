package povencoder

import (
	"fmt"
	"image"
	"image/color"
	"math"
	"sync/atomic"
	"time"

	"github.com/danielcbailey/POVDisplay/tools/videoEncoder/ffmpeg"
	"github.com/llgcode/draw2d/draw2dimg"
)

type EncodedGroup struct {
	Packets []byte
}

// Represents an eighth arc. So four per frame
type EncodedSegment struct {
	Groups []EncodedGroup
}

type EncodedFrame struct {
	Segments    []EncodedSegment
	timeOfFrame time.Duration
	startAngle  float64
}

type ledColor struct {
	r, g, b byte
}

// var groupPacketsPerSecond = []int{104166, 52083, 52083, 104166} // 20 Mbps
// var groupPacketsPerSecond = []int{78124, 39062, 39062, 78124} // 15 Mbps
var groupPacketsPerSecond = []int{39062, 19531, 19531, 39062} // 7.5 Mbps

const ledsPerIC = 8
const numLEDs = 128

const numThreads = 12

func EncodeFrame(img image.Image, timeOfFrame time.Duration, leftBottom bool) (*EncodedFrame, error) {
	// if leftBottom is true, it means the side of the board that has the hall sensor is down.
	// if leftBottom is false, it means the side of the board that has the hall sensor is up.

	// Samples 2000 angles per frame. Will try to squeeze as many updates into the encoded image within the packets per sec allotment.
	numSteps := 2000
	dTheta := 3.14159 / float64(numSteps)
	trueStartAngle := -3.14159 / 2
	startAngle := -3.14159 / 2
	if !leftBottom {
		trueStartAngle = 3.14159 / 2
		startAngle = 3.14159 / 2
	}

	ledDistances := make([]float64, numLEDs)
	ledStride := (float64(img.Bounds().Dx() - 8)) / numLEDs
	ledWidth := ledStride / 2.0 // divided into 2 because the LEDs are interlaced
	for i := 0; i < numLEDs; i++ {
		ledDistances[i] = ledStride*float64(numLEDs/2-i) - ledStride/4.0 // to account for the interlacing of the LEDs
	}

	allotmentsPerStep := make([]float64, len(groupPacketsPerSecond))
	for i := 0; i < len(groupPacketsPerSecond); i++ {
		allotmentsPerStep[i] = float64(groupPacketsPerSecond[i]) * timeOfFrame.Seconds() / float64(numSteps)
	}

	credits := []float64{0, 0, 0, 0} // accumulation of allotments
	ledColors := make([]ledColor, numLEDs)
	ledLastUpdated := make([]float64, numLEDs)
	for i := 0; i < numLEDs; i++ {
		ledLastUpdated[i] = startAngle
	}

	workingSegmentStart := startAngle
	workingGroups := make([]EncodedGroup, len(groupPacketsPerSecond))
	for j := 0; len(groupPacketsPerSecond) > j; j++ {
		workingGroups[j].Packets = make([]byte, 0, 256)
	}

	doneSegments := make([]EncodedSegment, 0, 4)

	for i := 0; i < numSteps; i++ {
		angle := startAngle + dTheta*float64(i) // work backwards because the LED will turn on and represent the pixels

		if angle-workingSegmentStart > 3.14159/2 {
			// segment is done
			doneSegments = append(doneSegments, EncodedSegment{Groups: workingGroups})
			workingSegmentStart = angle
			workingGroups = make([]EncodedGroup, len(groupPacketsPerSecond))
			for j := 0; len(groupPacketsPerSecond) > j; j++ {
				workingGroups[j].Packets = make([]byte, 0, 256)
			}
		}

		// updating credits
		for j := 0; j < len(credits); j++ {
			credits[j] += float64(allotmentsPerStep[j])
		}

		// updating LED colors
		for groupIdx := 0; groupIdx < len(groupPacketsPerSecond); groupIdx++ {
			// testing if has enough credit to update the LEDs
			for credits[groupIdx] >= 1 {
				credits[groupIdx]--
				// updating the LEDs - must pick the most despirate channel from each channel of each LED
				newLEDColors := make([]ledColor, numLEDs/len(groupPacketsPerSecond))
				startLEDIdx := numLEDs / len(groupPacketsPerSecond) * groupIdx
				for ledIdx := startLEDIdx; ledIdx < numLEDs/len(groupPacketsPerSecond)*(groupIdx+1); ledIdx++ {
					// only looking over the LEDs of this group
					newLEDColors[ledIdx-startLEDIdx] = sampleLED(ledDistances[ledIdx], ledWidth, angle-2*dTheta, angle+2*dTheta, img)
				}

				// picking the most despirate channel from each chip
				chipsPerGroup := len(newLEDColors) / ledsPerIC
				packet := make([]byte, 0, 8)
				for chipIdx := chipsPerGroup - 1; chipIdx >= 0; chipIdx-- {
					// finding the most despirate channel
					mostLEDIdx := 0
					mostChannel := 0
					maxDelta := -1
					for ledIdx := chipIdx * ledsPerIC; (chipIdx+1)*ledsPerIC > ledIdx; ledIdx++ {
						// note that LED Idx is relative to the new color array
						delta := int(newLEDColors[ledIdx].r) - int(ledColors[ledIdx+startLEDIdx].r)
						if delta < 0 {
							delta = -delta
						}

						if delta > maxDelta {
							maxDelta = delta
							mostLEDIdx = ledIdx
							mostChannel = 0 // red
						}

						delta = int(newLEDColors[ledIdx].g) - int(ledColors[ledIdx+startLEDIdx].g)
						if delta < 0 {
							delta = -delta
						}

						if delta > maxDelta {
							maxDelta = delta
							mostLEDIdx = ledIdx
							mostChannel = 1 // green
						}

						delta = int(newLEDColors[ledIdx].b) - int(ledColors[ledIdx+startLEDIdx].b)
						if delta < 0 {
							delta = -delta
						}

						if delta > maxDelta {
							maxDelta = delta
							mostLEDIdx = ledIdx
							mostChannel = 2 // blue
						}
					}

					// forming the command
					cmd, value := formCommand(mostLEDIdx%8, mostChannel, newLEDColors[mostLEDIdx])
					packet = append(packet, cmd, value)

					// updating the LED color
					if mostChannel == 0 {
						ledColors[mostLEDIdx+startLEDIdx].r = newLEDColors[mostLEDIdx].r
					} else if mostChannel == 1 {
						ledColors[mostLEDIdx+startLEDIdx].g = newLEDColors[mostLEDIdx].g
					} else {
						ledColors[mostLEDIdx+startLEDIdx].b = newLEDColors[mostLEDIdx].b
					}
				}

				// adding the packet to the group
				workingGroups[groupIdx].Packets = append(workingGroups[groupIdx].Packets, packet...)
			}
		}
	}

	// adding the last segment
	doneSegments = append(doneSegments, EncodedSegment{Groups: workingGroups})

	ret := &EncodedFrame{Segments: doneSegments, timeOfFrame: timeOfFrame, startAngle: trueStartAngle}

	return ret, nil
}

func sampleLED(dist, width float64, prevAngle, currAngle float64, img image.Image) ledColor {
	cx := float64(img.Bounds().Dx())/2.0 + dist*math.Cos(currAngle)
	cy := float64(img.Bounds().Dy()) - (float64(img.Bounds().Dy())/2.0 + dist*math.Sin(currAngle))

	// cropping the source image to 64x64 centered at (cx, cy)
	cropPadding := 16
	bounds := image.Rect(int(cx)-cropPadding, int(cy)-cropPadding, int(cx)+cropPadding, int(cy)+cropPadding)
	//cropped := img.(*image.RGBA).SubImage(bounds)

	// creating the path of the LED
	path := image.NewRGBA(image.Rect(0, 0, bounds.Dx(), bounds.Dy()))
	gc := draw2dimg.NewGraphicContext(path)
	gc.SetFillColor(color.RGBA{255, 255, 255, 255})
	gc.SetStrokeColor(color.RGBA{255, 255, 255, 255})
	gc.SetLineWidth(width)
	gc.BeginPath()
	//gc.MoveTo(cx, float64(img.Bounds().Dy())-cy)
	centerX := float64(bounds.Dx()/2) - dist*math.Cos(currAngle)
	centerY := float64(bounds.Dy()/2) + dist*math.Sin(currAngle)
	gc.ArcTo(centerX, centerY, dist, dist, -currAngle, prevAngle-currAngle)
	gc.Stroke()

	// averaging the color of the path
	r, g, b := 0.0, 0.0, 0.0
	aSum := 0.0
	for x := 0; cropPadding*2 > x; x++ {
		for y := 0; cropPadding*2 > y; y++ {
			rS, gS, bS, _ := img.At(x+bounds.Min.X, y+bounds.Min.Y).RGBA()
			alpha := float64(path.Pix[y*path.Stride+x*4+3])
			r += float64(rS>>8) * alpha // multiply by the alpha value
			g += float64(gS>>8) * alpha
			b += float64(bS>>8) * alpha
			aSum += alpha
		}
	}

	// obtain the current color - it should be equal the weight of the average
	rS, gS, bS, _ := img.At(int(cx), int(cy)).RGBA()

	r += float64(rS>>8) * aSum
	g += float64(gS>>8) * aSum
	b += float64(bS>>8) * aSum

	aSum += aSum

	// normalizing the color
	if aSum != 0 {
		r /= aSum
		g /= aSum
		b /= aSum
	}

	return ledColor{byte(r), byte(g), byte(b)}
}

// ch is 0 for red, 1 for green, 2 for blue
// ledIdx is relative to the chip. So 0-7. Note that 0 corresponds to D8_LX physically
func formCommand(ledIdx, ch int, value ledColor) (byte, byte) {
	physIdx := 3*(7-ledIdx) + ch
	retV := value.r
	if ch == 1 {
		retV = value.g
	} else if ch == 2 {
		retV = value.b
	}

	return byte(0x10+physIdx) << 1, retV
}

func EncodeFrames(frames *ffmpeg.FrameArray) []*EncodedFrame {
	size := (frames.GetNumFrames() / numThreads) * numThreads
	ret := make([]*EncodedFrame, size)

	channels := make([]chan bool, numThreads)
	amtDone := int32(0)
	for i := 0; numThreads > i; i++ {
		startIdx := i * size / numThreads
		endIdx := (i + 1) * size / numThreads
		done := make(chan bool)
		go frameEncoderWorker(frames, startIdx, endIdx, ret, done, &amtDone)
		channels[i] = done
	}

	for i := 0; numThreads > i; i++ {
		<-channels[i]
	}

	return ret
}

func frameEncoderWorker(frames *ffmpeg.FrameArray, startIdx, endIdx int, ret []*EncodedFrame, done chan bool, amtDone *int32) {
	for i := startIdx; endIdx > i; i++ {
		frame, _ := EncodeFrame(frames.GetFrame(i), frames.GetFrameTime(), i%2 == 0)
		fmt.Printf("Encoding. %.2f%%\n", float64(atomic.AddInt32(amtDone, 1))/float64(frames.GetNumFrames())*100)
		ret[i] = frame
	}

	done <- true
}
