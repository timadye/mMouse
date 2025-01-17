// mMouse v0.4a - fix middle mouse button for ASUS laptop that runs windows 10
//
// Version history:-
//
// ceezblog.info - 29/2/2016
//
// different approach with back / next function, just kill TAB of combo ALT-TAB-LEFT/RIGHT
//
// Cyrillev :
// Fix disable "Backward / Forward" and "Middle mouse fix"
// Open File Explorer With 3 fingers up gesture
// Middle click now with 2 fingers Tap and Right with 3 fingers Tap
// Fix : while (passNextClick || k>1) { Sleep(3); k--; } ==> while (passNextClick && k>1) { Sleep(3); k--; }
// Fix : while (passNextKey || k>1) {Sleep(3); k--;}  ==> while (passNextKey && k>1) {Sleep(3); k--;} 
// Add : SendMouseClick() function, and change function name : sendKey() ==> SendKey()
//
// 01/09/2017
// Fix : send Middle and Right Click
// MSDN : https://msdn.microsoft.com/en-us/library/windows/desktop/ms644986(v=vs.85).aspx
// The hook procedure should process a message in less time than the data entry specified in the LowLevelHooksTimeout value in the following registry key:
// HKEY_CURRENT_USER\Control Panel\Desktop
// The value is in milliseconds. If the hook procedure times out, the system passes the message to the next hook.  <---
// However, on Windows 7 and later, the hook is silently removed without being called.
// There is no way for the application to know whether the hook is removed.
// ==> Use thread to send Middle and Right Click so that the function is faster than LowLevelHooksTimeout
//     (and change LowLevelHooksTimeout in Windows 10 has no effect ???)
// ==> Send Open Windows Explorer by Thread
//
// Test : OK with SmartGesture_Win10_64_VER409 http://dlcdnet.asus.com/pub/ASUS/nb/Apps_for_Win10/SmartGesture/SmartGesture_Win10_64_VER409.zip?_ga=2.172942123.962806994.1504290823-185335011.1500703387
//      : NOK with 4.0.17 (not ok with : Backward / Forward (3 fingers swipe left / right) beacause Smart Gesture Send other key)
//
// 08/09/2017
// Use Thread to send Backward / Forward (3 fingers swipe left / right)
// Fix : Send RButtonDown in some case
// Fix : SendKey() function
// Disable OutputDebugStringA() function
// Test : OK with SmartGesture 4.0.9 and 4.0.17
// SmartGesture_Win10_64_VER409 http://dlcdnet.asus.com/pub/ASUS/nb/Apps_for_Win10/SmartGesture/SmartGesture_Win10_64_VER409.zip?_ga=2.172942123.962806994.1504290823-185335011.1500703387
//
// Tim Adye 28/03/2021 v0.4a
// update version numbers and tidy up file naming.
// Compiled with VS2019 for x64.
// Restore ceezblog's Middle click with 3 fingers Tap and Right with 2 fingers Tap.
// This is controlled by the program options below (switch 0<->1 for Cyrillev's settings).


#include <windows.h>
#include <tchar.h>
#include "resource.h"

//Program options
#define TWOMOUSE_MIDDLE 0
#define THREEMOUSE_RIGHT 0
#define THREEMOUSE_MIDDLE 1

//Macro for String
#define copyString(a,b)	swprintf((a),sizeof(a)/sizeof(*a),L"%s",(b))

#define TRAY_ICON_ID			5001
#define SWM_TRAYMSG				WM_APP		//	the message ID sent to our window
#define SM_THREEMOUSE_SWIPE_UP	WM_APP + 6	//	3 fingers swipe up
#define SM_DESTROY				WM_APP + 5	//	hide the window
#define SM_THREEMOUSE_TAP		WM_APP + 4	//	3 fingers tap
#define SM_THREEMOUSE_SWIPE		WM_APP + 3	//	3 fingers swipe
#define SM_ABOUTAPP				WM_APP + 2
#define SM_SEPARATOR			WM_APP + 1	//	SEPARATOR
#define VK_S					83

#define SM_CLOSE				5002
#define DELAY_TIMER_ID			2100
#define DELAY_TIMER_VALUE		30	// wait 30ms for detect software keypress or human keypress.

#define OutputDebugStringA  //Disable OutputDebugStringA function



//Structure used by WH_KEYBOARD_LL
typedef struct KBHOOKSTRUCT {
	DWORD   dwKeyCode;  //they usually use vkCode, for virtual key code
	DWORD   dwScanCode;
	DWORD   dwFlags;
	DWORD   dwTime;
	ULONG_PTR dwExtraInfo;
} KBHOOKSTRUCT;

