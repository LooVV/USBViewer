#pragma once
#include <vector>


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
	bool Checked = false;
};
//true::ok
bool UpdateList(std::vector<USBDeviceInfo>& USBDevices,
				std::vector<USBDeviceInfo>& RetiredDevices,
				std::vector<USBDeviceInfo*>& NewDevices);
