///////////////////////////////////////////////////////////////////////////////
// FILE:          Newport.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Newport Controller Driver
//
// AUTHOR:		  Logan Walker, 12/17/2021, translated previous SMC driver developed
// by authors below into one compatible with the ESP302 stage from Newport
// AUTHOR:        Liisa Hirvonen, 03/17/2009
// AUTHOR:        Nico Stuurman 08/18/2005, added velocity, multiple addresses, 
// enabling multiple controllers, relative position, easier busy check and multiple
//  fixes for annoying behavior, see repository logs for complete list
// COPYRIGHT:     University of Melbourne, Australia, 2009-2013
// COPYRIGHT:     Regents of the University of California, 2015
// COPYRIGHT:     University of Michigan, 2021-22
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//

#ifdef WIN32
#include <windows.h>
#endif
#include "FixSnprintf.h"

#include "Newport.h"
#include <string>
#include <math.h>
#include "ModuleInterface.h"
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>

const char* g_Newport_ZStageDeviceName = "NewportESP302Stage";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_Newport_ZStageDeviceName, MM::StageDevice, "Newport ESP302 Controller (1-axis)");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0 || strcmp(deviceName, g_Newport_ZStageDeviceName) != 0) return 0;
	return new NewportZStage();
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}


///////////////////////////////////////////////////////////////////////////////
// NewportZStage

NewportZStage::NewportZStage() :
	port_("Undefined"),
	stepSizeUm_(1),
	conversionFactor_(1000.0),    // divide request by this number
				  // to accomodate for units in mm rather than microns
	cAddress_(1),
	initialized_(false),
	lowerLimit_(-50),  // limit in native coordinates
	upperLimit_(50), // limit in native coordinates
	velocity_(5.0),
	velocityLowerLimit_(0.000001),
	velocityUpperLimit_(100000000000.0)
{
	InitializeDefaultErrorMessages();

	SetErrorText(ERR_POSITION_BEYOND_LIMITS, "Requested position is beyond the limits of this stage");
	SetErrorText(ERR_TIMEOUT, "Timed out waiting for command to complete.  Try increasing the Core-TimeoutMs property if this was premature");

	// create properties
	// ------------------------------------

	// Name
	CreateProperty(MM::g_Keyword_Name, g_Newport_ZStageDeviceName, MM::String, true);

	// Description
	CreateProperty(MM::g_Keyword_Description, "Newport ESP302 Controller (1-axis)", MM::String, true);

	// Port
	CPropertyAction* pAct = new CPropertyAction(this, &NewportZStage::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	// Conversion factor
	pAct = new CPropertyAction(this, &NewportZStage::OnConversionFactor);
	CreateFloatProperty("Conversion Factor", conversionFactor_, false, pAct, true);

	// Maximum allowed position (will only be used if smaller than the hardware limit
	pAct = new CPropertyAction(this, &NewportZStage::OnMaxPosition);
	CreateFloatProperty("Max Position (mm)", 50, false, pAct, true);
	pAct = new CPropertyAction(this, &NewportZStage::OnMinPosition);
	CreateFloatProperty("Min Position (mm)", -50, false, pAct, true);

	// Controller address
	pAct = new CPropertyAction(this, &NewportZStage::OnControllerAddress);
	CreateIntegerProperty("Controller Address", cAddress_, false, pAct, true);
	SetPropertyLimits("Controller Address", 1, 31);
}


NewportZStage::~NewportZStage()
{
	Shutdown();
}

void NewportZStage::GetName(char* Name) const
{
	CDeviceUtils::CopyLimitedString(Name, g_Newport_ZStageDeviceName);
}

int NewportZStage::Initialize()
{
	std::string command;
	std::string answer;
	int ret;

	std::cout << "Newport Z Stage Initializing." << std::endl;

	// Send the "homing" command to init stage
	std::cout << '\t' << "Sending homing signal..." << std::endl;
	ret = SetOrigin();
	if (ret != DEVICE_OK) return ret;

	// Position property
	std::ostringstream oss;
	double pos = 0.0;
	ret = GetPositionUm(pos);
	if (ret != DEVICE_OK) return ret;
	oss << pos;
	CPropertyAction* pAct = new CPropertyAction(this, &NewportZStage::OnPosition);
	CreateProperty(MM::g_Keyword_Position, oss.str().c_str(), MM::Float, false, pAct);
	SetPropertyLimits(MM::g_Keyword_Position, lowerLimit_ * conversionFactor_, upperLimit_ * conversionFactor_);

	initialized_ = true;
	return DEVICE_OK;
}

int NewportZStage::Shutdown()
{
	initialized_ = false;
	return DEVICE_OK;
}

bool NewportZStage::Busy()
{
	// Ask position
	std::string command = MakeCommand("TS");
	int ret = SendSerialCommand(port_.c_str(), command.c_str(), "\n");
	if (ret != DEVICE_OK) return ret;

	// Receive answer
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK) return ret;

	char return_signal = answer.c_str()[0];
	std::cout << "Recieved Busy Signal: " << return_signal << std::endl;
	bool status = return_signal & (1 << 2);
	std::cout << "Intepreting Busy Status = " << status << std::endl;
	return status; // need to check if this bitmask is correct
}

