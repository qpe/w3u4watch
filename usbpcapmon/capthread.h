#pragma once

#include <windows.h>

HANDLE create_filter_read_handle();
DWORD WINAPI read_thread(LPVOID lParam);
