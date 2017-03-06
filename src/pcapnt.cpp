#include "stdafx.h"

#include <windows.h>

#include <vector>
#include <string>

#include "pcapnt.h"
#include "pmsg.h"

#define BUFFER_SIZE     0x1000
#define DIRECTORY_QUERY 0x0001
#define NTSTATUS        ULONG

typedef struct _LSA_UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJDIR_INFORMATION
{
	UNICODE_STRING ObjectName;
	UNICODE_STRING ObjectTypeName;
	BYTE Data[1];
} OBJDIR_INFORMATION, *POBJDIR_INFORMATION;

typedef struct _OBJECT_ATTRIBUTES
{
	ULONG Length;
	HANDLE RootDirectory;
	UNICODE_STRING *ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES;

typedef NTSTATUS(WINAPI* NTQUERYDIRECTORYOBJECT)(HANDLE,
	OBJDIR_INFORMATION*,
	DWORD,
	DWORD,
	DWORD,
	DWORD*,
	DWORD*);

typedef NTSTATUS(WINAPI* NTOPENDIRECTORYOBJECT)(HANDLE*,
	DWORD,
	OBJECT_ATTRIBUTES*);

#define InitializeObjectAttributes(p, n, a, r, s) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES ); \
    (p)->RootDirectory = r; \
    (p)->Attributes = a; \
    (p)->ObjectName = n; \
    (p)->SecurityDescriptor = s; \
    (p)->SecurityQualityOfService = NULL; \
}

typedef NTSTATUS(WINAPI* NTCLOSE)(HANDLE);

static NTQUERYDIRECTORYOBJECT NtQueryDirectoryObject;
static NTOPENDIRECTORYOBJECT NtOpenDirectoryObject;
static NTCLOSE NtClose;
static HMODULE ntdll_handle;

const WCHAR *usbpcap_prefix = L"\\\\.\\";

static void __cdecl unload_ntdll(void)
{
	FreeLibrary(ntdll_handle);
}

static bool load_ntdll() {
	ntdll_handle = LoadLibraryW(L"ntdll.dll");
	if (ntdll_handle == NULL)
		return false;
	atexit(unload_ntdll);

	NtQueryDirectoryObject = 
		(NTQUERYDIRECTORYOBJECT)GetProcAddress(ntdll_handle, "NtQueryDirectoryObject");
	if (NtQueryDirectoryObject == NULL)
		return false;

	NtOpenDirectoryObject =
		(NTOPENDIRECTORYOBJECT)GetProcAddress(ntdll_handle, "NtOpenDirectoryObject");
	if (NtOpenDirectoryObject == NULL)
		return false;

	NtClose = (NTCLOSE)GetProcAddress(ntdll_handle, "NtClose");
	if (NtClose == NULL)
		return false;

	return true;
}


std::vector<std::wstring> find_usbpcap_devices()
{
	HANDLE handle;
	UNICODE_STRING str;
	OBJECT_ATTRIBUTES attr;
	DWORD index;
	DWORD written;
	PWSTR path = L"\\Device";
	POBJDIR_INFORMATION info;
	std::vector<std::wstring> list;

	list.clear();
	str.Length = wcslen(path) * 2;
	str.MaximumLength = wcslen(path) * 2 + 2;
	str.Buffer = path;

	if (!load_ntdll()) {
		pmsg(L"usbpcap: Cannot Load ntdll.dll.\n");
		return list;
	}
	
	InitializeObjectAttributes(&attr, &str, 0, NULL, NULL);

	info = (POBJDIR_INFORMATION)HeapAlloc(GetProcessHeap(), 0, BUFFER_SIZE);
	if (info == NULL) {
		pmsg(L"usbpcap: HeapAlloc failed.\n");
		return list;
	}

	auto status = NtOpenDirectoryObject(&handle, DIRECTORY_QUERY, &attr);
	if (status != 0) {
		pmsg(L"usbpcap: NtOpenDirectoryObject failed.\n");
		HeapFree(GetProcessHeap(), 0, info);
		return list;
	}

	status = NtQueryDirectoryObject(handle, info, BUFFER_SIZE, TRUE, TRUE, &index, &written);
	if (status != 0) {
		pmsg(L"NtOpenDirectoryObject() failed\n");
		NtClose(handle);
		HeapFree(GetProcessHeap(), 0, info);
		return list;
	}
	while (NtQueryDirectoryObject(handle, info, BUFFER_SIZE, TRUE, FALSE, &index, &written) == 0)
	{
		const wchar_t *prefix = L"USBPcap";
		size_t prefix_chars = 7;

		if (wcsncmp(prefix, info->ObjectName.Buffer, prefix_chars) == 0)
		{
			std::wstring tmp = usbpcap_prefix;
			tmp += info->ObjectName.Buffer;
			list.push_back(tmp);
		}
	}

	NtClose(handle);
	HeapFree(GetProcessHeap(), 0, info);

	return list;
}
