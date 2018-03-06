#pragma once
#include <vector>
#include <SetupAPI.h>

namespace USB{

enum class DeviceState {
	ENABLED,
	DISABLED,
	UNDEFINED
};
enum class EntryState {
	CHECKED,
	UNCHECKED,
	NEW,
	CHANGED_STATE
};
enum class ChangeStateResult
{
	OK,
	NEED_ADMIN,
	NOT_DISABLEABLE,
	WRONG_REQUEST,
	NEED_USE_UPDATE,
	UNKNOWN_ERROR
};

struct USBDeviceInfo 
{
	CString InstanceID;
	CString Description;
	std::vector<CString> HardwareIDs;
	CString BusReportedDeviceDescription;
	CString Manufacturer;
	CString FriendlyName;
	CString LocationInfo;
	CString DisplayCategory;
	CString Vid;
	CString Pid;
	CString Mi;

	DeviceState DevState;
	EntryState EntState;

	SP_DEVINFO_DATA DevInfoData;

	const TCHAR* DeviceInfoString() const
	{
		if ( !BusReportedDeviceDescription.IsEmpty())
			return BusReportedDeviceDescription.GetString();
		else
			return Description.GetString();
	}
};

using DeviceInfoSet = std::vector<USBDeviceInfo>;

// true - success
bool UpdateInfo(DeviceInfoSet& USBDevices);
ChangeStateResult ChangeDevState(USBDeviceInfo& Info, DeviceState NewState);
void ReleaseDevs();


}