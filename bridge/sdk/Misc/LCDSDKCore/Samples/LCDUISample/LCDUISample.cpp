//************************************************************************
//  The Logitech LCD SDK, including all acompanying documentation,
//  is protected by intellectual property laws.  All use of the Logitech
//  LCD SDK is subject to the License Agreement found in the
//  "Logitech LCD SDK License Agreement" file and in the Reference Manual.  
//  All rights not expressly granted by Logitech are reserved.
//************************************************************************

/***************************************************************

Sample using LCDUI

LCDUI contains many helper classes to make it easier to write to
the LCD, while still allowing you flexibility.  Although EZLCD 
makes implementation very easy, it has its limitations.  Using
the SDK at the level of LCDUI allows you the following:
    - Control over callback functions
    - Creation of custom controls
    - More options on existing controls (text size, scroll rate)
    - Finer control over when to update, draw, or read button data

This example will cover the following:
    - Initialization and clean-up
    - Handling softbuttons with a callback
    - Adding an existing control (Monochrome text)
    - Adding a custom control (OpenGL)

***************************************************************/

// LCDUISample.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "LCDUISample.h"

#include "lglcd.h"
#include "LCDUI\LCDConnection.h"
#include "LCDUI\LCDText.h"
#include "LCDUI\LCDColorText.h"

#include "OGLObject.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

//Variables we'll need
HWND g_hwnd = NULL;
CLCDConnection g_Connection;
TCHAR g_AppletTitle[] = _T("LCDUI Sample");
COGLObject g_OGLObj;
CLCDText g_MonoText;
lgLcdSoftbuttonsChangedContext g_SBContext;

//Some variables for rendering
DWORD g_PrevTime = 0;
float g_Angle = 0.0f;
float cx = 160;
float cy = 120;


// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

static DWORD WINAPI OnButtonCB(IN int connection, IN DWORD dwButtons, IN const PVOID pContext);


void SetupRendering();
void DoRendering(DWORD timestamp);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_LCDUISAMPLE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LCDUISAMPLE));

    /*
    Here we initialize our connection
    */

    //Create a connection context and connect to LCDMon
    lgLcdConnectContextEx ConnectCtx;
    ConnectCtx.appFriendlyName = g_AppletTitle;

    //This part tells LCDMon what you want to display on.
    //Use LGLCD_APPLET_CAP_BW for monochrome, and
    //LGLCD_APPLET_CAP_QVGA for color.  Or them together
    //to be dual mode.  See the lgcd.h documentation
    //for more details on dual mode applets.
    ConnectCtx.dwAppletCapabilitiesSupported = LGLCD_APPLET_CAP_BW | LGLCD_APPLET_CAP_QVGA;

    //We don't want to autostart a sample
    ConnectCtx.isAutostartable = FALSE;

    //Persistence has been deprecated, so we will skip that field
    //ConnectCtx.isPersistent = doesn't matter

    //This example does not cover the configure callback, but it
    //is very similar to setting up notifications.
    ConnectCtx.onConfigure.configCallback = NULL;
    ConnectCtx.onConfigure.configContext = NULL;

    //In this sample, we are using the default notification
    ConnectCtx.onNotify.notificationCallback = NULL;
    ConnectCtx.onNotify.notifyContext = NULL;

    //Let's use our softbutton callback
    g_SBContext.softbuttonsChangedCallback = OnButtonCB;
    g_SBContext.softbuttonsChangedContext = g_hwnd;

    //Initialize your connection
    //We are using our own softbutton callback
    if( FALSE == g_Connection.Initialize(ConnectCtx, &g_SBContext) )
    {
        return -1;
    }

    

    //Add your monochrome page
    CLCDOutput* pMonoOutput = g_Connection.MonoOutput();
    CLCDPage m_MonoPage;

    pMonoOutput->ShowPage(&m_MonoPage);

    //For monochrome, let's just display some text
    g_MonoText.SetText( _T("Hello monochrome display.\n") );
    g_MonoText.SetOrigin(0,0);
    g_MonoText.SetSize(160, 16);
    g_MonoText.SetFontPointSize(8);
    m_MonoPage.AddObject(&g_MonoText);
      
    
    //Add your color page
    CLCDOutput* pColorOutput = g_Connection.ColorOutput();
    CLCDPage m_ColorPage;

    pColorOutput->ShowPage(&m_ColorPage);

    //Add our new OpenGL object
    //We're going to take up the entire screen with it
    g_OGLObj.Initialize(320,240);
    m_ColorPage.AddObject(&g_OGLObj);


    //Let's setup some OpenGL stuff in this function
    g_OGLObj.MakeCurrent();
    SetupRendering();

	// Main message loop:
	BOOL Done = FALSE;
    DWORD timestamp;



    while(!Done)									
	{
		if( PeekMessage(&msg,NULL,0,0,PM_REMOVE) )	
		{
			if (msg.message==WM_QUIT)				
			{
				Done = TRUE;							
			}
			else									
			{
				TranslateMessage(&msg);				
				DispatchMessage(&msg);				
			}
		}
        else
        {
            timestamp = GetTickCount();
            g_OGLObj.BeginDraw();
            //Do OpenGL rendering here
            //One perk of OpenGL is that we do not have to put this rendering code
            //inside of COGLObject's OnDraw class.
            DoRendering(timestamp);
            g_OGLObj.EndDraw();

            //The update will do the rendering of any LCDUI objects we added to pages
            g_Connection.Update();

            //This loop goes very fast, so let's throttle it a bit
            Sleep(33);
        }
    }

    //Shutdown the connection
    //(also called in the destructor)
    g_Connection.Shutdown();

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LCDUISAMPLE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_LCDUISAMPLE);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   g_hwnd = hWnd;

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code here...
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

DWORD WINAPI OnButtonCB(IN int connection, IN DWORD dwButtons, IN const PVOID pContext)
{
    //Let's move around the triangle with the direction buttons
    if( dwButtons & LGLCDBUTTON_LEFT )
    {
        cx -= 10.0f;
    }
    else if( dwButtons & LGLCDBUTTON_RIGHT )
    {
        cx += 10.0f;
    }

    if( dwButtons & LGLCDBUTTON_UP )
    {
        cy -= 10.0f;
    }
    else if( dwButtons & LGLCDBUTTON_DOWN )
    {
        cy += 10.0f;
    }

    return 0;
}

void SetupRendering()
{
    //Let's do some 2D rendering

    int vPort[4];

    glGetIntegerv(GL_VIEWPORT, vPort);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glClearColor(1.0f,1.0f,1.0f,1.0f);

    glOrtho(0, vPort[2], 0, vPort[3], -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();    
}

void DoRendering(DWORD timestamp)
{
    if( g_PrevTime == 0 )
    {
        g_PrevTime = timestamp;
    }

    glClear(GL_COLOR_BUFFER_BIT);   

    //A spinning triangle
    glLoadIdentity();
    glTranslatef(cx, cy, 0.0f);
    glScalef(100.0f, 100.0f, 1.0f);
    glRotatef(g_Angle, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLES);

    glColor3ub(0, 0, 255);
    glVertex2f(0.0f, 1.0f);

    glColor3ub(0, 255, 0);
    glVertex2f(0.866f, -0.5f);

    glColor3ub(255, 0, 0);
    glVertex2f(-0.866f, -0.5f);

    glEnd();

    if( timestamp - g_PrevTime > 33 )
    {
        g_PrevTime = timestamp;

        g_Angle += 3.0f;
        if( g_Angle > 360.0f )
            g_Angle = 0.0f;
    }
}