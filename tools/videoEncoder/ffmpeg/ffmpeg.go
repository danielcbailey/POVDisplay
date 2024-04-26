package ffmpeg

import (
	"fmt"
	"image"
	"os"
	"os/exec"
	"sort"
	"time"
)

type FramePreparationOptions struct {
	Width      int
	Height     int
	FrameRate  int
	CropX      int
	CropY      int
	CropWidth  int
	CropHeight int
	SkipPrep   bool
}

type FrameArray struct {
	frames       []string // path to each frame, in order
	timePerFrame time.Duration
}

func PrepareFrames(inputFile string, options FramePreparationOptions) (*FrameArray, error) {
	// call ffmpeg to prepare video
	if !options.SkipPrep {
		//cmd := exec.Command("ffmpeg", "-y", "-i", inputFile, "-r", fmt.Sprintf("%d", options.FrameRate), "-vf", fmt.Sprintf("scale=%d:%d", options.Width, options.Height), "intermediate.mp4")
		cmd := exec.Command("ffmpeg", "-y", "-i", inputFile, "-vf", fmt.Sprintf("scale=%d:%d", options.Width, options.Height), "intermediate.mp4")
		if e := cmd.Run(); e != nil {
			return nil, fmt.Errorf("error preparing video: %v", e)
		}

		// Removing the directory and all its files if it already exists
		os.RemoveAll("inFrames")
		os.Mkdir("inFrames", os.ModePerm)

		// call ffmpeg to crop video and dump frames
		cmd = exec.Command("ffmpeg", "-y", "-i", "intermediate.mp4", "-vf", fmt.Sprintf("crop=%d:%d:%d:%d", options.CropWidth, options.CropHeight, options.CropX, options.CropY), "inFrames/%04d.png")
		if e := cmd.Run(); e != nil {
			return nil, fmt.Errorf("error extracting frames: %v", e)
		}
	}

	// get list of frames
	dir, e := os.Open("inFrames")
	if e != nil {
		return nil, fmt.Errorf("error opening frame directory: %v", e)
	}

	files, e := dir.Readdirnames(0)
	if e != nil {
		return nil, fmt.Errorf("error reading frame directory: %v", e)
	}

	// sorting files
	sort.Slice(files, func(i, j int) bool {
		return files[i] < files[j]
	})

	arr := &FrameArray{frames: files, timePerFrame: time.Second / time.Duration(options.FrameRate)}
	return arr, nil
}

func (f *FrameArray) GetFrame(i int) image.Image {
	// reading image
	file, e := os.Open("inFrames/" + f.frames[i])
	if e != nil {
		panic(e)
	}

	img, _, e := image.Decode(file)
	if e != nil {
		panic(e)
	}

	return img
}

func (f *FrameArray) GetFrameTime() time.Duration {
	return f.timePerFrame
}

func (f *FrameArray) GetNumFrames() int {
	return len(f.frames)
}
