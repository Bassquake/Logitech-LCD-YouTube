"""LCDYouTube - YouTube browser/player for the 320x240 LCD (SpacePilot Pro / G19)
via Logitech GamePanel LCD Manager. Replaces the dead Logitech LCDYouTube.exe
(GData v2 API + Flash player, both shut down).

Run with pythonw. Config: %APPDATA%\\LCDYouTube\\config.json
Log: %LOCALAPPDATA%\\LCDYouTube\\lcdyoutube.log
"""
import ctypes
import ctypes.wintypes as wt
import io
import json
import logging
import os
import re
import sys
import threading
import time
import urllib.request

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lcd as L  # noqa: E402
from player import Player, find_ffmpeg, list_feed  # noqa: E402

APP = 'LCDYouTube'
CFG_DIR = os.path.join(os.environ.get('APPDATA', '.'), APP)
LOG_DIR = os.path.join(os.environ.get('LOCALAPPDATA', '.'), APP)
CFG_PATH = os.path.join(CFG_DIR, 'config.json')

DEFAULT_CONFIG = {
    'volume': 0.8,
    'items_per_feed': 30,
    'clipboard': True,
    'feeds': [
        {'title': 'Music', 'url': 'ytsearch:music video'},
        {'title': 'Gaming', 'url': 'ytsearch:gaming'},
        {'title': 'News', 'url': 'ytsearch:news today'},
        {'title': 'Lofi Girl (uploads)', 'url': 'https://www.youtube.com/@LofiGirl/videos'},
        {'title': '3Dconnexion (uploads)', 'url': 'https://www.youtube.com/@3Dconnexion/videos'},
    ],
}

log = logging.getLogger('lcdyoutube')

# ---------------------------------------------------------------- drawing

W, H = L.WIDTH, L.HEIGHT
BG = (12, 12, 14)
PANEL = (32, 32, 38)
SEL = (190, 20, 20)
FG = (235, 235, 235)
DIM = (150, 150, 158)


def _font(name, size):
    try:
        return ImageFont.truetype(os.path.join(os.environ['WINDIR'], 'Fonts', name), size)
    except OSError:
        return ImageFont.load_default()


F_TITLE = _font('segoeuib.ttf', 14)
F_ITEM = _font('segoeui.ttf', 13)
F_ITEMB = _font('segoeuib.ttf', 13)
F_SMALL = _font('segoeui.ttf', 11)
F_BIG = _font('segoeuib.ttf', 20)

_STRIP = re.compile('[\U00010000-\U0010FFFF☀-➿️‍⬀-⯿]')


def clean(s):
    return re.sub(r'\s+', ' ', _STRIP.sub('', s or '')).strip()


def ellipsize(draw, text, font, width):
    if draw.textlength(text, font=font) <= width:
        return text
    while text and draw.textlength(text + '…', font=font) > width:
        text = text[:-1]
    return text.rstrip() + '…'


def wrap(draw, text, font, width, lines):
    words, out, cur = text.split(' '), [], ''
    for i, w in enumerate(words):
        t = (cur + ' ' + w).strip()
        if draw.textlength(t, font=font) <= width or not cur:
            cur = t
            continue
        out.append(cur)
        cur = w
        if len(out) == lines - 1:
            cur = ' '.join(words[i:])
            break
    if cur:
        out.append(cur)
    out = out[:lines]
    if out:
        out[-1] = ellipsize(draw, out[-1], font, width)
    return out


def fmt_time(s):
    if s is None:
        return 'LIVE'
    s = int(s)
    h, m, sec = s // 3600, s // 60 % 60, s % 60
    return f'{h}:{m:02d}:{sec:02d}' if h else f'{m}:{sec:02d}'


def fmt_views(v):
    if not v:
        return ''
    for div, suf in ((1e9, 'B'), (1e6, 'M'), (1e3, 'K')):
        if v >= div:
            return f'{v / div:.1f}'.rstrip('0').rstrip('.') + suf + ' views'
    return f'{v} views'


def to_bgra(img):
    return img.convert('RGBA').tobytes('raw', 'BGRA')


def header(d, title, right=''):
    d.rectangle((0, 0, W, 21), fill=SEL)
    d.text((6, 2), ellipsize(d, title, F_TITLE, W - 70), font=F_TITLE, fill=FG)
    if right:
        d.text((W - 6, 3), right, font=F_SMALL, fill=FG, anchor='ra')


