#include "stdafx.h"

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>

#include <winsvc.h>
#include <winioctl.h>
#include <usbioctl.h>

#include "pmsg.h"

#include "../src/usbpcap/usbpcap.h"

#define SNAPSHOT_LEN 262144
#define BUFFER_LEN   (262144*16)

extern std::wstring usbpcap;
extern std::wstring output;
extern int alive;
extern std::mutex alive_mtx;
extern size_t readsize;

/*
* Determines range and index for given address.
*
* Returns TRUE on success (address is within <0; 127>), FALSE otherwise.
*/
static bool USBPcapGetAddressRangeAndIndex(int address, std::uint8_t *range, std::uint8_t *index)
{
	if ((address < 0) || (address > 127))
	{
		pmsg(L"usbpcapmon: Invalid address: %d\n", address);
		return false;
	}

	*range = address / 32;
	*index = address % 32;
	return true;
}

static bool USBPcapIsDeviceFiltered(PUSBPCAP_ADDRESS_FILTER filter, int address)
{
	std::uint8_t range;
	std::uint8_t index;

	if (filter->filterAll == (true ? 1 : 0))
		/* Do not check individual bit if all devices are filtered. */
		return true;

	if (USBPcapGetAddressRangeAndIndex(address, &range, &index) == true)
		/* Assume that invalid addresses are filtered. */
		return true;

	if (filter->addresses[range] & (1 << index))
		return true;

	return false;
}

static bool USBPcapSetDeviceFiltered(PUSBPCAP_ADDRESS_FILTER filter, int address)
{
	std::uint8_t range;
	std::uint8_t index;

	if (USBPcapGetAddressRangeAndIndex(address, &range, &index) == false)
		return false;

	filter->addresses[range] |= (1 << index);
	return true;
}

/*
* Initializes address filter with given NULL-terminated, comma separated list of addresses.
*
* Returns TRUE on success, FALSE otherwise (malformed list or invalid filter pointer).
*/
static bool USBPcapInitAddressFilter(PUSBPCAP_ADDRESS_FILTER filter, const std::wstring list, bool filterAll)
{
	USBPCAP_ADDRESS_FILTER tmp;

	if (filter == NULL)
		return false;

	memset(&tmp, 0, sizeof(USBPCAP_ADDRESS_FILTER));
	tmp.filterAll = filterAll;

	/* TODO: list to device filter */
	/* USBPcapSetDeviceFiltered(&tmp, number) */

	/* Address list was valid. Copy resulting structure. */
	memcpy(filter, &tmp, sizeof(USBPCAP_ADDRESS_FILTER));
	return true;
}

HANDLE create_filter_read_handle()
{
	HANDLE filter_handle = INVALID_HANDLE_VALUE;
	USBPCAP_ADDRESS_FILTER filter;
	std::vector<char> inBuf;
	DWORD inBufSize = 0;
	DWORD bytes_ret;

	if (USBPcapInitAddressFilter(&filter, L"", true) == false)
	{
		pmsg(L"usbpcapmon: USBPcapInitAddressFilter failed!\n");
		goto finish;
	}
	/* new device is not capture. */
	// USBPcapSetDeviceFiltered(&filter, 0);

	filter_handle = CreateFileW(usbpcap.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		0,
		0);

	if (filter_handle == INVALID_HANDLE_VALUE)
	{
		pmsg(L"usbpcapmon: Couldn't open device - %d\n", GetLastError());
		goto finish;
	}

	/* snaplen -> 262144 */
	inBuf.resize(sizeof(USBPCAP_IOCTL_SIZE));
	((PUSBPCAP_IOCTL_SIZE)inBuf.data())->size = SNAPSHOT_LEN;
	inBufSize = sizeof(USBPCAP_IOCTL_SIZE);

	if (!DeviceIoControl(filter_handle,
		IOCTL_USBPCAP_SET_SNAPLEN_SIZE,
		inBuf.data(),
		inBufSize,
		NULL,
		0,
		&bytes_ret,
		0))
	{
		pmsg(L"usbpcapmon: DeviceIoControl failed with %d status (supplimentary code %d)\n",
			GetLastError(),
			bytes_ret);
		goto finish;
	}

	/* buffer size -> 262144 * 16 */
	((PUSBPCAP_IOCTL_SIZE)inBuf.data())->size = BUFFER_LEN;

	if (!DeviceIoControl(filter_handle, 
		IOCTL_USBPCAP_SETUP_BUFFER,
		inBuf.data(),
		inBufSize,
		NULL,
		0,
		&bytes_ret,
		0))
	{
		pmsg(L"usbpcapmon: DeviceIoControl failed with %d status (supplimentary code %d)\n",
			GetLastError(),
			bytes_ret);
		goto finish;
	}

	if (!DeviceIoControl(filter_handle,
		IOCTL_USBPCAP_START_FILTERING,
		(char*)&filter,
		sizeof(filter),
		NULL,
		0,
		&bytes_ret,
		0))
	{
		pmsg(L"usbpcapmon: DeviceIoControl failed with %d status (supplimentary code %d)\n",
			GetLastError(),
			bytes_ret);
		goto finish;
	}
	return filter_handle;

finish:
	if (filter_handle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(filter_handle);
	}

	return INVALID_HANDLE_VALUE;
}

DWORD WINAPI read_thread(LPVOID lParam)
{
	std::vector<std::uint8_t> buffer;
	OVERLAPPED read_overlapped;
	HANDLE rh = *(HANDLE *)lParam;
	HANDLE wh = INVALID_HANDLE_VALUE;
	buffer.resize(BUFFER_LEN);
	
	/* Open Write file. */
	wh = CreateFileW(output.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (rh == INVALID_HANDLE_VALUE)
	{
		pmsg(L"usbpcapmon: CreateFileW(%s) failed.\n", output.c_str());
		goto finish;
	}

	memset(&read_overlapped, 0, sizeof(read_overlapped));
	read_overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

	for (;;)
	{
		DWORD read;
		DWORD written;
		DWORD i;
		BOOL ret;

		ret = ReadFile(rh, (PVOID)buffer.data(), (DWORD)buffer.size(), &read, &read_overlapped);
		if (!ret)
			break;
		i = WaitForSingleObject(read_overlapped.hEvent, INFINITE);

		if (i == WAIT_OBJECT_0) {
			/* Standard read */
			GetOverlappedResult(rh, &read_overlapped, &read, TRUE);
			// ret = GetOverlappedResultEx(rh, &read_overlapped, &read, 1000, FALSE);
			ResetEvent(read_overlapped.hEvent);
			WriteFile(wh, buffer.data(), read, &written, NULL);
			FlushFileBuffers(wh);
		} else {
			read = 0;
		}

		/* check alive */
		{
			std::lock_guard<std::mutex> guard(alive_mtx);
			readsize += read;
			if (alive == 0)
				break;
		}		
	}
	CloseHandle(read_overlapped.hEvent);

finish:
	if(wh != INVALID_HANDLE_VALUE) CloseHandle(wh);

	return 0;
}
