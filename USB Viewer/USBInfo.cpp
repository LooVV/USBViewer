#include "stdafx.h"
#include "USBInfo.h"
#include <algorithm>

#include <windows.h>
#include <devguid.h>    
#include <setupapi.h>
#include <cfgmgr32.h>   
#include <clocale>
#define INITGUID
#include <tchar.h>

#pragma comment(lib, "SetupAPI")

#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY name
#endif // INITGUID

// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpkey.h
DEFINE_DEVPROPKEY(DEVPKEY_Device_BusReportedDeviceDesc, 0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4);     // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_DeviceDisplay_Category, 0x78c34fc8, 0x104a, 0x4aca, 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57, 0x5a);  // DEVPROP_TYPE_STRING_LIST
DEFINE_DEVPROPKEY(DEVPKEY_Device_LocationInfo, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 15);    // DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_Manufacturer, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 13);    // DEVPROP_TYPE_STRING

#define ARRAY_SIZE(arr)     (sizeof(arr)/sizeof(arr[0]))

#define USB_ENUMERATOR  L"USB"

static HDEVINFO hDevInfo = INVALID_HANDLE_VALUE;

static bool operator==(const SP_DEVINFO_DATA& lhv, const SP_DEVINFO_DATA& rhv)
{
	return lhv.cbSize == rhv.cbSize &&
			lhv.ClassGuid == rhv.ClassGuid &&
			lhv.DevInst == rhv.DevInst &&
			lhv.Reserved == rhv.Reserved;
}

static DeviceState GetDeviceState(DWORD DeviceInstance)
{
	ULONG devStatus, devProblemCode;
	auto ret = CM_Get_DevNode_Status(&devStatus, &devProblemCode, DeviceInstance, 0);
	switch (ret)
	{
	case CR_SUCCESS:
	{
		if (devStatus & DN_STARTED)
		{
			return DeviceState::ENABLED;
		}
		else if (devProblemCode & CM_PROB_DISABLED)
		{
			return DeviceState::DISABLED;
		}
	} break;

	default:
	{
		MessageBox(NULL, L"Error", L"errpr",0);
		// some error
	}
	}
	return DeviceState::UNDEFINED;
}

