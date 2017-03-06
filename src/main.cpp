#include "stdafx.h"

#include <objbase.h>
#include <usbiodef.h>
#include <shellapi.h>

#include "common.h"
#include "tuner.h"
#include "path.h"
#include "pmsg.h"
#include "simplecap.h"

#include "main.h"

#define MAX_LOADSTRING 100

#define CMDID_GET_FILTER   100
#define CMDID_USBPCAPMODE  101
#define PARAMID_TUNER_ID   102


HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
bda_tuner *g_tuner = nullptr;
HWND hMonWnd = NULL;

ATOM MyRegisterClass(HINSTANCE hInstance);
bool InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr)) {

		LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
		LoadStringW(hInstance, IDC_W3U4WATCH, szWindowClass, MAX_LOADSTRING);


		MyRegisterClass(hInstance);
		if (InitInstance(hInstance, nCmdShow)) {
			bda_tuner tuner;
			g_tuner = &tuner;
			MSG msg;
			while (GetMessageW(&msg, nullptr, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			g_tuner = nullptr;
		}
		CoUninitialize();
	}
	return 0;
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
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_W3U4WATCH));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_W3U4WATCH));

	return RegisterClassExW(&wcex);
}

bool InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;

	DWORD dwStyle = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME;
	RECT rect;
	rect.top = 0;
	rect.left = 0;
	rect.right = 640;
	rect.bottom = 480;
	AdjustWindowRect(&rect, dwStyle, FALSE);
	HWND hWnd = CreateWindowW(szWindowClass, szTitle, dwStyle,
		CW_USEDEFAULT, 0, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return false;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return true;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool launched = false;
	static HWND hWndMonButton = NULL;

	switch (message)
	{
	case WM_CREATE:
	{
		/* allow WM_COPYDATA */
		ChangeWindowMessageFilterEx(hWnd, WM_COPYDATA, MSGFLT_ADD, NULL);

		/* output edit control */
		HWND hWndEdit = CreateWindowExW(WS_EX_STATICEDGE, WC_EDITW, L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_LEFT, 6, 360, 628,
			114, hWnd, (HMENU)1, hInst, NULL);
		if (!hWndEdit)
		{
			return false;
		}
		register_hwnd(hWndEdit);

		/* get filter button */
		CreateWindowExW(WS_EX_STATICEDGE, WC_BUTTONW, L"get_filter",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER, 6, 6, 75, 23,
			hWnd, (HMENU)CMDID_GET_FILTER, hInst, NULL);

		/* USB pcap mode button */
		if (is_usbpcap_upper_filter_installed()) {
			hWndMonButton = CreateWindowExW(WS_EX_STATICEDGE, WC_BUTTONW, L"usbpcap",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_CENTER, 559, 6, 75, 23,
				hWnd, (HMENU)CMDID_USBPCAPMODE, hInst, NULL);
		}

		/* TODO: remove this test code... */
		pmsg(L"exe dir: %s\n", get_exe_dir().c_str());
		pmsg(L"time prefix: %s\n", get_time_prefix().c_str());
	}
	break;
	case WM_RBUTTONDOWN:
		/* TODO: create popup menu */
		break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId)
		{
			/* TODO: create popup menu 
			 * example:
			 *   case IDM_EXIT: */
		case CMDID_GET_FILTER:
			if (g_tuner != nullptr) {
				if (g_tuner->get_filter()) {
					EnableWindow(HWND(lParam), FALSE);
					/* target tuner button */
					HWND hWndCombo = CreateWindowExW(WS_EX_STATICEDGE, WC_COMBOBOXW, L"",
						WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 6, 35, 150, 23,
						hWnd, (HMENU)PARAMID_TUNER_ID, hInst, NULL);
					if (!hWndCombo)
						break;
					for (auto i = 0; !g_tuner->short_name(i).empty(); i++) {
						SendMessage(hWndCombo, (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)g_tuner->short_name(i).c_str());
					}
					SendMessage(hWndCombo, CB_SETCURSEL, 0, 0);
				}
			}
			break;
		case CMDID_USBPCAPMODE:
			{

				EnableWindow(HWND(lParam), FALSE);

				std::wstring exepath;
				std::wstring usbpcapdev;
				std::wstring filepath;

				if (launched) break;
				exepath = get_usbpcapmon();
				usbpcapdev = find_usbpcap();
				filepath = get_pcap_filename();
				if (exepath.empty() || usbpcapdev.empty()) break;

				std::wstring param = usbpcapdev;
				param += L" \"";
				param += filepath;
				param += L"\"";

				SHELLEXECUTEINFOW sei;
				ZeroMemory(&sei, sizeof(sei));
				sei.hwnd = hWnd;
				sei.cbSize = sizeof(sei);
				sei.lpVerb = L"open";
				sei.lpFile = exepath.c_str();
				sei.lpParameters = param.c_str();
				sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_WAITFORINPUTIDLE;
				sei.nShow = SW_SHOWDEFAULT;
				ShellExecuteEx(&sei);
				pmsg(L"exec: %s %s\n", exepath.c_str(), param.c_str());
			}
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_APP_SEND_HWND:
		if ((HWND)(wParam)) {
			hMonWnd = (HWND)(wParam);
			pmsg(L"usbcapmon: started.\n");
		}
		break;
	case WM_COPYDATA:
		if (wParam != NULL && (HWND)wParam == hMonWnd) {
			PCOPYDATASTRUCT data = (PCOPYDATASTRUCT)lParam;
			switch (data->dwData) {
			case COPYDATA_PMSG:
				if(data->cbData)
					pmsg(L"%s", (wchar_t *)data->lpData);
				break;
			}
		}
		break;
	case WM_PARENTNOTIFY:
		switch (LOWORD(wParam)) {
		case WM_CREATE:
			/* set font */
			SendMessage((HWND)lParam, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
			break;
		case WM_DESTROY:
			if ((HWND)lParam == hMonWnd) {
				pmsg(L"usbcapmon Window closed.\n");
				launched = false;
				EnableWindow(hWndMonButton, TRUE);
			}
			break;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	case WM_CLOSE:
		if (hMonWnd != NULL && IsWindow(hMonWnd))
			SendMessage(hMonWnd, WM_CLOSE, 0, 0);
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
