/*
 * Copyright (c) 2013 Tomasz Moń <desowin@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "stdafx.h"

#include <windows.h>
#include <cfgmgr32.h>
#include <winsvc.h>
#include <winioctl.h>
#include <usbioctl.h>
#include <shlwapi.h>
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <array>
#include <string>
#include <vector>

#include "path.h"
#include "pmsg.h"
#include "pcapnt.h"

#include "usbpcap/usbpcap.h"

#define IOCTL_OUTPUT_BUFFER_SIZE 1024

const wchar_t usbfriendlyname[] = L"PXW3U4 BDA Device(2Tx2S)";

/* Prototype for recursive... */
static bool search_hub(const std::wstring &hub,
	PUSB_NODE_CONNECTION_INFORMATION connection_info,
	ULONG level);

bool is_usbpcap_upper_filter_installed()
{
	LONG reg_val;
	HKEY hkey;
	DWORD length;
	DWORD type;

	std::wstring lookup = L"USBPcap";

	reg_val = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		L"System\\CurrentControlSet\\Control\\Class\\{36FC9E60-C465-11CF-8056-444553540000}",
		0, KEY_QUERY_VALUE, &hkey);

	if (reg_val != ERROR_SUCCESS)
	{
		pmsg(L"usbpcap: Failed to open USB Class registry key.(%d)\n", reg_val);
		return FALSE;
	}

	reg_val = RegQueryValueExW(hkey, L"UpperFilters", NULL, &type, NULL, &length);

	if (reg_val != ERROR_SUCCESS)
	{
		pmsg(L"usbpcap: Failed to query UpperFilters value size.(%d)\n", reg_val);
		RegCloseKey(hkey);
		return FALSE;
	}

	if (type != REG_MULTI_SZ)
	{
		pmsg(L"usbpcap: Invalid UpperFilters type.(%d)\n", type);
		RegCloseKey(hkey);
		return FALSE;
	}

	if (length <= 0)
	{
		RegCloseKey(hkey);
		return FALSE;
	}

	std::vector<WCHAR> multisz;
	multisz.resize(length, 0);
	reg_val = RegQueryValueExW(hkey, L"UpperFilters", NULL, NULL, (LPBYTE)multisz.data(), &length);

	if (reg_val != ERROR_SUCCESS)
	{
		pmsg(L"Failed to read UpperFilters value! Code %d\n", reg_val);
		RegCloseKey(hkey);
		return FALSE;
	}

	RegCloseKey(hkey);

	for (size_t i = 0; i < length - lookup.length(); i++)
	{
		int r = memcmp(&multisz.data()[i], lookup.data(), lookup.length() * sizeof(wchar_t));
		if (r == 0)
			return TRUE;
	}
	return FALSE;
}

static std::wstring GetDriverKeyName(HANDLE Hub, ULONG ConnectionIndex)
{
	BOOL                                success;
	ULONG                               nBytes;
	USB_NODE_CONNECTION_DRIVERKEY_NAME  driverKeyName;
	PUSB_NODE_CONNECTION_DRIVERKEY_NAME driverKeyNameW;
	std::wstring retval;
	driverKeyNameW = NULL;

	// Get the length of the name of the driver key of the device attached to
	// the specified port.
	driverKeyName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		&driverKeyName,
		sizeof(driverKeyName),
		&driverKeyName,
		sizeof(driverKeyName),
		&nBytes,
		NULL);

	if (!success)
	{
		pmsg(L"usbpcap: DeviceIoControl failed.\n");
		goto GetDriverKeyNameError;
	}

	// Allocate space to hold the driver key name
	nBytes = driverKeyName.ActualLength;

	if (nBytes <= sizeof(driverKeyName))
	{
		pmsg(L"usbpcap: actual size is too short.\n");
		goto GetDriverKeyNameError;
	}

	driverKeyNameW = (PUSB_NODE_CONNECTION_DRIVERKEY_NAME)GlobalAlloc(GPTR, nBytes);

	if (driverKeyNameW == NULL)
	{
		pmsg(L"usbpcap: GlobalAlloc failed.\n");
		goto GetDriverKeyNameError;
	}

	// Get the name of the driver key of the device attached to
	// the specified port.
	driverKeyNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_DRIVERKEY_NAME,
		driverKeyNameW,
		nBytes,
		driverKeyNameW,
		nBytes,
		&nBytes,
		NULL);

	if (!success)
	{
		pmsg(L"usbpcap: DeviceIoControl failed.\n");
		goto GetDriverKeyNameError;
	}
	retval = driverKeyNameW->DriverKeyName;
	GlobalFree(driverKeyNameW);

	return retval;

