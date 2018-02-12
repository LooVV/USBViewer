#pragma once
#include <vector>
#include <SetupAPI.h>

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
		if (BusReportedDeviceDescription != "")
			return (TCHAR*)BusReportedDeviceDescription.GetString();
		else
			return (TCHAR*)Description.GetString();
	}
};
//true::ok
bool UpdateInfo(std::vector<USBDeviceInfo>& USBDevices );
DWORD ChangeDevState(USBDeviceInfo& Info, DeviceState NewState);