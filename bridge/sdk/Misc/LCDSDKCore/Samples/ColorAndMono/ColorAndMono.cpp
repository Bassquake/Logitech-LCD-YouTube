//************************************************************************
//  The Logitech LCD SDK, including all acompanying documentation,
//  is protected by intellectual property laws.  All use of the Logitech
//  LCD SDK is subject to the License Agreement found in the
//  "Logitech LCD SDK License Agreement" file and in the Reference Manual.  
//  All rights not expressly granted by Logitech are reserved.
//************************************************************************

//************************************************************************
//
// ColorAndMono.cpp
//
//
// This sample will implement a "Dual Mode" applet: one that supports
// both color and monochrome. It will show you how to dynamically detect
// supported devices using the callback mechanism.
//
// If you do not have a device with a color display, right-click on the
// LCD Manager system tray icon, keeping the left CTRL and SHIFT keys pressed to 
// reveal a color-emulator menu option.
//
// Logitech LCD SDK
//
// Copyright 2008 Logitech Inc.
//************************************************************************

#include "stdafx.h"
#include "ColorAndMono.h"
#include "..\..\Src\lglcd.h"


// Add an explicit link to lgLcd.lib
#pragma comment(lib, "..\\..\\Libs\\x86\\lgLcd.lib")


// Global Variables:
HWND g_hWnd                 = NULL;
HINSTANCE g_hInst           = NULL;
LPCTSTR g_szName            = _T("ColorAnd2Mono Sample");
DWORD_PTR g_dwTimerId       = 0xABBABABE;
lgLcdConnectContextEx       g_LCDConnectionCtx;
APPLET_STATE                g_AppletState;


// When an LGLCD notification occurs, we transfer it as a message
// so that it is processed by the main thread
// PostMessage messages
#define WM_LGLCD_DEVICE_ARRIVAL         WM_APP + 1
#define WM_LGLCD_DEVICE_REMOVAL         WM_APP + 2
#define WM_LGLCD_CONNECTION_CLOSED      WM_APP + 3
#define WM_LGLCD_APPLET_ENABLED         WM_APP + 4
#define WM_LGLCD_BUTTON_EVENT           WM_APP + 5


// Forward declarations of functions included in this code module:
DWORD CALLBACK      OnLCDButtonsCallback(int device, DWORD dwButtons, const PVOID pContext);
DWORD CALLBACK      OnLCDNotificationCallback(int connection,
                                              const PVOID pContext,
                                              DWORD notificationCode,
                                              DWORD notifyParm1,
                                              DWORD notifyParm2,
                                              DWORD notifyParm3,
                                              DWORD notifyParm4);


//************************************************************************
//
// InitializeLCD
//
// This is where we will do the main LCD library initialization
//************************************************************************

BOOL InitializeLCD(void)
{
    int ret = 0;

    // The first thing to do is initialize the library using lgLcdInit
    // This requires that the Logitech GamePanel software is installed
    // on the target
    ret = lgLcdInit();
    if (ERROR_SUCCESS != ret)
    {
        // ERROR. Most likely the GamePanel software is not installed
        return FALSE;
    }

    // Inititalize the applet state
    ZeroMemory(&g_AppletState, sizeof(g_AppletState));
    g_AppletState.isEnabled = TRUE;
    g_AppletState.Color.nDeviceId = LGLCD_INVALID_DEVICE;
    g_AppletState.Mono.nDeviceId = LGLCD_INVALID_DEVICE;

    // Initialize our connection state
    ZeroMemory(&g_LCDConnectionCtx, sizeof(g_LCDConnectionCtx));
    // appFriendlyName: Display name of the applet
    g_LCDConnectionCtx.appFriendlyName = g_szName;
    // isAutostartable: Set to TRUE if you want your applet to start when LCD Manager starts
    g_LCDConnectionCtx.isAutostartable = FALSE;         
    // dwAppletCapabilitiesSupported: We support color and monochrome
    g_LCDConnectionCtx.dwAppletCapabilitiesSupported = (LGLCD_APPLET_CAP_BW|LGLCD_APPLET_CAP_QVGA);
    // connection: Initialize with LGLCD_INVALID_CONNECTION
    g_LCDConnectionCtx.connection = LGLCD_INVALID_CONNECTION;
    // onNotify: Set our internal callback
    g_LCDConnectionCtx.onNotify.notificationCallback = OnLCDNotificationCallback;
    g_LCDConnectionCtx.onNotify.notifyContext = NULL;

    // Start our loop
    g_dwTimerId = SetTimer(g_hWnd, g_dwTimerId, 1000, NULL);

    return TRUE;
}