GetDriverKeyNameError:
	// There was an error, free anything that was allocated
	if (driverKeyNameW != NULL)
	{
		GlobalFree(driverKeyNameW);
		driverKeyNameW = NULL;
	}

	return L"";
}

static std::wstring GetExternalHubName(HANDLE Hub, ULONG ConnectionIndex)
{
	BOOL                        success;
	ULONG                       nBytes;
	USB_NODE_CONNECTION_NAME	extHubName;
	PUSB_NODE_CONNECTION_NAME   extHubNameW;
	std::wstring retval;

	extHubNameW = NULL;

	// Get the length of the name of the external hub attached to the
	// specified port.
	extHubName.ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		&extHubName,
		sizeof(extHubName),
		&extHubName,
		sizeof(extHubName),
		&nBytes,
		NULL);

	if (!success)
	{
		pmsg(L"usbpcap: DeviceIoControl failed.\n");
		goto GetExternalHubNameError;
	}

	// Allocate space to hold the external hub name
	nBytes = extHubName.ActualLength;

	if (nBytes <= sizeof(extHubName))
	{
		pmsg(L"usbpcap: actual size is too short.\n");
		goto GetExternalHubNameError;
	}

	extHubNameW = (PUSB_NODE_CONNECTION_NAME)GlobalAlloc(GPTR, nBytes);

	if (extHubNameW == NULL)
	{
		pmsg(L"usbpcap: GlobalAlloc failed.\n");
		goto GetExternalHubNameError;
	}

	// Get the name of the external hub attached to the specified port
	extHubNameW->ConnectionIndex = ConnectionIndex;

	success = DeviceIoControl(Hub,
		IOCTL_USB_GET_NODE_CONNECTION_NAME,
		extHubNameW,
		nBytes,
		extHubNameW,
		nBytes,
		&nBytes,
		NULL);

	if (!success)
	{
		pmsg(L"usbpcap: DeviceIoControl failed.\n");
		goto GetExternalHubNameError;
	}

	retval = extHubNameW->NodeName;
	GlobalFree(extHubNameW);
	return retval;

GetExternalHubNameError:
	// There was an error, free anything that was allocated
	if (extHubNameW != NULL)
	{
		GlobalFree(extHubNameW);
		extHubNameW = NULL;
	}
	return L"";
}

static bool search_device_desc(const std::wstring &DriverName, ULONG Index,
	ULONG Level, USHORT deviceAddress, USHORT parentAddress)
{
	DEVINST    devInst;
	DEVINST    devInstNext;
	CONFIGRET  cr;
	ULONG      walkDone = 0;
	ULONG      len;
	std::array<WCHAR, MAX_DEVICE_ID_LEN> buf;

	// Get Root DevNode
	cr = CM_Locate_DevNode(&devInst, NULL, 0);

	if (cr != CR_SUCCESS)
	{
		return false;
	}

	// Do a depth first search for the DevNode with a matching
	// DriverName value
	while (!walkDone)
	{
		// Get the DriverName value
		len = sizeof(buf) / sizeof(buf[0]);
		cr = CM_Get_DevNode_Registry_Property(devInst,
			CM_DRP_DRIVER,
			NULL,
			buf.data(),
			&len,
			0);

		// If the DriverName value matches, return the DeviceDescription
		if (cr == CR_SUCCESS && _wcsicmp(DriverName.c_str(), buf.data()) == 0)
		{
			len = static_cast<ULONG>(buf.size());

			cr = CM_Get_DevNode_Registry_PropertyW(devInst,
				CM_DRP_DEVICEDESC,
				NULL,
				buf.data(),
				&len,
				0);


			if (cr == CR_SUCCESS)
			{
				if (wcsncmp(buf.data(), usbfriendlyname, buf.size()) == 0)
					return true;
			}

			// Nothing left to do
			return false;
		}

		// This DevNode didn't match, go down a level to the first child.
		cr = CM_Get_Child(&devInstNext, devInst, 0);

		if (cr == CR_SUCCESS)
		{
			devInst = devInstNext;
			continue;
		}

		// Can't go down any further, go across to the next sibling.  If
		// there are no more siblings, go back up until there is a sibling.
		// If we can't go up any further, we're back at the root and we're
		// done.
		for (;;)
		{
			cr = CM_Get_Sibling(&devInstNext, devInst, 0);

			if (cr == CR_SUCCESS)
			{
				devInst = devInstNext;
				break;
			}

			cr = CM_Get_Parent(&devInstNext, devInst, 0);

			if (cr == CR_SUCCESS)
			{
				devInst = devInstNext;
			}
			else
			{
				walkDone = 1;
				break;
			}
		}
	}
	return false;
}

