@echo off
rem Installs dependencies and writes %APPDATA%\LCDTube\LCDTube.ini, which tells LCDYouTube.exe
rem (wherever it is copied, e.g. LCD Manager's Applets folder) where Python and the script are.
setlocal
cd /d "%~dp0"
for /f "usebackq delims=" %%h in (`python -c "import sys; print(sys.base_prefix)"`) do set "PYHOME=%%h"
if not defined PYHOME (echo python not found on PATH & exit /b 1)
python -m pip install -U "yt-dlp[default]" deno sounddevice pillow numpy imageio-ffmpeg || exit /b 1
if not exist "%APPDATA%\LCDTube" mkdir "%APPDATA%\LCDTube"
(echo [python]& echo home=%PYHOME%& echo script=%~dp0lcdtube.pyw)> "%APPDATA%\LCDTube\LCDTube.ini"
echo Wrote %APPDATA%\LCDTube\LCDTube.ini
type "%APPDATA%\LCDTube\LCDTube.ini"