//************************************************************************
//
// OnTimerCallback
//
// Where the display output happens. We will:
//
// - Check if we need to attempt connect with the LCD Manager
// - If we have a color device, output data to that color device
// - If we have a monochrome device, output data to that monochrome device
//************************************************************************

void OnTimerCallback(void)
{
    // Check if we have an invalid connection. If so, attempt a connection
    if (LGLCD_INVALID_CONNECTION == g_LCDConnectionCtx.connection)
    {
        // attempt to connect
        if (ERROR_SUCCESS != lgLcdConnectEx(&g_LCDConnectionCtx))
        {
            return;
        }
        else
        {
            OutputDebugString(_T("Connected to LCD Manager\n"));
        }
    }

    // If the applet is disabled, then we don't need to bother displaying
    // anything
    if (FALSE == g_AppletState.isEnabled)
    {
        return;
    }

    // Check if we have a color device
    if (g_AppletState.Color.nDeviceId != LGLCD_INVALID_DEVICE)
    {
        // draw color
        lgLcdBitmapQVGAx32 ColorFrame;

        // Use the tick count to determine a color
        // Colors are in the RGBA format
        // Let's alternate between red, green, and blue
        ColorFrame.hdr.Format = LGLCD_BMP_FORMAT_QVGAx32;
        DWORD* pPixel = (DWORD*)ColorFrame.pixels;
        static int nCurColor = 0;

        DWORD colors[3] = {
            0x00ff0000,     // Red
            0x0000ff00,     // Green
            0x000000ff      // Blue
        };

        for (int i = 0; i < (LGLCD_QVGA_BMP_WIDTH * LGLCD_QVGA_BMP_HEIGHT); i++)
        {
            pPixel[i] = colors[nCurColor % 3];
        }
        nCurColor++;

        lgLcdUpdateBitmap(g_AppletState.Color.nDeviceId, &ColorFrame.hdr,
            LGLCD_ASYNC_UPDATE(LGLCD_PRIORITY_NORMAL));
    }

    // Check if we have a BW device
    if (g_AppletState.Mono.nDeviceId != LGLCD_INVALID_DEVICE)
    {
        // draw monochrome
        lgLcdBitmap160x43x1 MonoFrame;

        // Draw (somewhat) random bytes
        // Note: >= 128 is ON and < 128 is OFF
        MonoFrame.hdr.Format = LGLCD_BMP_FORMAT_160x43x1;
        BYTE* pPixel = (BYTE*)MonoFrame.pixels;
        for (int i = 0; i < (LGLCD_BW_BMP_WIDTH * LGLCD_BW_BMP_HEIGHT); i++)
        {
            pPixel[i] = (BYTE)rand();
        }

        lgLcdUpdateBitmap(g_AppletState.Mono.nDeviceId, &MonoFrame.hdr,
            LGLCD_ASYNC_UPDATE(LGLCD_PRIORITY_NORMAL));
    }
}


//************************************************************************
//
// HandleButtonPress
//
//************************************************************************