enum cKeyEvent { KEY_UP, KEY_DOWN };

// Global variables
static TCHAR szWindowClass[] = _T("win32app");
static TCHAR szTitle[] = _T("About mMouse v0.4a");
static TCHAR szTip[] = _T("mMouse - Middle Mouse for windows 10.\r\n\r\n This program provides Middle Mouse function\r\n which ASUS Smart Gesture fails.");

HINSTANCE hInst;
NOTIFYICONDATA	niData;	// Storing notify icon data
HHOOK kbhHook;
HWND hHiddenDialog;
HHOOK mousehHook;

static BOOL ThreeFingerTap = TRUE;
static BOOL ThreeFingerSwipe = TRUE;
static BOOL ThreeFingerSwipeUp = TRUE;
static BOOL SwipeReady = FALSE;
static BOOL SwipeLock = FALSE;

static BOOL LWinDown = FALSE;
static BOOL kill_LWin = FALSE;
static BOOL LAltDown = FALSE;
static BOOL kill_LAlt = FALSE;
static BOOL kill_Tab = FALSE;
static BOOL kill_Leftkey = FALSE;
static BOOL kill_RightKey = FALSE;
static BOOL Kill_SKey = FALSE;
static BOOL passNextKey = FALSE;
static BOOL passNextClick = FALSE;

static BOOL RButtonDown = FALSE;
static BOOL OpenFileExplorer = FALSE;

static BOOL timerOn = FALSE;
static INT	keyCounter = 0;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
__declspec(dllexport) LRESULT CALLBACK KBHookProc (int, WPARAM, LPARAM);
__declspec(dllexport) LRESULT CALLBACK MouseHookProc(int, WPARAM, LPARAM);


//////////// FUNCTIONS //////////////

// Pop up menu
void ShowContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	if(hMenu)
	{
		InsertMenu(hMenu, -1, MF_BYPOSITION , SM_ABOUTAPP, L"About mMouse 0.4a...");
		InsertMenu(hMenu, -1, MF_SEPARATOR, WM_APP+3, NULL);
		InsertMenu(hMenu, -1, (ThreeFingerTap)?MF_CHECKED:MF_UNCHECKED , SM_THREEMOUSE_TAP,
#if TWOMOUSE_MIDDLE
                   L"Middle mouse fix (2 fingers Tap)");
#else
                   L"Middle mouse fix (3 fingers Tap)");
#endif
		InsertMenu(hMenu, -1, (ThreeFingerSwipe)?MF_CHECKED:MF_UNCHECKED , SM_THREEMOUSE_SWIPE, L"Backward / Forward (3 fingers swipe left / right)");
		InsertMenu(hMenu, -1, (ThreeFingerSwipeUp)?MF_CHECKED : MF_UNCHECKED, SM_THREEMOUSE_SWIPE_UP, L"Open File Explorer (3 fingers swipe up)");

		InsertMenu(hMenu, -1, MF_SEPARATOR, SM_SEPARATOR, NULL);
		InsertMenu(hMenu, -1, MF_BYPOSITION, SM_DESTROY, L"Quit");

		SetForegroundWindow(hWnd);
		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL );
		DestroyMenu(hMenu);
	}
}

void SetTimeOut()
{
	SetTimer(hHiddenDialog, DELAY_TIMER_ID, DELAY_TIMER_VALUE, NULL);
	timerOn = TRUE;

	if (LAltDown)
	{
		OutputDebugStringA(LPCSTR("SetTimeOut() : LAltDown == true \n"));
	}
	
	if (LWinDown)
	{
		OutputDebugStringA(LPCSTR("SetTimeOut() : LWinDown == true \n"));
	}
	if (RButtonDown)
	{
		OutputDebugStringA(LPCSTR("SetTimeOut() : RButtonDown == true \n"));
	}   
}

void SetTimeOut(int milisec)
{
	SetTimer(hHiddenDialog, DELAY_TIMER_ID, milisec, NULL);
	timerOn = TRUE;
}

void StopTimeOut()
{
	KillTimer(hHiddenDialog, DELAY_TIMER_ID);
	timerOn = FALSE;
}

