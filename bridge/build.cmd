@echo off
rem Builds lglcd_bridge.dll (x64) from the LCD SDK that ships with LCD Manager.
setlocal
set HERE=%~dp0
set SDKZIP=C:\Program Files\Logitech\GamePanel Software\LCD Manager\SDK\LCDSDK_3.06.109.zip
set SDK=%HERE%sdk\LCDSDK

if not exist "%SDK%\Src\lglcd.h" (
    powershell -NoProfile -Command "Expand-Archive -Force '%SDKZIP%' '%HERE%sdk'" || exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%%VSWHERE%%" -latest -products * -property installationPath`) do set "VS=%%i"
call "%VS%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul || exit /b 1

pushd "%HERE%"
cl /nologo /O2 /W3 /LD /I"%SDK%\Src" lglcd_bridge.c /Fe:..\lglcd_bridge.dll ^
   /link "%SDK%\Lib\x64\lglcd.lib" user32.lib advapi32.lib ole32.lib || (popd & exit /b 1)
rem Icon: ..\icon-youtube.png -> lcdtube.ico (16..256 px)
python -c "from PIL import Image; Image.open(r'..\icon-youtube.png').convert('RGBA').save('lcdtube.ico', sizes=[(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 128, 256)])" || (popd & exit /b 1)
rc /nologo /fo launcher.res launcher.rc || (popd & exit /b 1)
cl /nologo /O2 /W3 /DUNICODE /D_UNICODE launcher.c launcher.res /Fe:..\LCDYouTube.exe ^
   /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib || (popd & exit /b 1)
del /q lglcd_bridge.obj launcher.obj launcher.res ..\lglcd_bridge.exp ..\lglcd_bridge.lib 2>nul
popd
echo Built %HERE%..\lglcd_bridge.dll and %HERE%..\LCDYouTube.exe
