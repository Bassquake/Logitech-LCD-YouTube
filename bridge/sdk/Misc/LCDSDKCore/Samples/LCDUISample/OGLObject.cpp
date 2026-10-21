//************************************************************************
//  The Logitech LCD SDK, including all acompanying documentation,
//  is protected by intellectual property laws.  All use of the Logitech
//  LCD SDK is subject to the License Agreement found in the
//  "Logitech LCD SDK License Agreement" file and in the Reference Manual.  
//  All rights not expressly granted by Logitech are reserved.
//************************************************************************

#include "LCDUI.h"
#include "OGLObject.h"

COGLObject::COGLObject()
{
    m_pBitmapInfo = NULL;
    m_hDC = NULL;
    m_hBitmap = NULL;
    m_hPrevBitmap = NULL;
    m_pBitmapBits = NULL;

    //OpenGL variables
    m_hRC = NULL;
	m_hPalette = NULL;
    m_GLPixelIndex = 0;
}

COGLObject::~COGLObject()
{
    Shutdown();
}

//CLCDBase
void COGLObject::OnDraw(CLCDGfxBase &rGfx)
{
    POINT pos = GetOrigin();
    int width = GetWidth();
    int height = GetHeight();
    BitBlt(rGfx.GetHDC(), pos.x, pos.y, width, height, m_hDC, 0, 0, SRCCOPY);
}

void COGLObject::OnUpdate(DWORD timestamp)
{

}

//OpenGL
BOOL COGLObject::Initialize(int width, int height)
{
    //--> m_Size
    SetSize(width, height); 

    //Create a bitmap of this size
    m_hDC = CreateCompatibleDC(NULL);
    if(NULL == m_hDC)
    {
        LCDUITRACE(_T("COGLObject::Initialize(): failed to create compatible DC.\n"));
        Shutdown();
        return FALSE;
    }

    int nBMISize = sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD);
    m_pBitmapInfo = (BITMAPINFO *) new BYTE [nBMISize];
    if(NULL == m_pBitmapInfo)
    {
        LCDUITRACE(_T("COGLObject::Initialize(): failed to allocate bitmap info.\n"));
        Shutdown();
        return FALSE;
    }

    ZeroMemory(m_pBitmapInfo, nBMISize);
    m_pBitmapInfo->bmiHeader.biSize = sizeof(m_pBitmapInfo->bmiHeader);
    m_pBitmapInfo->bmiHeader.biWidth = width;
    m_pBitmapInfo->bmiHeader.biHeight = -height;
    m_pBitmapInfo->bmiHeader.biPlanes = 1;
    m_pBitmapInfo->bmiHeader.biBitCount = 32;
    m_pBitmapInfo->bmiHeader.biCompression = BI_RGB;
    m_pBitmapInfo->bmiHeader.biSizeImage = 
        (width * 
        height * 
        m_pBitmapInfo->bmiHeader.biBitCount) / 8;
    m_pBitmapInfo->bmiHeader.biXPelsPerMeter = 3200;
    m_pBitmapInfo->bmiHeader.biYPelsPerMeter = 3200;
    m_pBitmapInfo->bmiHeader.biClrUsed = 256;
    m_pBitmapInfo->bmiHeader.biClrImportant = 256;

    m_hBitmap = CreateDIBSection(m_hDC, m_pBitmapInfo, DIB_RGB_COLORS, (PVOID *) &m_pBitmapBits, NULL, 0);
    if(NULL == m_hBitmap)
    {
        LCDUITRACE(_T("COGLObject::Initialize(): failed to create bitmap.\n"));
        Shutdown();
        return FALSE;
    }

    //select the newly created bitmap
    SelectBitmap();

    //OpenGL setup
    PIXELFORMATDESCRIPTOR pixelDesc= 
	{
		sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_BITMAP |
			PFD_SUPPORT_OPENGL,
			PFD_TYPE_RGBA,
			32,
			0,0,0,0,0,0,
			0,0,0,0,0,0,0,
			32,
			0,0,
			PFD_MAIN_PLANE,
			0,
			0,0,0
	};

    m_GLPixelIndex = ChoosePixelFormat(m_hDC, &pixelDesc);

    BOOL res = SetPixelFormat(m_hDC, m_GLPixelIndex, &pixelDesc);
    if( !res )
    {
        LCDUITRACE(_T("COGLObject::Initialize(): failed to set pixel format.\n"));
        Shutdown();
        return FALSE; 
    }

    DescribePixelFormat(m_hDC, m_GLPixelIndex, sizeof(pixelDesc), &pixelDesc);

	if(pixelDesc.dwFlags & PFD_NEED_PALETTE)
    {
		SetupLogicalPalette();
    }

	m_hRC = wglCreateContext(m_hDC);

    return TRUE;
}   