void SendKey(BYTE vkKey, cKeyEvent keyevent = KEY_DOWN)
{	
	DWORD _keyevent = 0x0;
	if (keyevent == KEY_UP) _keyevent = KEYEVENTF_KEYUP; 
	//else _keyevent = KEYEVENTF_EXTENDEDKEY; 

	passNextKey = TRUE;
	keybd_event(vkKey, MapVirtualKey(vkKey, 0), _keyevent,0 );

	//wait until the key is process by the hook or wait 1s until break out while
	int k=11;
	while (passNextKey && (k>1)) {Sleep(3); k--;} 

}

void SendMouseClick(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo)
{	
	passNextClick = TRUE;
	mouse_event(dwFlags, dx, dy, dwData, dwExtraInfo);

	//wait until the Click is process by the hook or wait 1s until break out while
	int k = 11;
	while (passNextClick && (k>1)) { Sleep(3); k--; OutputDebugStringA(LPCSTR("SendMouseClick - wait\n"));}
	//OutputDebugStringA(LPCSTR("SendMouseClick - end\n"));

}


// Start point of the program
// Start keyboard hook, init config dialog and notification icon
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = szWindowClass;
	wcex.hIconSm        = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex)) return 1;

	hInst = hInstance; // Store instance handle in our global variable
	
	// a dummy hidden dialog to receive the message
	hHiddenDialog = CreateWindow(szWindowClass,	szTitle, WS_TILED|WS_CAPTION|WS_THICKFRAME| WS_MINIMIZEBOX , CW_USEDEFAULT, CW_USEDEFAULT,390, 310, NULL,NULL,hInstance,NULL);
	
	if (!hHiddenDialog)	return 1;

	// add button OK
	HMENU OKButtonID = reinterpret_cast<HMENU>(static_cast<DWORD_PTR>(SM_CLOSE));
	HWND hButton = CreateWindowExW(0, L"Button", L"OK", WS_TABSTOP | WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON, 140, 228, 100, 25, hHiddenDialog, OKButtonID, hInst, nullptr);
	SetWindowLongPtr(hButton, GWLP_ID, static_cast<LONG_PTR>(static_cast<DWORD_PTR>(SM_CLOSE)));
		
	// setup the icon
	ZeroMemory(&niData,sizeof(NOTIFYICONDATA));
	niData.cbSize = sizeof(NOTIFYICONDATA);
	niData.uID = TRAY_ICON_ID;
	niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

	// load the icon
	niData.hIcon = (HICON)LoadImage(hInst,MAKEINTRESOURCE(IDI_ICON1),
		IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),
		LR_DEFAULTCOLOR);

	niData.hWnd = hHiddenDialog;
	niData.uCallbackMessage = SWM_TRAYMSG;

	// tooltip message
	copyString(niData.szTip, szTip);
	Shell_NotifyIcon(NIM_ADD,&niData);

	// free icon handle
	if (niData.hIcon && DestroyIcon(niData.hIcon)) niData.hIcon = NULL;
	
	// setup keyboard hook
	kbhHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC) KBHookProc, hInst, NULL);  

	// setup mouse hook
#if TWOMOUSE_MIDDLE
	if (ThreeFingerTap) {mousehHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)MouseHookProc, hInst, NULL); }
#endif

	// Reposition the window
	int ScreenX=0;
	int ScreenY=0;
	int WinX=0;
	int WinY=0;
	RECT wnd;
	GetWindowRect(hHiddenDialog, &wnd);
	WinX = wnd.right - wnd.left;
	WinY = wnd.bottom - wnd.top;
	ScreenX = GetSystemMetrics(SM_CXSCREEN);
	ScreenY = GetSystemMetrics(SM_CYSCREEN);
	ScreenX = (ScreenX / 2) - ((int)WinX / 2);
	ScreenY = (ScreenY / 2) - ((int)WinY / 2);
	SetWindowPos(hHiddenDialog,HWND_TOP, ScreenX, ScreenY, (int)WinX,(int)WinY,NULL);
		
	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int) msg.wParam; // program end here
}

// Timeout of press and hold ALT and also WIN
void timerTick()
{
	StopTimeOut();
	if (LAltDown)
	{
		OutputDebugStringA(LPCSTR("timerTick : LAltDown ==> kill_tab = false \n"));
		kill_Tab = FALSE;
		//kill_LAlt = FALSE; // just kill TAB so Alt - LEFT = back
		//SendKey(VK_LMENU);
		return;
	}
	
	if (LWinDown)
	{
		OutputDebugStringA(LPCSTR("timerTick : re-send VK_LWIN down\n"));
		kill_LWin = FALSE;
		LWinDown = FALSE;
		SendKey(VK_LWIN);
		return;
	}
	if (RButtonDown)
	{
		OutputDebugStringA(LPCSTR("timerTick : re-send WM_RBUTTONDOWN \n"));
		RButtonDown = FALSE;
		SendMouseClick(MOUSEEVENTF_RIGHTDOWN, 0, 0, NULL, NULL);
		return;
	}
}