static bool search_hub_ports(HANDLE hHubDevice, ULONG NumPorts, ULONG level, USHORT hubAddress)
{
	ULONG index;
	BOOL success;
	std::wstring driverKeyName;
	bool matched = false;

	// Loop over all ports of the hub.
	//
	// Port indices are 1 based, not 0 based.
	for (index = 1; index <= NumPorts; index++)
	{
		ULONG nBytes;
		USB_NODE_CONNECTION_INFORMATION connectionInfo;

		connectionInfo.ConnectionIndex = index;

		success = DeviceIoControl(hHubDevice,
			IOCTL_USB_GET_NODE_CONNECTION_INFORMATION,
			&connectionInfo,
			sizeof(USB_NODE_CONNECTION_INFORMATION),
			&connectionInfo,
			sizeof(USB_NODE_CONNECTION_INFORMATION),
			&nBytes,
			NULL);

		if (!success)
		{
			pmsg(L"usbpcap: DeviceIoControl(Port=%u) failed. skip ports.\n", index);
			continue;
		}

		// If there is a device connected, get the Device Description
		if (connectionInfo.ConnectionStatus != NoDeviceConnected)
		{
			driverKeyName = GetDriverKeyName(hHubDevice, index);

			if (!driverKeyName.empty())
			{
				matched = search_device_desc(driverKeyName, index, level,
					connectionInfo.DeviceAddress,
					hubAddress);
				if (matched) return true;
			}

			// If the device connected to the port is an external hub, get the
			// name of the external hub and recursively enumerate it.
			if (connectionInfo.DeviceIsHub)
			{
				auto extHubName = GetExternalHubName(hHubDevice, index);

				if (!extHubName.empty())
				{
					matched = search_hub(extHubName, &connectionInfo, level + 1);
					if (matched) return true;
				}
			}
		}
	}
	return matched;
}