void COGLObject::Shutdown(void)
{
    UnselectBitmap();

    if(NULL != m_hBitmap)
    {
        DeleteObject(m_hBitmap);
        m_hBitmap = NULL;
        m_pBitmapBits = NULL;
    }

    LCDUIASSERT(NULL == m_hPrevBitmap);
    m_hPrevBitmap = NULL;
    
    if(NULL != m_pBitmapInfo)
    {
        delete [] m_pBitmapInfo;
        m_pBitmapInfo = NULL;
    }
    
    if(NULL != m_hDC)
    {
        DeleteDC(m_hDC);
        m_hDC = NULL;
    }

    if(NULL != m_hPalette)
    {
        DeleteObject(m_hPalette);
        m_hPalette = NULL;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(m_hRC);
}

void COGLObject::MakeCurrent()
{
    BOOL res = wglMakeCurrent(m_hDC, m_hRC);
    if(!res)
    {
        LCDUITRACE( _T("COGLObject::BeginDraw failed wglMakeCurrent().\n") );
    }
}



void COGLObject::SetupLogicalPalette(void)
{
    struct
    {
        WORD Version;
        WORD NumberOfEntries;
        PALETTEENTRY aEntries[256];
    }logicalPalette={0x300,256};

    BYTE reds[]={0,36,72,109,145,182,218,255};
    BYTE greens[]={0,36,72,109,145,182,218,255};
    BYTE blues[]={0,85,170,255};

    for(int colorNum=0; colorNum<256; ++colorNum)
    {
        logicalPalette.aEntries[colorNum].peRed= reds[colorNum&0x07];
        logicalPalette.aEntries[colorNum].peGreen=greens[(colorNum>>0x03) &0x07];
		logicalPalette.aEntries[colorNum].peBlue=blues[(colorNum>>0x06) &0x03];
		logicalPalette.aEntries[colorNum].peFlags=0;
	}

	m_hPalette = CreatePalette((LOGPALETTE*)&logicalPalette);
}

void COGLObject::BeginDraw(void)
{
    if(m_hPalette)
	{
		SelectPalette(m_hDC,m_hPalette,FALSE);
		RealizePalette(m_hDC);
	}

    BOOL res = wglMakeCurrent(m_hDC, m_hRC);
    if(!res)
    {
        LCDUITRACE( _T("COGLObject::BeginDraw failed wglMakeCurrent().\n") );
    }
}

void COGLObject::EndDraw(void)
{
    glFlush();
    SwapBuffers(m_hDC);
	wglMakeCurrent(m_hDC, NULL);
}

void COGLObject::SelectBitmap(void)
{
    //Select the bitmap
    LCDUIASSERT(NULL != m_hBitmap);
    LCDUIASSERT(NULL == m_hPrevBitmap);
    if(NULL == m_hPrevBitmap)
    {
        m_hPrevBitmap = (HBITMAP) SelectObject(m_hDC, m_hBitmap);
    }
}

void COGLObject::UnselectBitmap(void)
{
    //Unselect the bitmap
    LCDUIASSERT(NULL != m_hPrevBitmap);
    if(NULL != m_hPrevBitmap)
    {
        m_hPrevBitmap = (HBITMAP) SelectObject(m_hDC, m_hPrevBitmap);
        LCDUIASSERT(m_hPrevBitmap == m_hBitmap);
        m_hPrevBitmap = NULL;
    }
}