// Handle message for the hidden dialog
// Show text on main dialog
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR text1[] = _T("mMouse v0.4a");
	TCHAR text2[] = _T("Touchpad fix for windows 10 Asus laptop, which");
	TCHAR text3[] = _T("ASUS not-so-Smart gesture epically fails for advanced");
	TCHAR text4[] =	_T("user like yourself.");
	TCHAR text5[] = _T("How does it work?");
	TCHAR text6[] = _T("This app will intercept messages that generated by");
	TCHAR text7[] = _T("ASUS not-so-Smart gesture and then send the Middle");
	TCHAR text8[] = _T("mouse button or Backward / Forward button instead");
	TCHAR text0[] = _T("ceezblog.info - 2016");

	TCHAR texta[] = _T("ASUS not-so-Smart gesture");
	TCHAR textb[] = _T("not-so");

	switch (message)
	{
	case WM_KEYDOWN: // same effect as OK = default button
		if (wParam == VK_RETURN || wParam == VK_ESCAPE)
			ShowWindow(hHiddenDialog, SW_HIDE);
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		HICON hIcon;

		hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
		DrawIcon(hdc, 10, 5, hIcon);

		SetTextColor(hdc, RGB(30,160,10)); //green-ish
		TextOut(hdc, 50, 15, text1, int(_tcslen(text1)));
		SetTextColor(hdc, RGB(10,20,130)); //blue-ish
		TextOut(hdc, 10, 110, text5, int(_tcslen(text5)));
		SetTextColor(hdc, RGB(10,10,10)); //black-ish
		TextOut(hdc, 10, 45, text2, int(_tcslen(text2)));
		TextOut(hdc, 10, 65, text3, int(_tcslen(text3)));
		TextOut(hdc, 10, 85, text4, int(_tcslen(text4)));
		TextOut(hdc, 10, 130, text6, int(_tcslen(text6)));
		TextOut(hdc, 10, 150, text7, int(_tcslen(text7)));
		TextOut(hdc, 10, 170, text8, int(_tcslen(text8)));
		TextOut(hdc, 220, 195, text0, int(_tcslen(text0)));		
		SetTextColor(hdc, RGB(180,80,80)); //red-ish
		TextOut(hdc, 10, 65, texta, int(_tcslen(texta)));
		TextOut(hdc, 10, 150, texta, int(_tcslen(texta)));
		SetTextColor(hdc, RGB(140,140,140)); //gray-ish
		TextOut(hdc, 50, 65, textb, int(_tcslen(textb)));
		TextOut(hdc, 50, 150, textb, int(_tcslen(textb)));

		EndPaint(hWnd, &ps);
		break;

	case SWM_TRAYMSG:
		switch(lParam)
		{
		case WM_LBUTTONDBLCLK:
			ShowWindow(hHiddenDialog, SW_SHOW);
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
			break;
		}
		break;
	
	case WM_CLOSE:
		ShowWindow(hHiddenDialog, SW_HIDE);
		break;

	case WM_DESTROY:
		UnhookWindowsHookEx(kbhHook);
		UnhookWindowsHookEx(mousehHook);
		PostQuitMessage(0);
		break;
		
	case WM_TIMER:
		timerTick();
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case SM_DESTROY:
			niData.uFlags = 0;
			Shell_NotifyIcon(NIM_DELETE,&niData);
			PostQuitMessage(0);
			break;
		case SM_THREEMOUSE_TAP:
			ThreeFingerTap = !ThreeFingerTap;
#if TWOMOUSE_MIDDLE
			if (ThreeFingerTap) 
			{ 
				mousehHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)MouseHookProc, hInst, NULL); 
			}
			else { UnhookWindowsHookEx(mousehHook); }
#endif
			break;
		case SM_THREEMOUSE_SWIPE:
			ThreeFingerSwipe = !ThreeFingerSwipe;
			break;
		case SM_THREEMOUSE_SWIPE_UP:
			ThreeFingerSwipeUp = !ThreeFingerSwipeUp;
			break;
		case SM_ABOUTAPP:
			ShowWindow(hHiddenDialog, SW_SHOW);
			break;
		case SM_CLOSE:
			ShowWindow(hHiddenDialog, SW_HIDE);
			break;
		}

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

