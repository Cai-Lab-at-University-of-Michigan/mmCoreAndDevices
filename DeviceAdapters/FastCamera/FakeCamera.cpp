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
#include<iostream>

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
	blankImage_ = (unsigned char *) malloc(GetImageBufferSize());
	oneImage_ = (unsigned char*) malloc(GetImageBufferSize());

	unsigned short* oneImageCast_ = (unsigned short *) oneImage_;
	for (int i = 0; i < GetImageWidth() * GetImageHeight(); i++) {
		oneImageCast_[i] = 500;
	}

	curImage_ = NULL;

	CreateProperty(MM::g_Keyword_Name, cameraName, MM::String, true);
	CreateProperty(MM::g_Keyword_Description, "??????", MM::String, true);
	CreateProperty(MM::g_Keyword_CameraName, "Fake camera adapter", MM::String, true);
	CreateProperty(MM::g_Keyword_CameraID, "FastCameraV0.1", MM::String, true);

	CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false);
	std::vector<std::string> binningValues;
	binningValues.push_back("1");
	SetAllowedValues(MM::g_Keyword_Binning, binningValues);

	InitializeDefaultErrorMessages();
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
	return GetImageWidth() * GetImageHeight() * GetImageBytesPerPixel();
}

int FakeCamera::IsExposureSequenceable(bool & isSequenceable) const
{
	isSequenceable = false;
	return DEVICE_OK;
}

const unsigned char* FakeCamera::GetImageBuffer()
{
	return GetImageBuffer(0);
}

const unsigned char * FakeCamera::GetImageBuffer(unsigned channelNr)
{
	if (channelNr % 2 == 1) return oneImage_;
	if (curImage_ == NULL) return blankImage_;
	return curImage_;
}

int FakeCamera::SnapImage()
{
	MM::MMTime start = GetCoreCallback()->GetCurrentMMTime();
	++frameCount_;

	getImg();

	MM::MMTime end = GetCoreCallback()->GetCurrentMMTime();
	double dt = (end - start).getMsec();

	//std::cout << "Snap Called " << dt << std::endl;
	return DEVICE_OK;
}

int FakeCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
	std::cout << "Starting sequence acquisition " << numImages << '-' << interval_ms << std::endl;
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