static void FillDetailInfo(USBDeviceInfo& Info, HDEVINFO& hDevInfo, SP_DEVINFO_DATA& DeviceInfoData, TCHAR szDeviceInstanceID[MAX_DEVICE_ID_LEN])
{ 
	DWORD dwSize, dwPropertyRegDataType;
	TCHAR szDesc[1024], szHardwareIDs[4096];
	WCHAR szBuffer[4096];
	DEVPROPTYPE ulPropertyType;
	LPTSTR pszToken, pszNextToken;
	TCHAR szVid[MAX_DEVICE_ID_LEN], szPid[MAX_DEVICE_ID_LEN], szMi[MAX_DEVICE_ID_LEN];
	const static LPCTSTR arPrefix[3] = { TEXT("VID_"), TEXT("PID_"), TEXT("MI_") };


	if (SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC,
		&dwPropertyRegDataType, (BYTE*)szDesc,
		sizeof(szDesc),   // The size, in bytes
		&dwSize))
		Info.Description = { szDesc };

	if (SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_HARDWAREID,
		&dwPropertyRegDataType, (BYTE*)szHardwareIDs,
		sizeof(szHardwareIDs),    // The size, in bytes
		&dwSize)) {
		LPCTSTR pszId;

		for (pszId = szHardwareIDs;
			*pszId != TEXT('\0') && pszId + dwSize / sizeof(TCHAR) <= szHardwareIDs + ARRAYSIZE(szHardwareIDs);
			pszId += lstrlen(pszId) + 1) {

			Info.HardwareIDs.push_back(pszId);
		}
	}

	// Retreive the device description as reported by the device itself
	// On Vista and earlier, we can use only SPDRP_DEVICEDESC
	// On Windows 7, the information we want ("Bus reported device description") is
	// accessed through DEVPKEY_Device_BusReportedDeviceDesc

	if (SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
		&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0)) {

		if (SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc,
			&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
		{
			Info.BusReportedDeviceDescription = szBuffer;
		}
		if (SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_Manufacturer,
			&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
		{
			Info.Manufacturer = szBuffer;
		}
		if (SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_FriendlyName,
			&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
		{
			Info.FriendlyName = szBuffer;
		}
		if (SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_LocationInfo,
			&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
		{
			Info.LocationInfo = szBuffer;
		}

		if (SetupDiGetDevicePropertyW(hDevInfo, &DeviceInfoData, &DEVPKEY_DeviceDisplay_Category,
			&ulPropertyType, (BYTE*)szBuffer, sizeof(szBuffer), &dwSize, 0))
		{
			Info.DisplayCategory = szBuffer;
		}
	}

	pszToken = _tcstok_s(szDeviceInstanceID, TEXT("\\#&"), &pszNextToken);
	while (pszToken != NULL) {
		szVid[0] = TEXT('\0');
		szPid[0] = TEXT('\0');
		szMi[0] = TEXT('\0');
		for (unsigned int j = 0; j < 3; j++) {
			if (_tcsncmp(pszToken, arPrefix[j], lstrlen(arPrefix[j])) == 0) {
				switch (j) {
				case 0:
					_tcscpy_s(szVid, ARRAY_SIZE(szVid), pszToken);
					break;
				case 1:
					_tcscpy_s(szPid, ARRAY_SIZE(szPid), pszToken);
					break;
				case 2:
					_tcscpy_s(szMi, ARRAY_SIZE(szMi), pszToken);
					break;
				default:
					break;
				}
			}
		}
		if (szVid[0] != TEXT('\0'))
			Info.Vid = szVid + 4;
		if (szPid[0] != TEXT('\0'))
			Info.Pid = szPid + 4;
		if (szMi[0] != TEXT('\0'))
			Info.Mi = szMi + 3;
		pszToken = _tcstok_s(NULL, TEXT("\\#&"), &pszNextToken);
	}
}

// List all USB devices with some additional information                  
bool UpdateInfo(std::vector<USBDeviceInfo>& USBDevices )
{
	unsigned i;
	CONFIGRET status;
	SP_DEVINFO_DATA DeviceInfoData;
	TCHAR szDeviceInstanceID[MAX_DEVICE_ID_LEN];
	
	// List all connected USB devices
	if(hDevInfo != INVALID_HANDLE_VALUE)
		SetupDiDestroyDeviceInfoList(hDevInfo);

	hDevInfo = SetupDiGetClassDevs(NULL, USB_ENUMERATOR, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return false;
	
	DeviceInfoData.cbSize = sizeof(DeviceInfoData);
	
	for (i = 0; ; i++) 
	{
		//getting device info
		if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData))
			break;

		//getting ID
		status = CM_Get_Device_ID(DeviceInfoData.DevInst, szDeviceInstanceID, MAX_PATH, 0);
		if (status != CR_SUCCESS)
			continue;

		// Display device instance ID
		CString ID{ szDeviceInstanceID };
		DeviceState state = GetDeviceState(DeviceInfoData.DevInst);
		
		auto Device = std::find_if(USBDevices.begin(), USBDevices.end(),
									[&](const USBDeviceInfo& Info) {
										return Info.InstanceID == ID;
									});
		//device still in list
		if (Device != USBDevices.end())
		{
			//check state change
			if (Device->DevState == state)
			{
				Device->EntState = EntryState::CHECKED;
			}
			else
			{
				Device->EntState = EntryState::CHANGED_STATE;
				Device->DevState = state;
			}
			//update DevInfoData if needed
			if (!(Device->DevInfoData == DeviceInfoData))
			{
				Device->DevInfoData = DeviceInfoData;
			}
			continue;
		}

		//its new device, need to fill info
		USBDeviceInfo Info;
		Info.InstanceID = ID;
		Info.DevState = state;
		Info.EntState = EntryState::NEW;
		Info.DevInfoData = DeviceInfoData;

		FillDetailInfo(Info, hDevInfo, DeviceInfoData, szDeviceInstanceID);

		USBDevices.push_back(std::move(Info));
	}
	//SetupDiDestroyDeviceInfoList(hDevInfo);
	return true;
}

DWORD ChangeDevState(USBDeviceInfo& Info, DeviceState NewState)
{
	if (NewState == DeviceState::UNDEFINED)
		return 1;
	if (Info.DevState == NewState)
		return 0;

	
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return false;

	SP_PROPCHANGE_PARAMS params;

	params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
	params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;

	if( NewState == DeviceState::DISABLED )
		params.StateChange = DICS_DISABLE;
	else if(NewState == DeviceState::ENABLED)
		params.StateChange = DICS_ENABLE;

	params.Scope = DICS_FLAG_GLOBAL;
	DWORD Error = 0;
	
	// setup proper parameters            
	if (!SetupDiSetClassInstallParams(hDevInfo, &(Info.DevInfoData), &params.ClassInstallHeader, sizeof(params))) 
	{
		Error = GetLastError();
		return Error;
	}

	// use parameters
	if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, &(Info.DevInfoData))) {
		Error = GetLastError(); // error here
	}
	return Error;

	/*auto ret = CM_Disable_DevNode(Info.DevInfoData.DevInst, 0);
	if (ret == CR_SUCCESS)
		return 0;
	else {
		return ret;
	}*/
}