//Backward button - Not used
void sendMBack()
{
	SendMouseClick(MOUSEEVENTF_XDOWN,0,0,XBUTTON1,NULL);
	SendMouseClick(MOUSEEVENTF_XUP,0,0,XBUTTON1,NULL);
}

//Forward button - Not used
void sendMNext()
{
	SendMouseClick(MOUSEEVENTF_XDOWN,0,0,XBUTTON2,NULL);
	SendMouseClick(MOUSEEVENTF_XUP,0,0,XBUTTON2,NULL);
}

//Middle mouse
DWORD WINAPI sendMMiddle(LPVOID lpParam)
{
	OutputDebugStringA(LPCSTR("(mMouse : send MiddleClick)\n"));
	SendMouseClick(MOUSEEVENTF_MIDDLEDOWN,0,0,NULL,NULL);
	SendMouseClick(MOUSEEVENTF_MIDDLEUP,0,0,NULL,NULL);

	return 0;
}


void sendMMiddleThread()
{
	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		sendMMiddle,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
}


//Right mouse
DWORD WINAPI sendMRight(LPVOID lpParam)
{
	OutputDebugStringA(LPCSTR("(mMouse : send RightClick)\n"));
	SendMouseClick(MOUSEEVENTF_RIGHTDOWN, 0, 0, NULL, NULL);
	SendMouseClick(MOUSEEVENTF_RIGHTUP, 0, 0, NULL, NULL);
	return 0;
}


void sendMRightThread()
{
	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		sendMRight,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
}

#if TWOMOUSE_MIDDLE
// Hook process of mouse hook
// Process cases of mouse-button to determine when to send mouse button 3,4,5
// Everything goes here
// 2-finger tap gives right-click by default, so only need this if we want it to give middle-click.
__declspec(dllexport) LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	

	if (nCode < HC_ACTION)
	{
		OutputDebugStringA(LPCSTR("nCode < HC_ACTION ==> Pass\n"));
		return CallNextHookEx(mousehHook, nCode, wParam, lParam);
	}
	if (passNextClick)
	{


		passNextClick = FALSE;
		if (wParam == WM_RBUTTONDOWN)        { OutputDebugStringA(LPCSTR("passNextClick (WM_RBUTTONDOWN) ==> Pass\n")); }
		else if (wParam == WM_RBUTTONUP)     { OutputDebugStringA(LPCSTR("passNextClick (WM_RBUTTONUP) ==> Pass\n")); }
		else if (wParam == WM_RBUTTONDBLCLK) { OutputDebugStringA(LPCSTR("passNextClick (WM_RBUTTONDBLCLK) ==> Pass\n")); }
		else if (wParam == WM_MBUTTONDOWN)   { OutputDebugStringA(LPCSTR("passNextClick (WM_MBUTTONDOWN) ==> Pass\n")); }
		else if (wParam == WM_MBUTTONUP)     { OutputDebugStringA(LPCSTR("passNextClick (WM_MBUTTONUP) ==> Pass\n")); }
		else if (wParam == WM_MBUTTONDBLCLK) { OutputDebugStringA(LPCSTR("passNextClick (WM_MBUTTONDBLCLK) ==> Pass\n")); }
		else if (wParam == WM_LBUTTONDOWN)   { OutputDebugStringA(LPCSTR("passNextClick (WM_LBUTTONDOWN) ==> Pass\n")); }
		else if (wParam == WM_LBUTTONUP)     { OutputDebugStringA(LPCSTR("passNextClick (WM_LBUTTONUP) ==> Pass\n")); }
		else if (wParam == WM_LBUTTONDBLCLK) { OutputDebugStringA(LPCSTR("passNextClick (WM_LBUTTONDBLCLK) ==> Pass\n")); }
		else if (wParam == WM_MOUSEFIRST)    { OutputDebugStringA(LPCSTR("passNextClick (WM_MOUSEFIRST) ==> Pass\n")); }
		else if (wParam == WM_MOUSEMOVE)     { OutputDebugStringA(LPCSTR("passNextClick (WM_MOUSEMOVE) ==> Pass\n")); }
		else { OutputDebugStringA(LPCSTR("passNextClick ==> Pass\n")); }
		return CallNextHookEx(mousehHook, nCode, wParam, lParam);
	}

	switch (wParam)
	{
		// Three finger Swipe
		case WM_RBUTTONDOWN:
			OutputDebugStringA(LPCSTR("WM_RBUTTONDOWN : "));
			RButtonDown = TRUE;
			if (timerOn) StopTimeOut();
			SetTimeOut();
			OutputDebugStringA(LPCSTR("kill\n"));
			return 1; // kiLl the key (retaure if timeout eg real right click)
			break;

		case WM_RBUTTONUP:
			OutputDebugStringA(LPCSTR("WM_RBUTTONUP : "));
			if (timerOn && RButtonDown)
			{
				StopTimeOut(); // stop LWIN being fired on timer
                RButtonDown = FALSE;
				//KillTimer(hHiddenDialog, DELAY_TIMER_ID);
				//timerOn = FALSE;
                
				OutputDebugStringA(LPCSTR("Kill\n"));
				sendMMiddleThread();

				//OutputDebugStringA(LPCSTR("kill\n"));
				////sendMMiddle(); // dure trop longtemps du coup le clic droit est envoy�
				//Sleep(300);
				/*
				SendMouseClick(MOUSEEVENTF_MIDDLEDOWN, 0, 0, NULL, NULL);
				SendMouseClick(MOUSEEVENTF_MIDDLEUP, 0, 0, NULL, NULL);
				RButtonDown = FALSE;				
				*/
				return 1; //kill the key
			}
			OutputDebugStringA(LPCSTR("pass\n"));
			break;
	}
	//OutputDebugStringA(LPCSTR("Mouse pass\n"));
	return CallNextHookEx(mousehHook, nCode, wParam, lParam);
}
#endif