int NewportZStage::SetPositionSteps(long steps)
{
	std::cout << "Reached SetPositionSteps" << std::endl;
	double pos = steps * stepSizeUm_;
	return SetPositionUm(pos);
}

int NewportZStage::GetPositionSteps(long& steps)
{
	std::cout << "Reached GetPositionSteps" << std::endl;
	double pos;
	int ret = GetPositionUm(pos);
	if (ret != DEVICE_OK)
		return ret;
	steps = (long)(pos / stepSizeUm_);
	return DEVICE_OK;
}

int NewportZStage::SetPositionUm(double pos)
{
	std::cout << "Setting Position: " << pos << std::endl;
	//WaitForBusy();

	// convert from micron to mm:
	pos /= conversionFactor_;

	// compare position to limits (in native units)
	if (pos > upperLimit_ || lowerLimit_ > pos)
	{
		return ERR_POSITION_BEYOND_LIMITS;
	}

	// Send the "move" command
	std::ostringstream command;
	std::string answer;
	command << MakeCommand("PA") << pos;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK) return ret;

	return DEVICE_OK;
}

int NewportZStage::SetRelativePositionUm(double pos)
{
	std::cout << "Setting Relative Position: " << pos << std::endl;
	//WaitForBusy();

	// convert from micron to mm:
	pos /= conversionFactor_;

	// Send the "relative move" command
	std::ostringstream command;
	command << MakeCommand("PR") << pos;
	int ret = SendSerialCommand(port_.c_str(), command.str().c_str(), "\n");
	if (ret != DEVICE_OK) return ret;

	return DEVICE_OK;
}

int NewportZStage::GetPositionUm(double& pos)
{
	// Ask for axis position
	std::string command = MakeCommand("PA?");
	int ret = SendSerialCommand(port_.c_str(), command.c_str(), "\n");
	if (ret != DEVICE_OK) return ret;

	// Receive answer
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK) return ret;
	std::cout << "GetPosition Reply:" << answer;

	// Get the value from the reply string
	pos = atof(answer.c_str());
	pos *= conversionFactor_;

	return DEVICE_OK;
}



int NewportZStage::SetOrigin()
{
	// Send the "homing" command to init stage
	std::cout << "Sending Homing Signal..." << std::endl;
	std::string command = MakeCommand("OR");
	int ret = SendSerialCommand(port_.c_str(), command.c_str(), "\n");
	if (ret != DEVICE_OK) return ret;

	return WaitForBusy();
}

int NewportZStage::GetLimits(double& lowerLimit, double& upperLimit)
{
	lowerLimit = lowerLimit_ * conversionFactor_;
	upperLimit = upperLimit_ * conversionFactor_;

	return DEVICE_OK;
}

int NewportZStage::GetError(bool& error, std::string& errorCode)
{
	std::cout << "Reached GetError" << std::endl;

	// Ask for error message
	std::string cmd = MakeCommand("TE");
	LogMessage(cmd);

	int ret = SendSerialCommand(port_.c_str(), cmd.c_str(), "\n");
	if (ret != DEVICE_OK)
		return ret;

	// Receive error message
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK)
		return ret;

	// Check that there is no error
	std::ostringstream dbg;
	errorCode = answer.substr(cmd.size(), 1);

	error = true;
	if (answer.substr(cmd.size(), 1) == "@")
	{
		error = false;
		return DEVICE_OK;
	}

	if (answer.substr(cmd.size(), 1) == "H") // state is not referenced, so home and ask again
	{
		ret = SetOrigin();
		if (ret != DEVICE_OK)
		{
			return ret;
		}
		return GetError(error, errorCode);  // dangeroud! recursive call
	}

	std::string msg = "Device returned error code: " + errorCode;
	LogMessage(msg, true);

	SetErrorText(CONTROLLER_ERROR, msg.c_str());
	return CONTROLLER_ERROR;
}

int NewportZStage::WaitForBusy()
{
	std::cout << "Reached WaitForBusy" << std::endl;
	//char coreTimeout[MM::MaxStrLength];
	//GetCoreCallback()->GetDeviceProperty("Core", "TimeoutMs", coreTimeout);
	//long to = atoi(coreTimeout);
	//MM::TimeoutMs timeout(GetCurrentMMTime(), to);
	while (Busy()) {
		//if (timeout.expired(GetCurrentMMTime())) 
		//{
		//   return ERR_TIMEOUT;
		//}
		std::this_thread::sleep_for(std::chrono::microseconds(50));
	}
	return DEVICE_OK;
}