static bool search_hub(const std::wstring &hub,
	PUSB_NODE_CONNECTION_INFORMATION connection_info,
	ULONG level)
{
	PUSB_NODE_INFORMATION   hubInfo;
	HANDLE                  hHubDevice;
	PTSTR                   deviceName;
	size_t                  deviceNameSize;
	BOOL                    success;
	ULONG                   nBytes;
	bool                    matched = false;
	std::wstring            buff;

	// Initialize locals to not allocated state so the error cleanup routine
	// only tries to cleanup things that were successfully allocated.
	hubInfo = NULL;
	hHubDevice = INVALID_HANDLE_VALUE;

	// Allocate some space for a USB_NODE_INFORMATION structure for this Hub
	hubInfo = (PUSB_NODE_INFORMATION)GlobalAlloc(GPTR, sizeof(USB_NODE_INFORMATION));

	if (hubInfo == NULL)
	{
		pmsg(L"usbpcap: GlobalAlloc(hubInfo) failed.\n");
		goto EnumerateHubError;
	}

	if (hub.find(L"\\\?\?\\") == 0)
	{
		/* Replace the \??\ with \\.\ */
		buff = L"\\\\.\\";
		buff += hub.substr(4);
	}
	else if (hub[0] == L'\\')
	{
		buff = hub;
	}
	else
	{
		buff = L"\\\\.\\";
		buff += hub;
	}

	// Allocate a temp buffer for the full hub device name.
	deviceNameSize = buff.length() + 1;
	deviceName = (LPWSTR)GlobalAlloc(GPTR, deviceNameSize * sizeof(WCHAR));
	if (deviceName == NULL)
	{
		pmsg(L"usbpcap: GlobalAlloc(deviceName) failed.\n");
		goto EnumerateHubError;
	}
	wcscpy_s(deviceName, deviceNameSize, buff.data());

	// Try to hub the open device
	hHubDevice = CreateFileW(deviceName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	GlobalFree(deviceName);

	if (hHubDevice == INVALID_HANDLE_VALUE)
	{
		pmsg(L"usbpcap: CreateFile(%s) failed.\n", hub.c_str());
		goto EnumerateHubError;
	}

	// Now query USBHUB for the USB_NODE_INFORMATION structure for this hub.
	// This will tell us the number of downstream ports to enumerate, among
	// other things.
	success = DeviceIoControl(hHubDevice,
		IOCTL_USB_GET_NODE_INFORMATION,
		hubInfo,
		sizeof(USB_NODE_INFORMATION),
		hubInfo,
		sizeof(USB_NODE_INFORMATION),
		&nBytes,
		NULL);

	if (!success)
	{
		pmsg(L"usbpcap: DeviceIoControl(%s) failed.\n", hub.c_str());
		goto EnumerateHubError;
	}

	// Now recursively enumrate the ports of this hub.
	matched = search_hub_ports(hHubDevice,
		hubInfo->u.HubInformation.HubDescriptor.bNumberOfPorts,
		level,
		(connection_info == NULL) ? 0 : connection_info->DeviceAddress);

EnumerateHubError:

	// Clean up any stuff that got allocated
	if (hHubDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hHubDevice);
		hHubDevice = INVALID_HANDLE_VALUE;
	}

	if (hubInfo)
	{
		GlobalFree(hubInfo);
	}

	return matched;
}

static bool search_usb_devices(const std::wstring &filter)
{
	HANDLE filter_handle;
	std::array <WCHAR, IOCTL_OUTPUT_BUFFER_SIZE> out_buf;
	DWORD  bytes_ret;
	bool matched = false;

	filter_handle = CreateFileW(filter.c_str(), 0, 0, 0, OPEN_EXISTING, 0, 0);

	if (filter_handle == INVALID_HANDLE_VALUE)
	{
		pmsg(L"usbpcap: couldn't open device - %d\n", GetLastError());
		return matched;
	}

	if (DeviceIoControl(filter_handle, IOCTL_USBPCAP_GET_HUB_SYMLINK, NULL, 0, out_buf.data(), static_cast<DWORD>(out_buf.size()), &bytes_ret, 0))
	{
		if (bytes_ret > 0)
		{
			matched = search_hub(out_buf.data(), NULL, 2);
		}
	}
	CloseHandle(filter_handle);
	return matched;
}


std::wstring find_usbpcap() {

	auto pcap_devs = find_usbpcap_devices();

	if (pcap_devs.empty())
		return L"";

	for (auto dev : pcap_devs) {
		if (search_usb_devices(dev)) {
			pmsg(L"usbpcap: target usbpcap device found. Device: %s\n", dev.c_str());
			return dev;
		}
	}

	return L"";
}

std::wstring get_usbpcapmon() {
	std::wstring path = get_exe_dir();
	path += L"usbpcapmon.exe";
	if (!PathFileExistsW(path.c_str())) {
		pmsg(L"usbpcapmon.exe not found.\n");
		pmsg(L"Please put exe file in %s.\n", get_exe_dir().c_str());
		return L"";
	}
	
	return path;
}

std::wstring get_pcap_filename() {
	std::wstring path = get_exe_dir();
	path += L"usb_";
	path += get_time_prefix();
	path += L".pcap";
	return path;
}