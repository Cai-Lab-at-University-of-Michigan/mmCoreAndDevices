///////////////////////////////////////////////////////////////////////////////
// FILE:          FakeCamera.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   A camera implementation that is backed by the file system
//                Can access stage positions to choose image to display
//
// AUTHOR:        Lukas Lang
//
// COPYRIGHT:     2017 Lukas Lang
// LICENSE:       Licensed under the Apache License, Version 2.0 (the "License");
//                you may not use this file except in compliance with the License.
//                You may obtain a copy of the License at
//                
//                http://www.apache.org/licenses/LICENSE-2.0
//                
//                Unless required by applicable law or agreed to in writing, software
//                distributed under the License is distributed on an "AS IS" BASIS,
//                WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//                See the License for the specific language governing permissions and
//                limitations under the License.

#pragma once

#include <string>

#include "DeviceBase.h"

extern const char* cameraName;

class parse_error : public std::exception {};

class FakeCamera : public CCameraBase<FakeCamera>
{
public:
	FakeCamera();
	~FakeCamera() {};

	// Inherited via CCameraBase
	int Initialize();
	int Shutdown();
	void GetName(char * name) const;
	long GetImageBufferSize() const;
	unsigned GetBitDepth() const { return 8 * byteCount_; };
	int GetBinning() const { return 1; };
	int SetBinning(int binSize) { return DEVICE_OK; };
	void SetExposure(double exp_ms) { exposure_ = exp_ms; };
	double GetExposure() const { return exposure_; };
	int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize) { return DEVICE_OK; }
	int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize) { return DEVICE_OK; };
	int ClearROI() { return DEVICE_OK; };
	int IsExposureSequenceable(bool & isSequenceable) const;
	const unsigned char * GetImageBuffer();
	const unsigned char * GetImageBuffer(unsigned channelNr);
	unsigned GetImageWidth() const { return width_; };
	unsigned GetImageHeight() const { return height_; };
	unsigned GetImageBytesPerPixel() const { return byteCount_; };
	int SnapImage();
	int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
	int StopSequenceAcquisition();
	void OnThreadExiting() throw();

	unsigned GetNumberOfComponents() const { return 1;  };
	unsigned GetNumberOfChannels() const { return channels_; };

	void getImg() const;

private:
	bool initialized_;
	int frameCount_;
	bool capturing_;
	mutable unsigned width_;
	mutable unsigned height_;
	mutable unsigned channels_;
	unsigned byteCount_;
	double exposure_;
	unsigned char* curImage_;
	unsigned char* blankImage_;
	unsigned char* oneImage_;
};