def message(title, lines):
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    header(d, title)
    y = 60
    for i, ln in enumerate(lines):
        for part in wrap(d, ln, F_BIG if i == 0 else F_ITEM, W - 30, 4):
            d.text((W // 2, y), part, font=F_BIG if i == 0 else F_ITEM, fill=FG if i == 0 else DIM,
                   anchor='ma')
            y += 28 if i == 0 else 18
        y += 6
    return img


# ---------------------------------------------------------------- thumbnails

class Thumbs:
    def __init__(self, on_ready):
        self.cache = {}
        self.on_ready = on_ready
        self.lock = threading.Lock()

    def get(self, vid):
        with self.lock:
            if vid in self.cache:
                return self.cache[vid]
            self.cache[vid] = None
        threading.Thread(target=self._fetch, args=(vid,), daemon=True).start()
        return None

    def _fetch(self, vid):
        try:
            with urllib.request.urlopen(f'https://i.ytimg.com/vi/{vid}/mqdefault.jpg', timeout=10) as r:
                img = Image.open(io.BytesIO(r.read())).convert('RGB').resize((96, 54), Image.LANCZOS)
        except Exception:
            img = Image.new('RGB', (96, 54), PANEL)
        with self.lock:
            if len(self.cache) > 400:
                self.cache.clear()
            self.cache[vid] = img
        self.on_ready()


# ---------------------------------------------------------------- screens

class Menu:
    """Feed chooser."""

    def __init__(self, app):
        self.app = app
        self.sel = 0

    def items(self):
        feeds = [f['title'] for f in self.app.cfg['feeds']]
        return feeds + ['Recently played', 'Copied links']

    def key(self, k):
        n = len(self.items())
        if k == L.BTN_UP:
            self.sel = (self.sel - 1) % n
        elif k == L.BTN_DOWN:
            self.sel = (self.sel + 1) % n
        elif k in (L.BTN_OK, L.BTN_RIGHT):
            nf = len(self.app.cfg['feeds'])
            if self.sel < nf:
                f = self.app.cfg['feeds'][self.sel]
                self.app.push(VideoList(self.app, f['title'], url=f['url']))
            elif self.sel == nf:
                self.app.push(VideoList(self.app, 'Recently played', items=self.app.history))
            else:
                self.app.push(VideoList(self.app, 'Copied links', items=self.app.copied))
        elif k == L.BTN_MENU and self.app.player.state == 'playing':
            self.app.push(self.app.now_playing)
        self.app.dirty = True

    def render(self):
        img = Image.new('RGB', (W, H), BG)
        d = ImageDraw.Draw(img)
        header(d, 'YouTube', 'LCDYouTube')
        items = self.items()
        rows = 9
        top = max(0, min(self.sel - rows // 2, len(items) - rows))
        for i, t in enumerate(items[top:top + rows]):
            y = 24 + i * 24
            if top + i == self.sel:
                d.rectangle((0, y, W, y + 22), fill=PANEL)
                d.rectangle((0, y, 3, y + 22), fill=SEL)
            d.text((12, y + 3), ellipsize(d, t, F_ITEMB if top + i == self.sel else F_ITEM, W - 24),
                   font=F_ITEMB if top + i == self.sel else F_ITEM, fill=FG)
        if self.app.player.state == 'playing':
            d.text((W - 6, H - 4), 'MENU: now playing', font=F_SMALL, fill=DIM, anchor='rd')
        return img


class VideoList:
    def __init__(self, app, title, url=None, items=None):
        self.app, self.title, self.url = app, title, url
        self.sel = 0
        self.error = ''
        if url:
            self.items = None
            threading.Thread(target=self._load, daemon=True).start()
        else:
            self.items = list(items)

    def _load(self):
        try:
            self.items = list_feed(self.url, self.app.cfg.get('items_per_feed', 30))
        except Exception as e:
            log.exception('feed %s', self.url)
            self.error = str(e).replace('ERROR: ', '')[:200]
            self.items = []
        self.app.dirty = True

    def key(self, k):
        if k in (L.BTN_CANCEL, L.BTN_LEFT):
            self.app.pop()
            return
        if not self.items:
            return
        if k == L.BTN_UP:
            self.sel = (self.sel - 1) % len(self.items)
        elif k == L.BTN_DOWN:
            self.sel = (self.sel + 1) % len(self.items)
        elif k in (L.BTN_OK, L.BTN_RIGHT):
            self.app.play(self.items[self.sel])
        elif k == L.BTN_MENU and self.app.player.state == 'playing':
            self.app.push(self.app.now_playing)
        self.app.dirty = True

    def render(self):
        if self.items is None:
            return message(self.title, ['Loading…', 'Fetching video list from YouTube'])
        if not self.items:
            return message(self.title, ['No videos', self.error or 'This list is empty.'])
        img = Image.new('RGB', (W, H), BG)
        d = ImageDraw.Draw(img)
        header(d, self.title, f'{self.sel + 1}/{len(self.items)}')
        rows = 4
        top = max(0, min(self.sel - 1, len(self.items) - rows))
        for i, it in enumerate(self.items[top:top + rows]):
            y = 23 + i * 54
            selected = top + i == self.sel
            if selected:
                d.rectangle((0, y, W, y + 53), fill=PANEL)
                d.rectangle((0, y, 3, y + 53), fill=SEL)
            th = self.app.thumbs.get(it['id'])
            if th:
                img.paste(th.resize((88, 50)), (6, y + 2))
            else:
                d.rectangle((6, y + 2, 93, y + 51), fill=(45, 45, 52))
            dur = 'LIVE' if it.get('live') else (fmt_time(it['duration']) if it.get('duration') else '')
            if dur:
                tw = d.textlength(dur, font=F_SMALL)
                d.rectangle((92 - tw - 4, y + 37, 93, y + 51), fill=(0, 0, 0) if dur != 'LIVE' else SEL)
                d.text((91, y + 38), dur, font=F_SMALL, fill=FG, anchor='ra')
            font = F_ITEMB if selected else F_ITEM
            for j, ln in enumerate(wrap(d, clean(it['title']), font, W - 106, 2)):
                d.text((100, y + 1 + j * 16), ln, font=font, fill=FG)
            meta = ' · '.join(x for x in (clean(it.get('channel')), fmt_views(it.get('views'))) if x)
            d.text((100, y + 35), ellipsize(d, meta, F_SMALL, W - 106), font=F_SMALL, fill=DIM)
        return img


class NowPlaying:
    OVERLAY_SECS = 3.0

    def __init__(self, app):
        self.app = app
        self.overlay_until = 0
        self.pinned = False
        self.last = None
        self.next_static = 0.0
        self.next_overlay = 0.0

    def _static(self, make):
        now = time.monotonic()
        if now < self.next_static and not self.app.dirty:
            return None
        self.next_static, self.app.dirty = now + 0.3, False
        return make()

    def poke(self):
        self.overlay_until = time.monotonic() + self.OVERLAY_SECS
        self.next_overlay = 0.0

    def key(self, k):
        p = self.app.player
        if k == L.BTN_CANCEL:
            p.stop()
            self.app.pop()
            return
        if k == L.BTN_OK:
            p.toggle_pause()
        elif k == L.BTN_LEFT:
            p.seek(-15)
        elif k == L.BTN_RIGHT:
            p.seek(15)
        elif k == L.BTN_UP:
            p.volume = min(1.0, round(p.volume + 0.1, 2))
            self.app.cfg['volume'] = p.volume
        elif k == L.BTN_DOWN:
            p.volume = max(0.0, round(p.volume - 0.1, 2))
            self.app.cfg['volume'] = p.volume
        elif k == L.BTN_MENU:
            # Back to the browser while audio keeps playing.
            self.app.pop()
            return
        self.poke()

    def render(self):
        p = self.app.player
        info = p.info or {}
        title = clean(info.get('title')) or 'YouTube'
        if p.state == 'loading':
            return self._static(lambda: message(title, ['Loading…', 'Resolving video stream']))
        if p.state == 'error':
            return self._static(lambda: message('Playback error', ['Cannot play video', p.error]))
        if p.state == 'ended':
            self.app.pop()
            return None
        frame = p.frame()
        if frame is not None:
            self.last = frame
        overlay = self.pinned or p.paused or time.monotonic() < self.overlay_until
        if self.last is None:
            return self._static(lambda: message(title, ['Buffering…']))
        if not overlay:
            return frame  # bytes (or None if no new frame due)
        now = time.monotonic()
        if frame is None and now < self.next_overlay:
            return None
        self.next_overlay = now + 0.1
        img = Image.frombytes('RGBA', (W, H), self.last, 'raw', 'BGRA')
        d = ImageDraw.Draw(img, 'RGBA')
        d.rectangle((0, 0, W, 22), fill=(0, 0, 0, 170))
        d.text((6, 3), ellipsize(d, title, F_ITEMB, W - 12), font=F_ITEMB, fill=FG)
        d.rectangle((0, H - 34, W, H), fill=(0, 0, 0, 170))
        pos, dur = p.position(), info.get('duration')
        x0, x1, y = 8, W - 8, H - 28
        d.rectangle((x0, y, x1, y + 4), fill=(90, 90, 90, 255))
        if dur:
            d.rectangle((x0, y, x0 + int((x1 - x0) * min(1, pos / dur)), y + 4), fill=SEL)
        status = 'Paused' if p.paused else 'Playing'
        d.text((8, H - 19), f'{status}  {fmt_time(pos)} / {fmt_time(dur)}', font=F_SMALL, fill=FG)
        d.text((W - 8, H - 19), f'Vol {int(p.volume * 100)}%', font=F_SMALL, fill=FG, anchor='ra')
        if p.paused:
            d.rectangle((W // 2 - 14, H // 2 - 18, W // 2 - 6, H // 2 + 18), fill=(255, 255, 255, 200))
            d.rectangle((W // 2 + 6, H // 2 - 18, W // 2 + 14, H // 2 + 18), fill=(255, 255, 255, 200))
        return img


# ---------------------------------------------------------------- clipboard

YT_ID = re.compile(r'(?:youtube\.com/(?:watch\?(?:.*&)?v=|shorts/|live/|embed/)|youtu\.be/)'
                   r'([A-Za-z0-9_-]{11})')
YT_LIST = re.compile(r'youtube\.com/.*[?&]list=([A-Za-z0-9_-]+)')


class Clipboard:
    CF_UNICODETEXT = 13

    def __init__(self):
        u, k = ctypes.windll.user32, ctypes.windll.kernel32
        u.GetClipboardSequenceNumber.restype = wt.DWORD
        u.OpenClipboard.argtypes = [wt.HWND]
        u.GetClipboardData.restype = ctypes.c_void_p
        u.GetClipboardData.argtypes = [wt.UINT]
        k.GlobalLock.restype = ctypes.c_void_p
        k.GlobalLock.argtypes = [ctypes.c_void_p]
        k.GlobalUnlock.argtypes = [ctypes.c_void_p]
        self.u, self.k = u, k
        self.seq = u.GetClipboardSequenceNumber()

    def poll(self):
        seq = self.u.GetClipboardSequenceNumber()
        if seq == self.seq:
            return None
        self.seq = seq
        if not self.u.OpenClipboard(None):
            return None
        try:
            h = self.u.GetClipboardData(self.CF_UNICODETEXT)
            if not h:
                return None
            p = self.k.GlobalLock(h)
            try:
                return ctypes.wstring_at(p)[:2000] if p else None
            finally:
                self.k.GlobalUnlock(h)
        finally:
            self.u.CloseClipboard()


# ---------------------------------------------------------------- app

class App:
    def __init__(self):
        self.cfg = self._load_cfg()
        self.history = self.cfg.pop('history', [])
        self.copied = []
        self.player = Player(find_ffmpeg(), self.cfg.get('volume', 0.8))
        self.thumbs = Thumbs(self._mark_dirty)
        self.now_playing = NowPlaying(self)
        self.stack = [Menu(self)]
        self.dirty = True
        self.lcd = L.Lcd(APP)
        self.clip = Clipboard() if self.cfg.get('clipboard', True) else None

    def _mark_dirty(self):
        self.dirty = True

    def _load_cfg(self):
        try:
            with open(CFG_PATH, encoding='utf-8') as f:
                cfg = json.load(f)
        except FileNotFoundError:
            cfg = json.loads(json.dumps(DEFAULT_CONFIG))
            os.makedirs(CFG_DIR, exist_ok=True)
            with open(CFG_PATH, 'w', encoding='utf-8') as f:
                json.dump(cfg, f, indent=2)
        for k, v in DEFAULT_CONFIG.items():
            cfg.setdefault(k, v)
        return cfg

    def save_cfg(self):
        try:
            with open(CFG_PATH, encoding='utf-8') as f:
                on_disk = json.load(f)  # keep user's edits to feeds etc.
        except Exception:
            on_disk = dict(self.cfg)
        on_disk['volume'] = self.player.volume
        on_disk['history'] = self.history[:50]
        with open(CFG_PATH, 'w', encoding='utf-8') as f:
            json.dump(on_disk, f, indent=2, ensure_ascii=False)

    # navigation
    @property
    def top(self):
        return self.stack[-1]

    def push(self, screen):
        self.stack.append(screen)
        self.dirty = True

    def pop(self):
        if len(self.stack) > 1:
            self.stack.pop()
        self.dirty = True

    def play(self, item):
        self.history = [item] + [h for h in self.history if h['id'] != item['id']]
        self.history = self.history[:50]
        self.player.play(item['id'])
        self.now_playing.poke()
        if self.top is not self.now_playing:
            self.push(self.now_playing)
        self.save_cfg()

    def on_clipboard(self, text):
        text = text.strip()
        if text.lower().startswith('lcdyoutube:search='):
            q = text.split('=', 1)[1].strip()
            if q:
                self.stack = [self.stack[0], VideoList(self, f'Search: {q}', url=f'ytsearch:{q}')]
                self.lcd.foreground(True)
                self.dirty = True
            return
        m = YT_ID.search(text)
        if m:
            item = {'id': m.group(1), 'title': text if len(text) < 60 else m.group(1),
                    'channel': 'Copied link', 'duration': None, 'views': None}
            self.copied = [item] + [c for c in self.copied if c['id'] != item['id']]
            self.stack = self.stack[:1]
            self.play(item)
            self.lcd.foreground(True)
            return
        m = YT_LIST.search(text)
        if m:
            url = f'https://www.youtube.com/playlist?list={m.group(1)}'
            self.stack = [self.stack[0], VideoList(self, 'Playlist', url=url)]
            self.lcd.foreground(True)
            self.dirty = True

    def run(self):
        held, held_since, last_repeat = 0, 0.0, 0.0
        last_clip = 0.0
        prev_state = None
        while True:
            note = self.lcd.notification()
            if note == L.NOTIFY_TERMINATE:
                log.info('LCD Manager asked us to quit')
                break
            if note == L.NOTIFY_CLOSE_CONNECTION:
                log.info('LCD Manager closed the connection (restart?), reconnecting')
                self.lcd.reconnect()
                self.dirty = True
                continue
            if note == L.NOTIFY_APPLET_DISABLED:
                self.player.stop()

            b = self.lcd.buttons()
            now = time.monotonic()
            pressed = b & ~held
            if b != held:
                log.debug('buttons 0x%04x', b)
                held, held_since = b, now
            elif b & (L.BTN_UP | L.BTN_DOWN) and now - held_since > 0.45 and now - last_repeat > 0.12:
                pressed = b & (L.BTN_UP | L.BTN_DOWN)
                last_repeat = now
            for bit in L.BTN_NAMES:
                if pressed & bit:
                    self.top.key(bit)

            if self.clip and now - last_clip > 0.5:
                last_clip = now
                try:
                    text = self.clip.poll()
                    if text:
                        self.on_clipboard(text)
                except Exception:
                    log.exception('clipboard')

            if self.player.state != prev_state:
                prev_state = self.player.state
                self.dirty = True

            if self.top is self.now_playing:
                out = self.now_playing.render()
                if out is not None:
                    self.lcd.show(out if isinstance(out, bytes) else to_bgra(out))
                time.sleep(0.01)
                continue
            if self.dirty:
                self.dirty = False
                self.lcd.show(to_bgra(self.top.render()))
            elif not self.lcd.opened:
                self.dirty = True  # device came back: repaint
            time.sleep(0.03)


def single_instance():
    ctypes.windll.kernel32.CreateMutexW(None, False, 'LCDYouTube_SingleInstance')
    return ctypes.windll.kernel32.GetLastError() != 183  # ERROR_ALREADY_EXISTS


def main():
    os.makedirs(LOG_DIR, exist_ok=True)
    logging.basicConfig(filename=os.path.join(LOG_DIR, 'lcdyoutube.log'), level=logging.INFO,
                        format='%(asctime)s %(levelname)s %(name)s: %(message)s')
    if '--debug' in sys.argv:
        logging.getLogger().setLevel(logging.DEBUG)
        logging.getLogger().addHandler(logging.StreamHandler())
    if not single_instance():
        return
    app = App()
    log.info('started, ffmpeg=%s', app.player.ffmpeg)
    try:
        app.run()
    except KeyboardInterrupt:
        pass
    except Exception:
        log.exception('crashed')
        raise
    finally:
        app.player.stop()
        try:
            app.save_cfg()
        except Exception:
            pass
        app.lcd.close()


if __name__ == '__main__':
    main()
