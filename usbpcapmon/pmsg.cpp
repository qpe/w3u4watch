#include "stdafx.h"

#include <windows.h>

#include <array>
#include <string>

#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>

#include "../src/common.h"

#include "pmsg.h"

#define MAX_BUFFER_SIZE        (2048)

extern HWND hWnd;
extern HWND hParentWnd;

void pmsg(const wchar_t * fmt, ...)
{
	if (hWnd == NULL || hParentWnd == NULL)
		return;

	int rc;
	va_list args;
	std::wstring append;
	{
		std::array<wchar_t, MAX_BUFFER_SIZE> temp = { 0 };
		va_start(args, fmt);
		rc = _vsnwprintf_s(temp.data(), temp.size(), temp.size() - 1, fmt, args);
		va_end(args);
		append = temp.data();
	}

	if (rc <= 0) return;
	{
		/* send message */
		if (IsWindow(hParentWnd)) {
			COPYDATASTRUCT data;
			data.dwData = COPYDATA_PMSG;
			data.cbData = (DWORD)((append.size() + 1) * sizeof(wchar_t));
			data.lpData = (PVOID)append.data();
			SendMessageW(hParentWnd, WM_COPYDATA, (WPARAM)hWnd, (LPARAM)&data);
		}
	}
}