package main

import (
	"fmt"
	"time"

	"github.com/danielcbailey/POVDisplay/tools/videoEncoder/ffmpeg"
	povencoder "github.com/danielcbailey/POVDisplay/tools/videoEncoder/povEncoder"
)

var inputFile = "C:\\Users\\dcoop\\Documents\\youtubedl\\myOhMy.webm"

func main() {
	prepOptions := ffmpeg.FramePreparationOptions{
		Width:      1280,
		Height:     720,
		FrameRate:  24,
		CropX:      (1280 - 720) / 2,
		CropY:      0,
		CropWidth:  720,
		CropHeight: 720,
		SkipPrep:   true, // for testing
	}

	fmt.Println("Preparing frames...")
	frames, err := ffmpeg.PrepareFrames(inputFile, prepOptions)
	if err != nil {
		panic(err)
	}
	fmt.Println("Frames prepared. n =", frames.GetNumFrames())

	tStart := time.Now()
	encodedFrames := povencoder.EncodeFrames(frames)
	fmt.Println("Time to encode frames: ", time.Since(tStart))

	povencoder.SaveEncodedVideo(encodedFrames, "outputFile.crv")

	//povencoder.RenderFrames(encodedFrames, 1280)
}