DWORD WINAPI OpenExplorer(LPVOID lpParam)
{
	// Open Explorer (LWIN + E)
	OutputDebugStringA(LPCSTR("(mMouse : open File Explorer)\n"));
	SendKey(VK_LWIN);
	SendKey('E');
	SendKey('E', KEY_UP);
	SendKey(VK_LWIN, KEY_UP);
	return 0;
}

void OpenExplorerThread()
{
	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		OpenExplorer,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
}

DWORD WINAPI SendBackward(LPVOID lpParam)
{
	OutputDebugStringA(LPCSTR("(mMouse : send backward)\n"));
	SendKey(VK_LEFT);
	SendKey(VK_LEFT, KEY_UP);
	return 0;
}

void SendBackwardThread()
{
	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		SendBackward,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
}


DWORD WINAPI SendForward(LPVOID lpParam)
{
	OutputDebugStringA(LPCSTR("(mMouse : send forward)\n"));
	SendKey(VK_RIGHT);
	SendKey(VK_RIGHT, KEY_UP);
	return 0;
}

void SendForwardThread()
{
	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		SendForward,       // thread function name
		NULL,          // argument to thread function 
		0,                      // use default creation flags 
		NULL);   // returns the thread identifier 
}

// Hook process of Keyboard hook
// Process cases of key-press to determine when to send mouse button 3,4,5
// Everything goes here
__declspec(dllexport) LRESULT CALLBACK KBHookProc (int nCode, WPARAM wParam, LPARAM lParam)
{
	KBHOOKSTRUCT kbh = *(KBHOOKSTRUCT*) lParam;

	if (nCode != HC_ACTION)
		return CallNextHookEx(kbhHook, nCode, wParam, (LPARAM)(&kbh));

	if (passNextKey)
	{
		passNextKey = FALSE;
		if      ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && (kbh.dwKeyCode == VK_LMENU)) { OutputDebugStringA(LPCSTR("passNextKey (VK_LMENU Down) ==> Pass\n")); }
		else if ((wParam == WM_KEYUP   || wParam == WM_SYSKEYUP)   && (kbh.dwKeyCode == VK_LMENU)) { OutputDebugStringA(LPCSTR("passNextKey (VK_LMENU Up) ==> Pass\n")); }
		else if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && (kbh.dwKeyCode == VK_LEFT))  { OutputDebugStringA(LPCSTR("passNextKey (VK_LEFT Down) ==> Pass\n")); }
		else if ((wParam == WM_KEYUP   || wParam == WM_SYSKEYUP)   && (kbh.dwKeyCode == VK_LEFT))  { OutputDebugStringA(LPCSTR("passNextKey (VK_LEFT Up) ==> Pass\n")); }
		else if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && (kbh.dwKeyCode == VK_RIGHT)) { OutputDebugStringA(LPCSTR("passNextKey (VK_RIGHT Down) ==> Pass\n")); }
		else if ((wParam == WM_KEYUP   || wParam == WM_SYSKEYUP)   && (kbh.dwKeyCode == VK_RIGHT)) { OutputDebugStringA(LPCSTR("passNextKey (VK_RIGHT Up) ==> Pass\n")); }
		else if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && (kbh.dwKeyCode == VK_LWIN))  { OutputDebugStringA(LPCSTR("passNextKey (VK_LWIN Down) ==> Pass\n")); }
		else if ((wParam == WM_KEYUP   || wParam == WM_SYSKEYUP)   && (kbh.dwKeyCode == VK_LWIN))  { OutputDebugStringA(LPCSTR("passNextKey (VK_LWIN Up) ==> Pass\n")); }
		else if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && (kbh.dwKeyCode == 'E'))  { OutputDebugStringA(LPCSTR("passNextKey ('E' Down) ==> Pass\n")); }
		else if ((wParam == WM_KEYUP   || wParam == WM_SYSKEYUP)   && (kbh.dwKeyCode == 'E'))  { OutputDebugStringA(LPCSTR("passNextKey ('E' Up) ==> Pass\n")); }
		else { OutputDebugStringA(LPCSTR("passNextKey ==> Pass\n")); }
		return CallNextHookEx(kbhHook, nCode, wParam, (LPARAM)(&kbh)); 
	}
	
	if (!ThreeFingerTap && !ThreeFingerSwipe && !ThreeFingerSwipeUp)
		return CallNextHookEx(kbhHook, nCode, wParam, (LPARAM)(&kbh));

	//if (killNextKey)
	//{
	//	killNextKey = FALSE;
	//	return 1;
	//}

	//////////////////////////
	// KEYDOWN / SYSKEYDOWN event
	if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
	{
		switch (kbh.dwKeyCode)
		{
		// Three finger Swipe
		case VK_LMENU:
			OutputDebugStringA(LPCSTR("VK_MENU (LALT) Down : "));
			LAltDown = TRUE;
			if (LWinDown) break;
			if (!ThreeFingerSwipe) break;
			
			kill_Tab=TRUE;
			//kill_LAlt = TRUE;
			if (timerOn) StopTimeOut();
			SetTimeOut();
			//return 1; // since use Alt-Left instead of mouse 4 button
			break;

		case VK_TAB:
			OutputDebugStringA(LPCSTR("VK_TAB Down : "));
				if (!ThreeFingerSwipe && !ThreeFingerSwipeUp && !ThreeFingerTap) break;
			// if alt is not press in time or ALT is not held down
			if (LWinDown) //three finger swipe up gesture
			{
				if (ThreeFingerSwipeUp)
				{
					
					OpenFileExplorer = TRUE;
					kill_Tab = TRUE;
					kill_LWin = TRUE;
					
					//if (timerOn) { StopTimeOut(); /*Sleep(10);*/ }
					OutputDebugStringA(LPCSTR("kill\n"));
					return 1;
				}
				else
				{
					if (timerOn) { StopTimeOut(); timerTick(); Sleep(10); }
				}
				
			}
			if (!LAltDown) break;
			if (!timerOn) break;

			// timer is on, and alt is held down			
			StopTimeOut();
			SwipeReady = TRUE;
			SwipeLock = TRUE; //we got a lock on this
			kill_Tab = TRUE;
			OutputDebugStringA(LPCSTR("kill\n"));
			return 1;
			break;

		case VK_LEFT:
			OutputDebugStringA(LPCSTR("VK_LEFT Down : "));
			if (LWinDown)
			{
				if (timerOn) {StopTimeOut(); timerTick();Sleep(10);}
				break;
			}
			if (SwipeLock)
			{
				kill_Leftkey = TRUE;
				OutputDebugStringA(LPCSTR("kill\n"));
				if (SwipeReady) 
				{
					//OutputDebugStringA(LPCSTR("(mMouse : send backward)\n"));
					//SendKey(VK_LEFT);
					//SendKey(VK_LEFT,KEY_UP);
					SendBackwardThread();
					SwipeReady = FALSE;
				}				
				return 1;
			}
			break;

		case VK_RIGHT:
			OutputDebugStringA(LPCSTR("VK_RIGHT Down : "));
			if (LWinDown)
			{
				if (timerOn) {StopTimeOut(); timerTick();Sleep(10);}
				break;
			}
			if (SwipeLock)
			{
				kill_RightKey = TRUE;
				OutputDebugStringA(LPCSTR("kill\n"));
				if (SwipeReady) 
				{
					//OutputDebugStringA(LPCSTR("(mMouse : send forward)\n"));
					//SendKey(VK_RIGHT);
					//SendKey(VK_RIGHT,KEY_UP);
					SendForwardThread();
					SwipeReady = FALSE;
				}
				return 1;
			}
			break;

		// Three finger Tap
		case VK_LWIN:
			OutputDebugStringA(LPCSTR("VK_LWIN Down : "));
						
			if (LAltDown) break;
			if (!ThreeFingerTap && !ThreeFingerSwipeUp) break;

			LWinDown = TRUE;
			if (ThreeFingerTap) { Kill_SKey = FALSE; }
			kill_LWin = TRUE;
			//keyCounter = 1;
			if (timerOn) StopTimeOut();
			SetTimeOut();
			OutputDebugStringA(LPCSTR("kill\n"));
			return 1; // kill the key
			break;

		case VK_S:
			OutputDebugStringA(LPCSTR("VK_S Down : "));
			if (timerOn && ThreeFingerTap && LWinDown)
			{
				StopTimeOut(); // stop LWIN being fired on timer
				Kill_SKey = TRUE; // we have a match 's'
				OutputDebugStringA(LPCSTR("kill\n"));
				//sendMMiddle();
#if THREEMOUSE_RIGHT
				sendMRightThread();
#elif THREEMOUSE_MIDDLE
				sendMMiddleThread();
#endif
				return 1; //kill the key
			}
			else if (LWinDown)
			{
				// LWIN + S with TreeFigerTap desactivate
				// send LWIN Key
				
				StopTimeOut(); timerTick(); //SendKey(VK_LWIN); before VK_S
				Kill_SKey = FALSE;
				kill_LWin = FALSE;

				//SendKey('S');
				//SendKey('S', KEY_UP);
				//SendKey(VK_LWIN, KEY_UP);
				//return -1;
			}
			break;

		default: //if other key is pressed, just pass it
			OutputDebugStringA(LPCSTR("other key Down : "));

			if (LWinDown && timerOn) {StopTimeOut(); timerTick();Sleep(10);}
				break; 

		}

		// if other keys, just do nothing at all
	}

	//////////////////////////
	// KEYUP / SYSKEYUP event
	if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
	{
		switch (kbh.dwKeyCode)
		{
		// Three finger Swipe
		case VK_LMENU:
			OutputDebugStringA(LPCSTR("VK_MENU (LALT) Up : "));
			LAltDown = FALSE;
			SwipeLock = FALSE;
			SwipeReady = FALSE;
			if (!ThreeFingerSwipe) break;
			
			if (timerOn)
			{
				StopTimeOut();
				kill_Tab = FALSE;
			}
			break;
			
		case VK_TAB:
			OutputDebugStringA(LPCSTR("VK_TAB Up : "));
			if (kill_Tab) 
			{
				kill_Tab=FALSE;
				OutputDebugStringA(LPCSTR("kill\n"));
				return 1;
			}
			break;

		case VK_LEFT:
			OutputDebugStringA(LPCSTR("VK_LEFT Up : "));
			if (kill_Leftkey) {kill_Leftkey=FALSE; OutputDebugStringA(LPCSTR("kill\n")); return 1;}
			break;

		case VK_RIGHT:
			OutputDebugStringA(LPCSTR("VK_RIGHT Up : "));
			if (kill_RightKey) {kill_RightKey=FALSE; OutputDebugStringA(LPCSTR("kill\n")); return 1;}
			break;

		// Three finger tap
		case VK_LWIN:
			OutputDebugStringA(LPCSTR("VK_LWIN Up : "));
			if (kill_LWin) { OutputDebugStringA(LPCSTR("kill\n")); }
			if (OpenFileExplorer)
			{
				// Open Explorer (LWIN + E)
				//OutputDebugStringA(LPCSTR("(mMouse : open File Explorer)\n"));
				//SendKey(VK_LWIN);
				//SendKey('E');
				//SendKey('E', KEY_UP);
				//SendKey(VK_LWIN, KEY_UP);
				OpenExplorerThread();
				OpenFileExplorer = FALSE;
			}
			LWinDown = FALSE;
			if (kill_LWin) {kill_LWin=FALSE; return 1;}
			break;

		case VK_S:
			OutputDebugStringA(LPCSTR("VK_S Up : "));
			if (Kill_SKey) {Kill_SKey=FALSE; OutputDebugStringA(LPCSTR("kill\n")); return 1;}
			break;

		default: //if other key is pressed, just pass it
			OutputDebugStringA(LPCSTR("other key Up : "));
		}
	}

	OutputDebugStringA(LPCSTR("pass\n"));
	return CallNextHookEx(kbhHook, nCode, wParam, (LPARAM)(&kbh));
}
