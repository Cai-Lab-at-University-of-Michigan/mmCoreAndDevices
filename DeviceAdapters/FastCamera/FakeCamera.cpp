///////////////////////////////////////////////////////////////////////////////
// FILE:          FakeCamera.cpp
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

#include "FakeCamera.h"

const char* cameraName = "FastCamera";

FakeCamera::FakeCamera() :
	initialized_(false),
	capturing_(false),
	byteCount_(2),
	exposure_(10),
	width_(2304),
	height_(2304),
	channels_(3)
{
	curImage_ = NULL;

	CreateProperty(MM::g_Keyword_Name, cameraName, MM::String, true);
	CreateProperty(MM::g_Keyword_Description, "??????", MM::String, true);
	CreateProperty(MM::g_Keyword_CameraName, "Fake camera adapter", MM::String, true);
	CreateProperty(MM::g_Keyword_CameraID, "FastCameraV0.1", MM::String, true);

	InitializeDefaultErrorMessages();
}

FakeCamera::~FakeCamera()
{
}

int FakeCamera::Initialize()
{
	initialized_ = true;
	return DEVICE_OK;
}

int FakeCamera::Shutdown()
{
	initialized_ = false;
	return DEVICE_OK;
}

void FakeCamera::GetName(char * name) const
{
	CDeviceUtils::CopyLimitedString(name, cameraName);
}

long FakeCamera::GetImageBufferSize() const
{
	return width_ * height_ * GetImageBytesPerPixel();
}

unsigned FakeCamera::GetBitDepth() const
{
	return 8 * byteCount_;
}

int FakeCamera::GetBinning() const
{
	return 1;
}

int FakeCamera::SetBinning(int binSize)
{
	return DEVICE_OK;
}

void FakeCamera::SetExposure(double exposure)
{
	exposure_ = exposure;
}

double FakeCamera::GetExposure() const
{
	return exposure_;
}

int FakeCamera::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
	return DEVICE_OK;
}

int FakeCamera::GetROI(unsigned & x, unsigned & y, unsigned & xSize, unsigned & ySize)
{
	return DEVICE_OK;
}

int FakeCamera::ClearROI()
{
	return DEVICE_OK;
}

int FakeCamera::IsExposureSequenceable(bool & isSequenceable) const
{
	isSequenceable = false;
	return DEVICE_OK;
}

const unsigned char * FakeCamera::GetImageBuffer()
{
	return NULL;
}

unsigned FakeCamera::GetNumberOfComponents() const
{
	return channels_;
}

unsigned FakeCamera::GetImageWidth() const
{
	return width_;
}

unsigned FakeCamera::GetImageHeight() const
{
	return height_;
}

unsigned FakeCamera::GetImageBytesPerPixel() const
{
	return byteCount_ * channels_;
}

int FakeCamera::SnapImage()
{
	MM::MMTime start = GetCoreCallback()->GetCurrentMMTime();
	++frameCount_;

	getImg();

	MM::MMTime end = GetCoreCallback()->GetCurrentMMTime();
	double dt = (end - start).getMsec();

	return DEVICE_OK;
}

int FakeCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
	capturing_ = true;
	return CCameraBase::StartSequenceAcquisition(numImages, interval_ms, stopOnOverflow);
}

int FakeCamera::StopSequenceAcquisition()
{
	capturing_ = false;
	return CCameraBase::StopSequenceAcquisition();
}

void FakeCamera::OnThreadExiting() throw()
{
	capturing_ = false;
	CCameraBase::OnThreadExiting();
}


void FakeCamera::getImg() const
{
	return;
}