void HandleButtonPress(void)
{
    OutputDebugString(_T("BUTTON EVENT\n"));

    // Handle the monochrome button state
    if (g_AppletState.Mono.dwButtonState)
    {
        OutputDebugString(_T("  MONOCHROME: "));
        if (g_AppletState.Mono.dwButtonState & LGLCDBUTTON_BUTTON0)
        {
            OutputDebugString(_T("BUTTON0 "));
        }
        if (g_AppletState.Mono.dwButtonState & LGLCDBUTTON_BUTTON1)
        {
            OutputDebugString(_T("BUTTON1 "));
        }
        if (g_AppletState.Mono.dwButtonState & LGLCDBUTTON_BUTTON2)
        {
            OutputDebugString(_T("BUTTON2 "));
        }
        if (g_AppletState.Mono.dwButtonState & LGLCDBUTTON_BUTTON3)
        {
            OutputDebugString(_T("BUTTON3 "));
        }
        OutputDebugString(_T("\n"));

    }

    // Handle the color button state
    if (g_AppletState.Color.dwButtonState)
    {
        OutputDebugString(_T("  COLOR: "));
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_LEFT)
        {
            OutputDebugString(_T("LGLCDBUTTON_LEFT "));
        }
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_RIGHT)
        {
            OutputDebugString(_T("LGLCDBUTTON_RIGHT "));
        }
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_UP)
        {
            OutputDebugString(_T("LGLCDBUTTON_UP "));
        }
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_DOWN)
        {
            OutputDebugString(_T("LGLCDBUTTON_DOWN "));
        }
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_OK)
        {
            OutputDebugString(_T("LGLCDBUTTON_OK "));
        }
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_MENU)
        {
            OutputDebugString(_T("LGLCDBUTTON_MENU "));
        }
        if (g_AppletState.Color.dwButtonState & LGLCDBUTTON_CANCEL)
        {
            OutputDebugString(_T("LGLCDBUTTON_CANCEL "));
        }
        OutputDebugString(_T("\n"));
    }
}



//************************************************************************
//
// OnLCDNotificationCallback
//
// IMPORTANT: This is called from a different thread than the main
// thread. You must take care to be thread-safe and return fast
//
// All we do here is pass on the notification to the main thread
//************************************************************************

DWORD CALLBACK OnLCDNotificationCallback(int connection,
                                         const PVOID pContext,
                                         DWORD notificationCode,
                                         DWORD notifyParm1,
                                         DWORD notifyParm2,
                                         DWORD notifyParm3,
                                         DWORD notifyParm4)
{    
    OutputDebugString(_T("OnLCDNotificationCallback\n"));
    switch(notificationCode)
    {
    case LGLCD_NOTIFICATION_DEVICE_ARRIVAL:
        PostMessage(g_hWnd, WM_LGLCD_DEVICE_ARRIVAL, (WPARAM)notifyParm1, 0);
        break;

    case LGLCD_NOTIFICATION_DEVICE_REMOVAL:
        PostMessage(g_hWnd, WM_LGLCD_DEVICE_REMOVAL, (WPARAM)notifyParm1, 0);
        break;

    case LGLCD_NOTIFICATION_APPLET_ENABLED:
    case LGLCD_NOTIFICATION_APPLET_DISABLED:
        PostMessage(g_hWnd, WM_LGLCD_APPLET_ENABLED, (WPARAM)notificationCode, 0);
        break;

    case LGLCD_NOTIFICATION_CLOSE_CONNECTION:
        PostMessage(g_hWnd, WM_LGLCD_CONNECTION_CLOSED, (WPARAM)notificationCode, 0);
        break;

    default:
        break;
    }

    return 1;
}


//************************************************************************
//
// OnLCDButtonsCallback
//
// IMPORTANT: This is called from a different thread than the main
// thread. You must take care to be thread-safe and return fast
//************************************************************************

DWORD CALLBACK OnLCDButtonsCallback(int device, DWORD dwButtons, const PVOID pContext)
{
    PostMessage(g_hWnd, WM_LGLCD_BUTTON_EVENT, (WPARAM)dwButtons, (LPARAM)device);
    return 0;
}