/**
 * Utility function to read values that are returned with the command
 * such as the software limits
 */
int NewportZStage::GetValue(const char* cmd, double& val)
{
	return DEVICE_OK;
}


/**
 * Sets the velocity of this stage
 * Uses device native values (i.e. mm/sec)
 */
int NewportZStage::SetVelocity(double velocity)
{
	std::ostringstream os;
	os << MakeCommand("VA") << velocity;

	// Set Velocity
	return SendSerialCommand(port_.c_str(), os.str().c_str(), "\n");
}

/**
 * Enquires the device about the current value of its velocity property
 * Uses device native values (i.e. mm/sec)
 */
int NewportZStage::GetVelocity(double& velocity)
{
	// Ask about velocity
	std::string cmd = MakeCommand("VA?");
	int ret = SendSerialCommand(port_.c_str(), cmd.c_str(), "\n");
	if (ret != DEVICE_OK) return ret;

	// Receive answer
	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), "\n", answer);
	if (ret != DEVICE_OK) return DEVICE_OK;

	velocity = atof( answer.c_str() );
	std::ostringstream os;
	os << answer << ", " << velocity;
	LogMessage(os.str().c_str());

	return DEVICE_OK;
}


/**
 * Asks the controller for all its axis information
 * Now only used to read the maximum velocity
 * Can be extended in the future to store more information about the controller
 * and drive
 */
int NewportZStage::GetControllerInfo()
{
	// length of controller address as returned by controller in chars
	 /*
	int offset = 1;
	if (cAddress_ > 9)
	   offset = 2;
	std::string command = MakeCommand("ZT");
	int ret = SendSerialCommand(port_.c_str(), command.c_str(), "\n");
	if (ret != DEVICE_OK)
	   return ret;
	std::string answer;
	bool done = false;
	while (ret == DEVICE_OK && !done) { // keep on reading until we time out
	   ret = GetSerialAnswer(port_.c_str(), "\r\n", answer);
	   LogMessage(answer.c_str(), true);
	   if (answer.substr(offset, 2) == "SL") {
		   lowerLimit_ = atof(answer.substr(offset + 2).c_str());
	   }
	   if (answer.substr(offset, 2) == "SR") {
		   double tmp = atof(answer.substr(offset + 2).c_str());
		   if (tmp < upperLimit_)
			   upperLimit_ = tmp;
	   }
	   if (answer.substr(offset,2) == "VA") {
		  velocityUpperLimit_ = atof(answer.substr(offset + 2).c_str());
	   }
	   if (answer.substr(offset,3) == "PW0") {
		  done = true;
	   }
	}
	return ret;*/
	return DEVICE_OK;
}


/**
 * Utility function that prepends the command with the
 * current device Address (set as a pre-init property, 1-31)
 */
std::string NewportZStage::MakeCommand(const char* cmd)
{
	std::cout << "Making command " << '"' << cmd << '"' << std::endl;
	std::ostringstream os;
	os << cAddress_ << cmd;
	return os.str();
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int NewportZStage::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return ERR_PORT_CHANGE_FORBIDDEN;
		}
		pProp->Get(port_);
	}

	return DEVICE_OK;
}

int NewportZStage::OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double pos;
		int ret = GetPositionUm(pos);
		if (ret != DEVICE_OK)
			return ret;
		pProp->Set(pos);
	}
	else if (eAct == MM::AfterSet)
	{
		double pos;
		pProp->Get(pos);
		int ret = SetPositionUm(pos);
		if (ret != DEVICE_OK)
		{
			pProp->Set(pos); // revert
			return ret;
		}
	}

	return DEVICE_OK;
}

int NewportZStage::OnConversionFactor(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(conversionFactor_);
	}
	else if (eAct == MM::AfterSet)
	{
		double pos;
		pProp->Get(pos);
		conversionFactor_ = pos;
	}

	return DEVICE_OK;
}

int NewportZStage::OnMaxPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(upperLimit_);
	}
	else if (eAct == MM::AfterSet)
	{
		double pos;
		pProp->Get(pos);
		upperLimit_ = pos;
	}

	return DEVICE_OK;
}

int NewportZStage::OnMinPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(lowerLimit_);
	}
	else if (eAct == MM::AfterSet)
	{
		double pos;
		pProp->Get(pos);
		lowerLimit_ = pos;
	}

	return DEVICE_OK;
}

int NewportZStage::OnControllerAddress(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set((long)cAddress_);
	}
	else if (eAct == MM::AfterSet)
	{
		long pos;
		pProp->Get(pos);
		cAddress_ = (int)pos;
	}

	return DEVICE_OK;
}

int NewportZStage::OnVelocity(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		int ret = GetVelocity(velocity_);
		if (ret != DEVICE_OK)
			return DEVICE_OK;
		pProp->Set((double)velocity_);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(velocity_);
		return SetVelocity(velocity_);
	}

	return DEVICE_OK;
}
