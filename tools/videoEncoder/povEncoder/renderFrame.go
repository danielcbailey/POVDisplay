package povencoder

import (
	"fmt"
	"image"
	"image/color"
	"os"
	"sync/atomic"

	"github.com/llgcode/draw2d/draw2dimg"
)

func RenderFrame(frame *EncodedFrame, prevFrame *EncodedFrame, width int) *image.RGBA {
	workingFrame := image.NewRGBA(image.Rect(0, 0, width, width))
	gc := draw2dimg.NewGraphicContext(workingFrame)
	if prevFrame != nil {
		renderPartialFrame(prevFrame, gc, width)
	}
	renderPartialFrame(frame, gc, width)

	return workingFrame
}

func renderPartialFrame(frame *EncodedFrame, gc *draw2dimg.GraphicContext, width int) {
	ledDistances := make([]float64, numLEDs)
	ledStride := (float64(width - 8)) / numLEDs
	ledWidth := ledStride / 2.0 // divided into 2 because the LEDs are interlaced
	for i := 0; i < numLEDs; i++ {
		ledDistances[i] = ledStride*float64(numLEDs/2-i) - ledStride/4.0 // to account for the interlacing of the LEDs
	}

	groupDThetas := make([]float64, len(groupPacketsPerSecond)) // see encoding.go for definition of groupPacketsPerSecond and other constants
	for i := 0; i < len(groupPacketsPerSecond); i++ {
		//packetsPerFrame := float64(groupPacketsPerSecond[i]) * frame.timeOfFrame.Seconds()
		//groupDThetas[i] = 3.1415 / packetsPerFrame
		groupDThetas[i] = 3.1415 / 2 / float64(len(frame.Segments[0].Groups[i].Packets)/8)
	}

	ledColors := make([]ledColor, numLEDs)

	for groupIdx := 0; groupIdx < len(groupPacketsPerSecond); groupIdx++ {
		i := -1
		segmentIdx := 0
	gLoop:
		for theta := frame.startAngle; theta < frame.startAngle+3.1415; theta += groupDThetas[groupIdx] {
			i++
			if i*8 >= len(frame.Segments[segmentIdx].Groups[groupIdx].Packets) {
				segmentIdx++
				if segmentIdx >= len(frame.Segments) {
					break gLoop
				}
				i = 0
			}
			packet := frame.Segments[segmentIdx].Groups[groupIdx].Packets[8*i : 8*i+8]

			// first updating the LED colors
			ledColors = updateLEDColorsFromPacket(packet, ledColors, groupIdx)

			// now draw the arcs
			for ledIdx := groupIdx * 32; ledIdx < (groupIdx+1)*32; ledIdx++ {
				drawArc(gc, theta, groupDThetas[groupIdx], ledDistances[ledIdx], ledWidth, width, color.RGBA{ledColors[ledIdx].r, ledColors[ledIdx].g, ledColors[ledIdx].b, 255})
			}
		}
	}
}

func updateLEDColorsFromPacket(packet []byte, ledColors []ledColor, groupIdx int) []ledColor {
	for i := 0; i < len(packet); i += 2 {
		cmd := packet[i]
		data := packet[i+1]

		physIdx := (cmd >> 1) - 0x10
		relLedIdx := int(7 - physIdx/3)
		ch := physIdx % 3

		chipIdx := 3 - i/2
		ledIdx := relLedIdx + chipIdx*ledsPerIC + groupIdx*32

		if ch == 0 {
			ledColors[ledIdx].r = data
		} else if ch == 1 {
			ledColors[ledIdx].g = data
		} else {
			ledColors[ledIdx].b = data
		}
	}

	return ledColors
}

func drawArc(gc *draw2dimg.GraphicContext, theta, dTheta float64, ledDistance float64, ledWidth float64, imgWidth int, c color.Color) {
	gc.BeginPath()
	gc.SetStrokeColor(c)
	gc.SetLineWidth(ledWidth)
	gc.ArcTo(float64(imgWidth/2), float64(imgWidth/2), ledDistance, ledDistance, -theta, -dTheta)
	gc.Stroke()
}

func RenderFrames(frames []*EncodedFrame, width int) {
	os.Mkdir("outFrames", os.ModePerm)

	threads := make([]chan bool, numThreads)
	amtDone := int32(0)
	for i := 0; numThreads > i; i++ {
		start := i * len(frames) / numThreads
		end := (i + 1) * len(frames) / numThreads
		if i == numThreads-1 {
			end = len(frames)
		}

		done := make(chan bool)
		go renderFramesWorker(frames, width, start, end, done, &amtDone)
		threads[i] = done
	}

	for i := 0; numThreads > i; i++ {
		<-threads[i]
	}
}

func renderFramesWorker(frames []*EncodedFrame, width int, start, end int, done chan bool, amtDone *int32) {
	for i := start; i < end; i++ {
		var img *image.RGBA
		if i == 0 {
			img = RenderFrame(frames[i], nil, width)
		} else {
			img = RenderFrame(frames[i], frames[i-1], width)
		}

		draw2dimg.SaveToPngFile(fmt.Sprintf("outFrames/%04d.png", i), img)
		fmt.Printf("Rendering. %.2f%%\n", float64(atomic.AddInt32(amtDone, 1))/float64(len(frames))*100)
	}
	done <- true
}
