# LCDYouTube

A replacement for Logitech's **LCD Video Player for YouTube** (`LCDYouTube.exe`) on 320x240 colour
LCDs driven by GamePanel LCD Manager for 3DConnexion or Logitech devices such as SpacePilot Pro or G19.

## Why the original stopped working

Analysis of the original `LCDYouTube.exe` (build 2021.2.2, x86 MFC; kept here as `LCDYouTube.exe.bak`):

* **Video lists** come from the YouTube Data API **v2** (`http://gdata.youtube.com/feeds/api/...`:
  standardfeeds, `videos?q=`, `users/%s/uploads`), parsed as GData Atom XML
  (`entry`, `media:group`, `yt:videoid`, `media:thumbnail`, `yt:format`) with a Logitech
  `X-GData-Key`. Google retired v2 in 2015; the endpoint now answers **HTTP 404**, so every list is
  empty. It's plain HTTP, so TLS/certificates aren't involved.
* **Playback** hosts the **Flash ActiveX** control running YouTube's AS3 `apiplayer` SWF and drives it
  with `<invoke name=...>` calls. Flash is gone from Windows and the SWF player no longer exists.

Both back ends are gone, so patching the exe can't bring playback back. LCDYouTube replaces it.

## How it works

* `bridge/lglcd_bridge.c` → `lglcd_bridge.dll`: flat C exports over the static `lglcd.lib` from the
  LCD SDK that ships with LCD Manager (`LCD Manager\SDK\LCDSDK_3.06.109.zip`). Rebuild with
  `bridge\build.cmd` (needs Visual Studio).
* `player.py`: yt-dlp lists searches/channels/playlists and resolves streams (≤360p H.264 + AAC).
  One ffmpeg process decodes video straight to 320x240 BGRA frames and another decodes audio to PCM,
  which plays through `sounddevice`. Audio is the clock, so video frames follow it.
* `lcdyoutube.pyw`: the LCD UI (feed menu, video list with thumbnails, player) and a clipboard watcher.

## Requirements

```
pip install "yt-dlp[default]" deno sounddevice pillow numpy imageio-ffmpeg
```

ffmpeg is used from PATH if present, otherwise from `imageio-ffmpeg`. `deno` is the JavaScript runtime
yt-dlp needs for some YouTube formats. Update yt-dlp regularly (`pip install -U yt-dlp`) because
YouTube changes often.

## Use

1. Run `install.cmd`. It installs the Python packages and writes `%APPDATA%\LCDYouTube\LCDYouTube.ini`
   (`home=` Python install, `script=` path to `lcdyoutube.pyw`). Because of that file, `LCDYouTube.exe`
   (the launcher) works from any folder.
2. Copy `LCDYouTube.exe` into `C:\Program Files\Logitech\GamePanel Software\LCD Manager\Applets`.
   LCD Manager launches every exe in that folder when it starts (at login), as it does for the stock
   applets. LCDYouTube sits idle until you select it on the LCD.
   This replaces Logitech's dead `LCDYouTube.exe` of the same name.

| Button | Menu / list          | Playing                         |
|--------|----------------------|---------------------------------|
| Up/Down| move (hold repeats)  | volume ±10%                     |
| OK     | open / play          | pause / resume                  |
| Right  | open / play          | seek +15 s                      |
| Left   | back                 | seek −15 s                      |
| Cancel | back                 | stop                            |
| Menu   | go to now-playing    | back to browser (keeps playing) |

**Clipboard** (like the original): copy a YouTube video link and it plays. Copy a playlist link and it
opens as a list. Copy `lcdyoutube:search=your words` to search (same command as the original applet).

## Config

`%APPDATA%\LCDYouTube\config.json` holds `feeds` (title + URL), `volume`, `items_per_feed`,
`clipboard`, and the play history. A feed URL can be `ytsearch:words`, a channel
`https://www.youtube.com/@name/videos`, or a playlist URL. Trending isn't included because YouTube
removed that page. The log is in `%LOCALAPPDATA%\LCDYouTube\lcdyoutube.log`. Run with `--debug` to log
button codes.
