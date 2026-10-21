"""ctypes wrapper around lglcd_bridge.dll (see bridge/)."""
import ctypes
import logging
import os
import time

log = logging.getLogger('lcdtube.lcd')

WIDTH, HEIGHT = 320, 240
FRAME_BYTES = WIDTH * HEIGHT * 4

BTN_LEFT = 0x0100
BTN_RIGHT = 0x0200
BTN_OK = 0x0400
BTN_CANCEL = 0x0800
BTN_UP = 0x1000
BTN_DOWN = 0x2000
BTN_MENU = 0x4000
BTN_NAMES = {BTN_LEFT: 'left', BTN_RIGHT: 'right', BTN_OK: 'ok', BTN_CANCEL: 'cancel',
             BTN_UP: 'up', BTN_DOWN: 'down', BTN_MENU: 'menu'}

NOTIFY_APPLET_DISABLED = 4
NOTIFY_APPLET_ENABLED = 5
NOTIFY_TERMINATE = 6
NOTIFY_CLOSE_CONNECTION = 3

PRIORITY_NORMAL = 128


class Lcd:
    def __init__(self, name):
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lglcd_bridge.dll')
        self._dll = d = ctypes.WinDLL(path)
        d.lcd_connect.argtypes = [ctypes.c_wchar_p, ctypes.c_int]
        d.lcd_update.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
        for f in (d.lcd_connect, d.lcd_open, d.lcd_update, d.lcd_buttons,
                  d.lcd_take_notification, d.lcd_set_foreground):
            f.restype = ctypes.c_uint32
        self._name = ctypes.c_wchar_p(name)  # LCD Manager may keep the pointer
        self.opened = False
        self.connect()

    def connect(self):
        """Register with LCD Manager, waiting for it if it isn't running (yet)."""
        while True:
            rc = self._dll.lcd_connect(self._name, 1)  # autostartable, like the stock applets
            if rc == 0:
                self.opened = False
                return
            log.info('lgLcdConnectEx failed (%d), LCD Manager not running? retrying', rc)
            self._dll.lcd_close()
            time.sleep(3)

    def reconnect(self):
        self._dll.lcd_close()
        time.sleep(2)  # give a restarting LCDMon time to come up
        self.connect()

    def try_open(self):
        if not self.opened:
            self.opened = self._dll.lcd_open() == 0
        return self.opened

    def show(self, bgra: bytes):
        if not self.try_open():
            return False
        rc = self._dll.lcd_update(bgra, PRIORITY_NORMAL)
        if rc:
            self.opened = False
        return rc == 0

    def buttons(self):
        return self._dll.lcd_buttons()

    def notification(self):
        return self._dll.lcd_take_notification()

    def foreground(self, yes):
        if self.try_open():
            self._dll.lcd_set_foreground(1 if yes else 0)

    def close(self):
        self._dll.lcd_close()
