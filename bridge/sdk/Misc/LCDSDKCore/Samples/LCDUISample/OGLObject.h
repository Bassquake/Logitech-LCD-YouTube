//************************************************************************
//  The Logitech LCD SDK, including all acompanying documentation,
//  is protected by intellectual property laws.  All use of the Logitech
//  LCD SDK is subject to the License Agreement found in the
//  "Logitech LCD SDK License Agreement" file and in the Reference Manual.  
//  All rights not expressly granted by Logitech are reserved.
//************************************************************************

#ifndef __OGLOBJECT_H__
#define __OGLOBJECT_H__

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#include <math.h>
#include <GL\gl.h>
#include <GL\glu.h>

#include "LCDUI\LCDBase.h"

class COGLObject : public CLCDBase
{
public:
    COGLObject();
    virtual ~COGLObject();

    //CLCDBase
    //These are the two functions you need to override from the base class
    virtual void OnDraw(CLCDGfxBase &rGfx);
    virtual void OnUpdate(DWORD timestamp);

    //OpenGL
    BOOL Initialize(int width, int height);
    void Shutdown(void);
    void MakeCurrent();

    void SetupLogicalPalette(void);
    void BeginDraw(void);
    void EndDraw(void);


private:
    void SelectBitmap(void);
    void UnselectBitmap(void);
    
    BITMAPINFO *m_pBitmapInfo;
    HDC m_hDC;
    HBITMAP m_hBitmap;
    HBITMAP m_hPrevBitmap;
    PBYTE m_pBitmapBits;

    //OpenGL variables
    HGLRC m_hRC;
	HPALETTE m_hPalette;
    int m_GLPixelIndex;
};

#endif