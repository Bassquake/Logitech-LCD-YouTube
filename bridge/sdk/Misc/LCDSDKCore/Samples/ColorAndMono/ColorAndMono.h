//************************************************************************
//  The Logitech LCD SDK, including all acompanying documentation,
//  is protected by intellectual property laws.  All use of the Logitech
//  LCD SDK is subject to the License Agreement found in the
//  "Logitech LCD SDK License Agreement" file and in the Reference Manual.  
//  All rights not expressly granted by Logitech are reserved.
//************************************************************************

#pragma once

// LCD device state
typedef struct LCD_DEVICE_STATE
{
    int nDeviceId;
    DWORD dwButtonState;

}LCD_DEVICE_STATE;

// Applet state
typedef struct APPLET_STATE
{
    LCD_DEVICE_STATE Color;
    LCD_DEVICE_STATE Mono;
    BOOL isEnabled;

}APPLET_STATE;
