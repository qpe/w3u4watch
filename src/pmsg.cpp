#include "stdafx.h"

#include <windows.h>

#include <array>
#include <mutex>
#include <regex>
#include <string>

#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>

#include "path.h"

#include "pmsg.h"

#define MAX_TOTAL_BUFFER_SIZE  (1024*32)
#define MAX_BUFFER_SIZE        (2048)

static HWND hWndEdit = NULL;
static std::wstring buff;
std::mutex buff_mutex;  // protects buff

void register_hwnd(HWND hWnd)
{
	hWndEdit = hWnd;
	buff = L"";
}

/* ex: return '2017/03/01 09:00:00' */
static std::wstring get_logtime() {

	/* YYYY/mm/dd HH:MM:SS -> 19byte */
	WCHAR buff[20] = { 0 };
	auto t = time(NULL);
	tm lt;
	auto e = localtime_s(&lt, &t);
	if (e != 0)
		return L"";

	wcsftime(buff, sizeof(buff), L"%Y/%m/%d %H:%M:%S", &lt);

	return buff;
}

static void append_logfile(std::wstring line)
{
	std::wstring data = get_logtime() + L" " + line;

	static std::wstring logname;
	if (logname.empty()) {
		logname = get_exe_dir();
		logname += L"w3u4watch_";
		logname += get_time_prefix();
		logname += L".log";
	}
	HANDLE file = CreateFileW(logname.c_str(), FILE_GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE) {
		auto pos = SetFilePointer(file, 0, NULL, FILE_END);
		DWORD write = data.length() * sizeof(wchar_t);
		DWORD written;
		if (LockFile(file, pos, 0, write, 0)) {
			WriteFile(file, data.data(), write, &written, NULL);
			UnlockFile(file, pos, 0, write, 0);
		}
		CloseHandle(file);
	}
}

void pmsg(const wchar_t * fmt, ...)
{
	if (hWndEdit == NULL) return;

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

	/* replace \n -> \r\n */
	std::wregex re(LR"(([^\r])(\n))");
	append = std::regex_replace(append, re, L"$1\r\n");

	if (rc <= 0) return;
	{
		std::lock_guard<std::mutex> lock(buff_mutex);
		/* write to logfile */
		append_logfile(append);

		/* append to edit box */
		buff.append(append);
		
		size_t s = buff.length();
		if (s > MAX_TOTAL_BUFFER_SIZE) {
			buff.erase(0, s - MAX_TOTAL_BUFFER_SIZE);
			buff.shrink_to_fit();
		}

		SetWindowTextW(hWndEdit, buff.c_str());
	}
	SendMessage(hWndEdit, WM_VSCROLL, SB_BOTTOM, 0);
}