//************************************************************************
//
// WndProc
//
//************************************************************************

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TIMER:
        OnTimerCallback();
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_LGLCD_DEVICE_ARRIVAL:
        // Check the capability of the new device, and then open up the
        // appropriate device
        // You will only get ONE arrival per device capability
        // So, if you have 2 monochrome devices, you will still only get
        // 1 arrival, but writing to that one device will multicast the
        // the output to both devices
        {
            lgLcdOpenByTypeContext OpenCtx;
            ZeroMemory(&OpenCtx, sizeof(OpenCtx));

            int nDeviceType = (int)wParam;

            //The connection class has already filled in the connection index.
            //OpenCtx.connection = doesn't matter
            OpenCtx.device = LGLCD_INVALID_DEVICE;
            OpenCtx.deviceType = nDeviceType;
            OpenCtx.connection = g_LCDConnectionCtx.connection;
            OpenCtx.onSoftbuttonsChanged.softbuttonsChangedCallback = OnLCDButtonsCallback;
            OpenCtx.onSoftbuttonsChanged.softbuttonsChangedContext = NULL;
            if (ERROR_SUCCESS == lgLcdOpenByType(&OpenCtx))
            {
                if (LGLCD_DEVICE_BW == wParam)
                {
                    OutputDebugString(_T("Monochrome device was found\n"));
                    g_AppletState.Mono.nDeviceId = OpenCtx.device;
                }
                else if (LGLCD_DEVICE_QVGA == wParam)
                {
                    OutputDebugString(_T("Color device was found\n"));
                    g_AppletState.Color.nDeviceId = OpenCtx.device;
                }
            }
        }
        break;

    case WM_LGLCD_DEVICE_REMOVAL:
        // It's important that the devices are closed upon notification
        // Note that the notification is NOT the device ID, but rather
        // the device capability
        if (LGLCD_DEVICE_BW == wParam)
        {
            OutputDebugString(_T("Monochrome device was removed\n"));
            lgLcdClose(g_AppletState.Mono.nDeviceId );
            g_AppletState.Mono.nDeviceId = LGLCD_INVALID_DEVICE;
        }
        else if (LGLCD_DEVICE_QVGA == wParam)
        {
            OutputDebugString(_T("Color device was removed\n"));
            lgLcdClose(g_AppletState.Color.nDeviceId );
            g_AppletState.Color.nDeviceId = LGLCD_INVALID_DEVICE;
        }
        break;

    case WM_LGLCD_APPLET_ENABLED:
        // One good optimization is to disable LCD rendering if the applet
        // is disabled
        g_AppletState.isEnabled = (LGLCD_NOTIFICATION_APPLET_ENABLED == wParam) ? TRUE : FALSE;
        if (g_AppletState.isEnabled)
        {
            OutputDebugString(_T("Applet was enabled\n"));
        }
        else
        {
            OutputDebugString(_T("Applet was disabled\n"));
        }
        break;

    case WM_LGLCD_CONNECTION_CLOSED:
        // Dereference the connection handle, and close all the open devices
        OutputDebugString(_T("LCD Manager has closed its connection\n"));
        if (LGLCD_INVALID_CONNECTION != g_LCDConnectionCtx.connection)
        {
            lgLcdDisconnect(g_LCDConnectionCtx.connection);
            g_LCDConnectionCtx.connection = LGLCD_INVALID_CONNECTION;
        }

        if (LGLCD_INVALID_DEVICE != g_AppletState.Mono.nDeviceId)
        {
            lgLcdClose(g_AppletState.Mono.nDeviceId);
            g_AppletState.Mono.nDeviceId = LGLCD_INVALID_DEVICE;
        }

        if (LGLCD_INVALID_DEVICE != g_AppletState.Color.nDeviceId)
        {
            lgLcdClose(g_AppletState.Color.nDeviceId);
            g_AppletState.Color.nDeviceId = LGLCD_INVALID_DEVICE;
        }
        break;

    case WM_LGLCD_BUTTON_EVENT:
        // Handling of the button state is left as an exercice t
        if (lParam == g_AppletState.Mono.nDeviceId)
        {
            g_AppletState.Mono.dwButtonState = (DWORD)wParam;
        }

        if (lParam == g_AppletState.Color.nDeviceId)
        {
            g_AppletState.Color.dwButtonState = (DWORD)wParam;
        }
        HandleButtonPress();
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


//************************************************************************
//
// InitInstance
//
//************************************************************************

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_hInst = hInstance; // Store instance handle in our global variable

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc     = WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= NULL;
    wcex.hCursor		    = NULL;
    wcex.hbrBackground	= NULL;
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= g_szName;
    wcex.hIconSm		    = NULL;

    RegisterClassEx(&wcex);

    g_hWnd = CreateWindow(g_szName, g_szName, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!g_hWnd)
    {
        return FALSE;
    }

    // We're hidden
    ShowWindow(g_hWnd, SW_HIDE);

    return TRUE;
}


//************************************************************************
//
// WinMain
//
//************************************************************************

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    // seed
    srand( GetTickCount() );

    // Initialize the LCD portion
    InitializeLCD();

    // Main message loop:
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // De-initialize the library
    lgLcdDeInit();

    return (int) msg.wParam;
}