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

#define INTERNAL_DEBUG_MODE 0

const char* cameraName = "FastCamera";

FakeCamera::FakeCamera() :
	initialized_(false),
	byteCount_(2),
	exposure_(10),
	width_(2304),
	height_(2304),
	channels_(4),
	components_(1),
	bitDepth_(16),
	liveThread_(0)
{
	liveThread_ = new LiveThread(this);

	blankImage_ = (unsigned char *) malloc(GetImageBufferSize());
	oneImage_ = (unsigned char*) malloc(GetImageBufferSize());

	unsigned short* oneImageCast_ = (unsigned short *) oneImage_;

	//for (unsigned short i = 0; i < GetImageWidth() * GetImageHeight() * GetNumberOfChannels(); i++) {
	//	oneImageCast_[i] = i / (GetImageWidth() * GetImageHeight());
	//}

	curImage_ = NULL;
	
	CreateProperty(MM::g_Keyword_Name, cameraName, MM::String, true);
	CreateProperty(MM::g_Keyword_Description, "[??????]", MM::String, true);
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
	delete liveThread_;
	liveThread_ = 0;
	return DEVICE_OK;
}

void FakeCamera::GetName(char * name) const
{
	CDeviceUtils::CopyLimitedString(name, cameraName);
}

long FakeCamera::GetImageBufferSize() const
{
	return GetImageWidth() * GetImageHeight() * GetImageBytesPerPixel() * GetNumberOfChannels();
}

int FakeCamera::IsExposureSequenceable(bool & isSequenceable) const
{
	isSequenceable = false;
	return DEVICE_OK;
}

const unsigned char* FakeCamera::GetImageBuffer()
{
	if(INTERNAL_DEBUG_MODE)
		std::cout << "requestedImageBuffer [DEF]" << std::endl;
	return GetImageBuffer(0);
}

const unsigned char * FakeCamera::GetImageBuffer(unsigned channelNr)
{
	if(INTERNAL_DEBUG_MODE)
		std::cout << "requestedImageBuffer [" << channelNr << "]" << std::endl;
	return oneImage_ + (channelNr * GetImageWidth() * GetImageHeight() * GetImageBytesPerPixel());
}

int FakeCamera::SnapImage()
{
	if(INTERNAL_DEBUG_MODE)
		std::cout << "Snap Called: " << frameCount_ << std::endl;
	
	MM::MMTime start = GetCoreCallback()->GetCurrentMMTime();
	++frameCount_;

	getImg();

	MM::MMTime end = GetCoreCallback()->GetCurrentMMTime();
	double dt = (end - start).getMsec();

	std::cout << "Snap Finished " << dt << std::endl;
	return DEVICE_OK;
}

int FakeCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
	if(INTERNAL_DEBUG_MODE)
		std::cout << "Starting sequence acquisition " << numImages << '-' << interval_ms << std::endl;
	
	if (IsCapturing()) return DEVICE_CAMERA_BUSY_ACQUIRING;

	GetCoreCallback()->PrepareForAcq(this);

	// Start Camera

	liveThread_->SetNumImages(numImages);
	startTime_ = GetCoreCallback()->GetCurrentMMTime();
	liveThread_->activate();

	return DEVICE_OK;
	//return CCameraBase::StartSequenceAcquisition(numImages, interval_ms, stopOnOverflow);
}

int FakeCamera::StopSequenceAcquisition()
{
	// Stop Camera
	liveThread_->Abort();
	GetCoreCallback()->AcqFinished(this, 0);
	return DEVICE_OK;
	//return CCameraBase::StopSequenceAcquisition();
}

bool FakeCamera::IsCapturing() {
	return liveThread_->IsRunning();
}

void FakeCamera::OnThreadExiting() throw()
{
	CCameraBase::OnThreadExiting();
}


void FakeCamera::getImg() const {
	return;
}

int FakeCamera::LiveThread::svc()
{
	stopRunning_ = false;
	running_ = true;
	imageCounter_ = 0;

	// put the hardware into a continuous acqusition state
	while (true) {
		if (stopRunning_)
			break;

		int ret = cam_->SnapImage();

		if (ret != DEVICE_OK) {
			char txt[1000];
			sprintf(txt, "FastCamera Live Thread: ImageSnap() error %d", ret);
			cam_->GetCoreCallback()->LogMessage(cam_, txt, false);
			break;
		}

		char label[MM::MaxStrLength];

		cam_->GetLabel(label);

		MM::MMTime timestamp = cam_->GetCurrentMMTime();
		Metadata md;

		MetadataSingleTag mstStartTime(MM::g_Keyword_Metadata_StartTime, label, true);
		mstStartTime.SetValue(CDeviceUtils::ConvertToString(cam_->startTime_.getMsec()));
		md.SetTag(mstStartTime);

		MetadataSingleTag mstElapsed(MM::g_Keyword_Elapsed_Time_ms, label, true);
		MM::MMTime elapsed = timestamp - cam_->startTime_;
		mstElapsed.SetValue(CDeviceUtils::ConvertToString(elapsed.getMsec()));
		md.SetTag(mstElapsed);

		MetadataSingleTag mstCount(MM::g_Keyword_Metadata_ImageNumber, label, true);
		mstCount.SetValue(CDeviceUtils::ConvertToString(imageCounter_));
		md.SetTag(mstCount);

		// insert all channels
		for (unsigned i = 0; i < cam_->GetNumberOfChannels(); i++)
		{
			char buf[MM::MaxStrLength];
			MetadataSingleTag mstChannel(MM::g_Keyword_CameraChannelIndex, label, true);
			snprintf(buf, MM::MaxStrLength, "%d", i);
			mstChannel.SetValue(buf);
			md.SetTag(mstChannel);

			MetadataSingleTag mstChannelName(MM::g_Keyword_CameraChannelName, label, true);
			cam_->GetChannelName(i, buf);
			mstChannelName.SetValue(buf);
			md.SetTag(mstChannelName);

			ret = cam_->GetCoreCallback()->InsertImage(cam_, cam_->GetImageBuffer(i),
				cam_->GetImageWidth(),
				cam_->GetImageHeight(),
				cam_->GetImageBytesPerPixel(),
				md.Serialize().c_str());
			if (ret == DEVICE_BUFFER_OVERFLOW) {
				cam_->GetCoreCallback()->ClearImageBuffer(cam_);
				cam_->GetCoreCallback()->InsertImage(cam_, cam_->GetImageBuffer(i),
					cam_->GetImageWidth(),
					cam_->GetImageHeight(),
					cam_->GetImageBytesPerPixel(),
					md.Serialize().c_str());
			}
			else if (ret != DEVICE_OK) {
				cam_->GetCoreCallback()->LogMessage(cam_, "BitFlow thread: error inserting image", false);
				break;
			}
		}


		imageCounter_++;
		if (numImages_ >= 0 && imageCounter_ >= numImages_) {
			cam_->StopSequenceAcquisition();
		}
		//{
		//	cam_->bfDev_.StopContinuousAcq();
		//	break;
		//}

	}
	running_ = false;
	return 0;
}

void FakeCamera::LiveThread::Abort() {
	stopRunning_ = true;
	wait();
}
