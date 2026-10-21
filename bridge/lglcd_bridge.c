/*
 * lglcd_bridge.dll - flat C exports over the Logitech LCD SDK's static
 * lglcd.lib (SDK 3.06, shipped with GamePanel LCD Manager), so Python can
 * drive the 320x240 colour LCD via ctypes.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "lglcd.h"

#define EXPORT __declspec(dllexport)

static int g_connection = LGLCD_INVALID_CONNECTION;
static int g_device = LGLCD_INVALID_DEVICE;
static volatile LONG g_buttons;
static volatile LONG g_notify;      /* last notification code */
static lgLcdBitmap g_bmp;

static DWORD WINAPI OnButtons(int device, DWORD buttons, const PVOID ctx)
{
    (void)device; (void)ctx;
    InterlockedExchange(&g_buttons, (LONG)buttons);
    return 0;
}

static DWORD WINAPI OnNotify(int connection, const PVOID ctx, DWORD code,
                             DWORD p1, DWORD p2, DWORD p3, DWORD p4)
{
    (void)connection; (void)ctx; (void)p1; (void)p2; (void)p3; (void)p4;
    InterlockedExchange(&g_notify, (LONG)code);
    return 0;
}

/* Initialise the library and register the applet with LCD Manager. */
EXPORT DWORD lcd_connect(const wchar_t *friendlyName, BOOL autostartable)
{
    lgLcdConnectContextExW ctx;
    DWORD rc = lgLcdInit();
    if (rc != ERROR_SUCCESS && rc != ERROR_ALREADY_INITIALIZED)
        return rc;

    ZeroMemory(&ctx, sizeof(ctx));
    ctx.appFriendlyName = friendlyName;
    ctx.isPersistent = TRUE;     /* stay listed in LCD Manager when not running */
    ctx.isAutostartable = autostartable;
    ctx.connection = LGLCD_INVALID_CONNECTION;
    ctx.dwAppletCapabilitiesSupported = LGLCD_APPLET_CAP_QVGA;
    ctx.onNotify.notificationCallback = OnNotify;
    rc = lgLcdConnectExW(&ctx);
    if (rc == ERROR_SUCCESS)
        g_connection = ctx.connection;
    return rc;
}

/* Open the colour (QVGA) device; fails while no device is attached. */
EXPORT DWORD lcd_open(void)
{
    lgLcdOpenByTypeContext ctx;
    DWORD rc;
    if (g_device != LGLCD_INVALID_DEVICE)
        return ERROR_SUCCESS;
    ZeroMemory(&ctx, sizeof(ctx));
    ctx.connection = g_connection;
    ctx.deviceType = LGLCD_DEVICE_QVGA;
    ctx.onSoftbuttonsChanged.softbuttonsChangedCallback = OnButtons;
    ctx.device = LGLCD_INVALID_DEVICE;
    rc = lgLcdOpenByType(&ctx);
    if (rc == ERROR_SUCCESS)
        g_device = ctx.device;
    return rc;
}

/* Push one 320x240 BGRA frame (307200 bytes). */
EXPORT DWORD lcd_update(const BYTE *bgra, DWORD priority)
{
    DWORD rc;
    if (g_device == LGLCD_INVALID_DEVICE)
        return ERROR_DEVICE_NOT_CONNECTED;
    g_bmp.hdr.Format = LGLCD_BMP_FORMAT_QVGAx32;
    CopyMemory(g_bmp.bmp_qvga32.pixels, bgra, sizeof(g_bmp.bmp_qvga32.pixels));
    rc = lgLcdUpdateBitmap(g_device, &g_bmp.hdr, LGLCD_ASYNC_UPDATE(priority));
    if (rc == ERROR_DEVICE_NOT_CONNECTED) {
        lgLcdClose(g_device);
        g_device = LGLCD_INVALID_DEVICE;
    }
    return rc;
}

/* Current soft-button bitmask (LGLCDBUTTON_*), as last reported. */
EXPORT DWORD lcd_buttons(void)
{
    return (DWORD)g_buttons;
}

/* Last notification code received (LGLCD_NOTIFICATION_*), then clear it. */
EXPORT DWORD lcd_take_notification(void)
{
    return (DWORD)InterlockedExchange(&g_notify, 0);
}

EXPORT DWORD lcd_set_foreground(BOOL yes)
{
    if (g_device == LGLCD_INVALID_DEVICE)
        return ERROR_DEVICE_NOT_CONNECTED;
    return lgLcdSetAsLCDForegroundApp(g_device, yes ? LGLCD_LCD_FOREGROUND_APP_YES
                                                    : LGLCD_LCD_FOREGROUND_APP_NO);
}

EXPORT void lcd_close(void)
{
    if (g_device != LGLCD_INVALID_DEVICE)
        lgLcdClose(g_device);
    if (g_connection != LGLCD_INVALID_CONNECTION)
        lgLcdDisconnect(g_connection);
    g_device = LGLCD_INVALID_DEVICE;
    g_connection = LGLCD_INVALID_CONNECTION;
    lgLcdDeInit();
}
