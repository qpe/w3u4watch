#include "stdafx.h"

#include <windows.h>
#include <time.h>

#include "path.h"

/* ex: return 'C\:Users\user\Desktop\' */
std::wstring get_exe_dir() {
	WCHAR full_buff[_MAX_PATH];
	WCHAR drive_buff[_MAX_DRIVE];
	WCHAR dir_buff[_MAX_DIR];
	WCHAR buff[_MAX_PATH];

	// get executable path
	GetModuleFileNameW(NULL, full_buff, _MAX_PATH);
	// split path
	_wsplitpath_s(full_buff, drive_buff, _MAX_DRIVE, dir_buff, _MAX_DIR, NULL, 0, NULL, 0);
	// concat
	swprintf_s(buff, _MAX_PATH, L"%s%s", drive_buff, dir_buff);

	return buff;
}

/* ex: return '20170301_090000' */
std::wstring get_time_prefix() {

	/* YYYYmmdd_HHMMSS -> 15byte */
	WCHAR buff[16] = {0};
	auto t = time(NULL);
	tm lt;
	auto e = localtime_s(&lt, &t);
	if (e != 0)
		return L"";

	wcsftime(buff, sizeof(buff), L"%Y%m%d_%H%M%S", &lt);
	
	return buff;
}