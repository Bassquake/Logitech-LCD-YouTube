"""YouTube lookup (yt-dlp) and playback (ffmpeg -> LCD frames + sounddevice audio)."""
import logging
import queue
import shutil
import subprocess
import threading
import time

import numpy as np

from lcd import WIDTH, HEIGHT, FRAME_BYTES

log = logging.getLogger('lcdtube.player')

FPS = 25
RATE = 48000
CHANNELS = 2
NO_WINDOW = 0x08000000  # CREATE_NO_WINDOW, so pythonw doesn't flash consoles

VIDEO_FORMAT = ('bv*[height<=360][vcodec^=avc1]+ba[ext=m4a]/'
                'bv*[height<=360]+ba/b[height<=480]/b')


def find_ffmpeg():
    exe = shutil.which('ffmpeg')
    if exe:
        return exe
    import imageio_ffmpeg
    return imageio_ffmpeg.get_ffmpeg_exe()


def _ydl_params(**extra):
    params = {'quiet': True, 'no_warnings': True, 'noprogress': True}
    try:
        import deno
        params['js_runtimes'] = {'deno': {'path': deno.find_deno_bin()}}
    except Exception:
        pass
    params.update(extra)
    return params


def list_feed(url, count=30):
    """Flat-list a search ("ytsearchN:q"), channel, or playlist URL."""
    import yt_dlp
    if url.startswith('ytsearch:'):
        url = f'ytsearch{count}:' + url[len('ytsearch:'):]
    with yt_dlp.YoutubeDL(_ydl_params(extract_flat='in_playlist', playlistend=count)) as ydl:
        info = ydl.extract_info(url, download=False)
    out = []
    for e in info.get('entries') or []:
        if not e or not e.get('id') or e.get('ie_key') not in (None, 'Youtube'):
            continue
        out.append({
            'id': e['id'],
            'title': e.get('title') or e['id'],
            'channel': e.get('channel') or e.get('uploader') or '',
            'duration': e.get('duration'),
            'views': e.get('view_count'),
            'live': e.get('live_status') == 'is_live',
        })
    return out


def resolve(video_id):
    """Return stream URLs for one video."""
    import yt_dlp
    with yt_dlp.YoutubeDL(_ydl_params(format=VIDEO_FORMAT)) as ydl:
        info = ydl.extract_info(f'https://www.youtube.com/watch?v={video_id}', download=False)
    fmts = info.get('requested_formats') or [info]
    video = next((f for f in fmts if f.get('vcodec') not in (None, 'none')), fmts[0])
    audio = next((f for f in fmts if f.get('acodec') not in (None, 'none')), None)
    return {
        'id': video_id,
        'title': info.get('title') or video_id,
        'channel': info.get('channel') or info.get('uploader') or '',
        'duration': info.get('duration'),
        'live': info.get('is_live') or False,
        'video': (video['url'], video.get('http_headers') or {}),
        'audio': (audio['url'], audio.get('http_headers') or {}) if audio else None,
    }


