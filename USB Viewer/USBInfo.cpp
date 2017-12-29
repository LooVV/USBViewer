#include "stdafx.h"
#include "USBInfo.h"
#include <algorithm>

#include <windows.h>
#include <devguid.h>    // for GUID_DEVCLASS_CDROM etc
#include <setupapi.h>
#include <cfgmgr32.h>   // for MAX_DEVICE_ID_LEN, CM_Get_Parent and CM_Get_Device_ID
#include <clocale>
#define INITGUID
#include <tchar.h>
#include <stdio.h>

#pragma comment(lib, "SetupAPI")


// include DEVPKEY_Device_BusReportedDeviceDesc from WinDDK\7600.16385.1\inc\api\devpropdef.h
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


static bool UpdateInfo(std::vector<USBDeviceInfo>& Devices,
	std::vector<USBDeviceInfo>& NewDevices);

static std::vector<USBDeviceInfo> EraseRetired(std::vector<USBDeviceInfo>& USBDevices);
					

#define CLSID_STR_WEIUSB (L"{A5DCBF10-6530-11D2-901F-00C04FB951ED}")

 // List all USB devices with some additional information
bool UpdateInfo(std::vector<USBDeviceInfo>& USBDevices, std::vector<USBDeviceInfo>& NewDevices )
{
	GUID     DevClass;
	CLSIDFromString(CLSID_STR_WEIUSB, &DevClass);

	LPCTSTR pszEnumerator = L"USB";
	unsigned i, j;
	DWORD dwSize, dwPropertyRegDataType;
	DEVPROPTYPE ulPropertyType;
	CONFIGRET status;
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	const static LPCTSTR arPrefix[3] = { TEXT("VID_"), TEXT("PID_"), TEXT("MI_") };
	TCHAR szDeviceInstanceID[MAX_DEVICE_ID_LEN];
	TCHAR szDesc[1024], szHardwareIDs[4096];
	WCHAR szBuffer[4096];
	LPTSTR pszToken, pszNextToken;
	TCHAR szVid[MAX_DEVICE_ID_LEN], szPid[MAX_DEVICE_ID_LEN], szMi[MAX_DEVICE_ID_LEN];


	// List all connected USB devices
	hDevInfo = SetupDiGetClassDevs(&DevClass, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE); //DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return false;
	
	
	// Find the ones that are driverless
	for (i = 0; ; i++) 
	{		
		DeviceInfoData.cbSize = sizeof(DeviceInfoData);
		if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData))
			break;

		status = CM_Get_Device_ID(DeviceInfoData.DevInst, szDeviceInstanceID, MAX_PATH, 0);
		if (status != CR_SUCCESS)
			continue;

		// Display device instance ID
		CString ID { szDeviceInstanceID };


		auto Device = std::find_if(USBDevices.begin(), USBDevices.end(),
						[&](const USBDeviceInfo& Info) {
							return Info.InstanceID == ID;
						});
		//устройство осталось в списке
		if (Device != USBDevices.end())
		{
			Device->Checked = true;
			continue;
		}
		
		//новое устройство, нужно заполнить информацией
		USBDeviceInfo Info;
		//TODO: не уверен, что у CString есть move семантика
		Info.InstanceID = std::move(ID);


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

			//_tprintf(TEXT("    Hardware IDs:\n"));
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
			for (j = 0; j < 3; j++) {
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


		//записываем в основной список
		NewDevices.push_back(std::move(Info));
	}

	return true;
}

bool UpdateList(std::vector<USBDeviceInfo>& USBDevices,
				std::vector<USBDeviceInfo>& RetiredDevices,
				std::vector<USBDeviceInfo*>& NewDevicesRef)
{
	std::vector<USBDeviceInfo> NewDevices;
	if (!UpdateInfo(USBDevices, NewDevices))
		return false;

	RetiredDevices = EraseRetired(USBDevices);
	//на этом этапе старые приборы в RetiredDevices

	for (auto It = NewDevices.rbegin(); It != NewDevices.rend(); ++It)
	{
		USBDevices.push_back(std::move(*It));
		NewDevicesRef.push_back(&(USBDevices.back()));
	}
	//новые устройства перенесены в USBDevices, а в NewDevicesRef ссылки на них
	//обновляем метки
	for (auto& Item : USBDevices) {
		Item.Checked = false;
	}

	return true;
}

std::vector<USBDeviceInfo> EraseRetired(std::vector<USBDeviceInfo>& USBDevices)
{
	std::vector<USBDeviceInfo> RetiredDevices;
	//0 всего = 0 старых
	if (USBDevices.size() == 0)
		return RetiredDevices;

	//в процессе обхода нужно удалять элементы, поэтому обратный обход
	for (size_t i = USBDevices.size() - 1; i >= 0 && i < USBDevices.size(); --i)
	{

		if (! USBDevices[i].Checked)
		{
			RetiredDevices.push_back(std::move(USBDevices[i]));

			USBDevices.erase(USBDevices.begin() + i);
		}
	}

	return RetiredDevices;
}