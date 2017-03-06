#include "stdafx.h"

#include <shellapi.h>
#include <tlhelp32.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#include <string>
#include <mutex>

#include "pmsg.h"
#include "capthread.h"

#include "../src/common.h"

#include "usbpcapmon.h"

struct FINDAPPWIN {
	DWORD pid;
	HWND hwnd;
};

HINSTANCE inst;
HWND hWnd;
HWND hParentWnd;
int alive = 1;
std::mutex alive_mtx;
std::wstring usbpcap;
std::wstring output;
size_t readsize = 0;
static const WCHAR title[] = L"usbpcapmon";
static const WCHAR window_class[] = L"usbpcapmonMainWindow";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static int GetParentProcessId() {
	int pid = -1;
	int ret = -1;
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe = { 0 };
	pe.dwSize = sizeof(PROCESSENTRY32);

	pid = GetCurrentProcessId();
	
	if (Process32First(h, &pe)) {
		do {
			if (pe.th32ProcessID == pid) {
				ret = pe.th32ParentProcessID;
			}
		} while (Process32Next(h, &pe));
	}

	CloseHandle(h);
	return ret;
}

static BOOL CALLBACK findAppWindow(HWND hwnd, LPARAM lParam)
{
	FINDAPPWIN& faw = *(FINDAPPWIN*)lParam;
	DWORD pid;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid == faw.pid)
	{
		faw.hwnd = hwnd;
		return FALSE;
	}
	return TRUE;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_USBPCAPMON));
	wcex.hCursor = LoadCursorW(hInstance, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = window_class;
	wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_USBPCAPMON));

	return RegisterClassExW(&wcex);
}

bool InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	inst = hInstance;
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	RECT rect = { 0 };
	rect.right = 320;
	rect.bottom = 136;
	AdjustWindowRect(&rect, style, FALSE);

	hWnd = CreateWindowExW(0, window_class, title, style, CW_USEDEFAULT, 0,
		rect.right - rect.left, rect.bottom - rect.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!hWnd) return false;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	SendMessageW(hParentWnd, WM_APP_SEND_HWND, (WPARAM)hWnd, NULL);

	return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static HWND hWndByte;
	switch (message)
	{
	case WM_CREATE:
	{
		/* Progress bar */
		InitCommonControls();
		HWND hWndProgress = CreateWindowExW(0, PROGRESS_CLASS, (LPTSTR)NULL,
			WS_CHILD | WS_VISIBLE | PBS_MARQUEE, 6, 105, 308, 25, hWnd, (HMENU)0, inst, NULL);
		SendMessageW(hWndProgress, PBM_SETMARQUEE, TRUE, 0);

		/* target device */
		CreateWindowExW(0, L"STATIC", usbpcap.c_str(),
			WS_CHILD | WS_VISIBLE, 6, 6, 308, 25, hWnd, (HMENU)0, inst, NULL);
		SendMessageW(hWndProgress, PBM_SETMARQUEE, TRUE, 0);

		/* output file */
		CreateWindowExW(0, L"STATIC", output.c_str(),
			WS_CHILD | WS_VISIBLE, 6, 37, 308, 25, hWnd, (HMENU)0, inst, NULL);
		SendMessageW(hWndProgress, PBM_SETMARQUEE, TRUE, 0);

		/* byte counter */
		hWndByte = CreateWindowExW(0, L"STATIC", L"0 byte",
			WS_CHILD | WS_VISIBLE, 6, 68, 308, 25, hWnd, (HMENU)0, inst, NULL);
		SetTimer(hWnd, 1, 1000, NULL);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_TIMER:
	{
		/* redraw byte count */
		size_t showbyte = 0;
		{
			std::lock_guard<std::mutex> guard(alive_mtx);
			showbyte = readsize;
		}
		WCHAR buff[200];
		swprintf_s(buff, L"%zu byte", showbyte);
		SetWindowTextW(hWndByte, buff);
	}
	break;
	case WM_PARENTNOTIFY:
		switch (LOWORD(wParam)) {
		case WM_CREATE:
			/* set font */
			SendMessage((HWND)lParam, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
			break;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR pCmdLine, int nCmdShow)
{
	int argc;
	LPWSTR *argv;

	argv = CommandLineToArgvW(pCmdLine, &argc);
	if (argc != 2) {
		MessageBoxW(NULL, L"Usage: usbpcapmon.exe <usbpcap target> <output file>", L"Usage", MB_OK);
		return 0;
	}
	usbpcap = argv[0];
	output = argv[1];
	GlobalFree(argv);

	/* get parent window */
	int ppid = GetParentProcessId();
	if (ppid < 0) return 0;

	FINDAPPWIN faw;
	faw.pid = ppid;
	faw.hwnd = NULL;
	EnumWindows(findAppWindow, (LPARAM)&faw);
	if (faw.hwnd == NULL) {
		MessageBoxW(NULL, L"Cannot find parent window handle.", L"Error", MB_OK);
		return 0;
	}
	hParentWnd = faw.hwnd;

	/* Allow WM_CLOSE message */
	ChangeWindowMessageFilter(WM_CLOSE, MSGFLT_ADD);

    MyRegisterClass(hInstance);
    if (!InitInstance (hInstance, nCmdShow))
		return FALSE;

	/* get device handle */
	HANDLE rh = create_filter_read_handle();
	if (rh == INVALID_HANDLE_VALUE)
		return FALSE;

	/* launch thread */
	DWORD thid;
	auto th = CreateThread(NULL, 0, read_thread, (LPVOID)&rh, 0, &thid);

	/* Main Loop */
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
    }
	/* wait for thread */
	{
		std::lock_guard<std::mutex> guard(alive_mtx);
		alive = 0;
		CancelIoEx(rh, NULL);
	}
	DWORD res = STILL_ACTIVE;
	size_t c = 0;
	while (res == STILL_ACTIVE)
	{
		GetExitCodeThread(th, &res);
		Sleep(1);
	}
	CloseHandle(th);
	CloseHandle(rh);

	/* tell to parent window */
	pmsg(L"usbpcapmon: shutdown...\n");
	SendMessageW(hParentWnd, WM_PARENTNOTIFY, (WPARAM)WM_DESTROY, (LPARAM)hWnd);

    return (int) msg.wParam;
}