class Player:
    """Plays one resolved stream. Audio drives the clock; video frames follow it."""

    def __init__(self, ffmpeg, volume=0.8):
        self.ffmpeg = ffmpeg
        self.volume = volume
        self.paused = False
        self.info = None
        self.state = 'idle'       # idle | loading | playing | ended | error
        self.error = ''
        self._gen = 0
        self._procs = []
        self._stream = None

    # ---- public -------------------------------------------------------

    def play(self, video_id):
        self.stop()
        self.state = 'loading'
        self.paused = False
        self.info = {'id': video_id, 'title': '', 'channel': '', 'duration': None}
        gen = self._gen
        threading.Thread(target=self._resolve_then_start, args=(gen, video_id), daemon=True).start()

    def stop(self):
        self._gen += 1
        self._kill()
        self.state = 'idle'

    def toggle_pause(self):
        if self.state == 'playing':
            self.paused = not self.paused
            if not self._audio_ok:
                if self.paused:
                    self._wall_paused_at = time.monotonic()
                else:
                    self._wall_start += time.monotonic() - self._wall_paused_at

    def seek(self, delta):
        if self.state != 'playing' or not self.info or self.info.get('live'):
            return
        t = max(0.0, self.position() + delta)
        dur = self.info.get('duration')
        if dur:
            t = min(t, max(0.0, dur - 2))
        self._start(self._gen, t)

    def position(self):
        if self.state != 'playing':
            return getattr(self, '_base', 0.0)
        if self._audio_ok:
            return self._base + self._played / RATE
        now = self._wall_paused_at if self.paused else time.monotonic()
        return self._base + max(0.0, now - self._wall_start)

    def frame(self):
        """Latest frame due at the current clock (bytes, BGRA) or None if unchanged."""
        if self.state != 'playing':
            return None
        clock = self.position()
        out = None
        while True:
            if self._pending is None:
                try:
                    self._pending = self._frames.get_nowait()
                except queue.Empty:
                    break
            ts, data = self._pending
            if data is None:  # video EOF marker
                self._video_eof = True
                self._pending = None
                break
            if ts > clock:
                break
            out = data
            self._pending = None
        if self._video_eof and self._pending is None and self._frames.empty():
            if not self._audio_ok or self._audio_eof:
                self._finish(clock)
        return out

    # ---- internals ----------------------------------------------------

    def _resolve_then_start(self, gen, video_id):
        try:
            info = resolve(video_id)
        except Exception as e:
            if gen == self._gen:
                log.exception('resolve failed')
                self.error = str(e).replace('ERROR: ', '')[:200]
                self.state = 'error'
            return
        if gen != self._gen:
            return
        self.info = info
        self._start(gen, 0.0)

    def _finish(self, clock):
        dur = self.info.get('duration')
        # Stream dropped early (e.g. paused so long the connection timed out): resume.
        if dur and clock < dur - 5 and self._retries < 3:
            log.info('stream ended early at %.1fs of %ss, reconnecting', clock, dur)
            self._retries += 1
            retries = self._retries
            self._start(self._gen, clock)
            self._retries = retries
            return
        self._kill()
        self.state = 'ended'

    def _input_args(self, url, headers, start):
        args = []
        if start > 0:
            args += ['-ss', f'{start:.2f}']
        hdr = ''.join(f'{k}: {v}\r\n' for k, v in headers.items() if k.lower() != 'user-agent')
        if 'User-Agent' in headers:
            args += ['-user_agent', headers['User-Agent']]
        if hdr:
            args += ['-headers', hdr]
        if url.startswith('http') and '.m3u8' not in url:
            args += ['-reconnect', '1', '-reconnect_streamed', '1', '-reconnect_delay_max', '5']
        return args + ['-i', url]

    def _spawn(self, args):
        p = subprocess.Popen([self.ffmpeg, '-hide_banner', '-loglevel', 'error', '-nostdin'] + args,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             creationflags=NO_WINDOW, bufsize=0)
        threading.Thread(target=self._log_stderr, args=(p,), daemon=True).start()
        self._procs.append(p)
        return p

    @staticmethod
    def _log_stderr(p):
        for line in p.stderr:
            log.warning('ffmpeg: %s', line.decode('utf-8', 'replace').rstrip())

    def _start(self, gen, start):
        self._kill()
        self._gen += 1
        gen = self._gen
        self._base = start
        self._played = 0
        self._frames = queue.Queue(maxsize=FPS)       # ~1 s of decoded video
        self._audio = queue.Queue(maxsize=96)         # ~2 s of PCM
        self._pending = None
        self._leftover = b''
        self._video_eof = False
        self._audio_eof = False
        self._retries = 0
        info = self.info
        vurl, vhdr = info['video']
        vf = (f'fps={FPS},scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease,'
              f'pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2')
        vp = self._spawn(self._input_args(vurl, vhdr, start) +
                         ['-an', '-vf', vf, '-pix_fmt', 'bgra', '-f', 'rawvideo', '-'])
        threading.Thread(target=self._read_video, args=(gen, vp, start), daemon=True).start()

        self._audio_ok = False
        aurl, ahdr = info['audio'] or info['video']
        try:
            import sounddevice as sd
            ap = self._spawn(self._input_args(aurl, ahdr, start) +
                             ['-vn', '-ac', str(CHANNELS), '-ar', str(RATE), '-f', 's16le', '-'])
            threading.Thread(target=self._read_audio, args=(gen, ap), daemon=True).start()
            self._stream = sd.RawOutputStream(samplerate=RATE, channels=CHANNELS, dtype='int16',
                                              blocksize=1024, callback=self._audio_cb)
            self._stream.start()
            self._audio_ok = True
        except Exception:
            log.exception('audio unavailable, using wall clock')
            self._wall_start = time.monotonic()
            self._wall_paused_at = self._wall_start
        self.state = 'playing'
        log.info('playing %s from %.1fs', info['id'], start)

    def _read_video(self, gen, p, start):
        n = 0
        while gen == self._gen:
            data = p.stdout.read(FRAME_BYTES)
            while data and len(data) < FRAME_BYTES:
                more = p.stdout.read(FRAME_BYTES - len(data))
                if not more:
                    break
                data += more
            if not data or len(data) < FRAME_BYTES:
                break
            self._put(gen, self._frames, (start + n / FPS, data))
            n += 1
        self._put(gen, self._frames, (0, None))

    def _read_audio(self, gen, p):
        while gen == self._gen:
            data = p.stdout.read(4096)
            if not data:
                break
            self._put(gen, self._audio, data)
        self._put(gen, self._audio, None)

    def _put(self, gen, q, item):
        while gen == self._gen:
            try:
                q.put(item, timeout=0.2)
                return
            except queue.Full:
                pass

    def _audio_cb(self, outdata, frames, time_info, status):
        need = frames * CHANNELS * 2
        if self.paused or self._audio_eof:
            outdata[:] = b'\0' * need
            return
        buf = self._leftover
        while len(buf) < need:
            try:
                chunk = self._audio.get_nowait()
            except queue.Empty:
                break
            if chunk is None:
                self._audio_eof = True
                break
            buf += chunk
        if len(buf) < need:  # underrun: wait for more data, don't advance the clock
            self._leftover = buf
            outdata[:] = b'\0' * need
            return
        pcm, self._leftover = buf[:need], buf[need:]
        if self.volume < 0.999:
            pcm = (np.frombuffer(pcm, np.int16) * self.volume).astype(np.int16).tobytes()
        outdata[:] = pcm
        self._played += frames

    def _kill(self):
        if self._stream is not None:
            try:
                self._stream.abort()
                self._stream.close()
            except Exception:
                pass
            self._stream = None
        for p in self._procs:
            try:
                p.kill()
            except Exception:
                pass
        self._procs = []
