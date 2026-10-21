/*
 * LCDYouTube.exe (LCDTube launcher) - hosts Python in-process and runs lcdtube.pyw from the same
 * folder. LCD Manager records the connecting process's exe as the applet, so
 * this gives it something it can list and autostart (pythonw.exe can't be).
 *
 * Settings come from LCDTube.ini next to the exe, or failing that
 * %APPDATA%\LCDTube\LCDTube.ini (so the exe can be copied anywhere,
 * e.g. into LCD Manager's Applets folder):
 *     [python]
 *     home=C:\path\to\python
 *     script=C:\path\to\lcdtube\lcdtube.pyw   (default: next to the exe)
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

typedef int (*Py_MainFn)(int, wchar_t **);

static void fail(const wchar_t *msg)
{
    MessageBoxW(NULL, msg, L"LCDTube", MB_ICONERROR);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int show)
{
    wchar_t dir[MAX_PATH], ini[MAX_PATH], script[MAX_PATH], home[MAX_PATH];
    wchar_t dll[MAX_PATH], path[32768], msg[1024], *slash;
    wchar_t **argv, **pyargv;
    int argc, i, rc;
    HMODULE py = NULL;
    Py_MainFn Py_Main;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    (void)hInst; (void)hPrev; (void)cmdLine; (void)show;

    GetModuleFileNameW(NULL, dir, MAX_PATH);
    slash = wcsrchr(dir, L'\\');
    if (slash) slash[1] = 0;
    swprintf(ini, MAX_PATH, L"%sLCDTube.ini", dir);
    if (GetFileAttributesW(ini) == INVALID_FILE_ATTRIBUTES) {
        wchar_t appdata[MAX_PATH];
        if (GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH))
            swprintf(ini, MAX_PATH, L"%s\\LCDTube\\LCDTube.ini", appdata);
    }
    swprintf(script, MAX_PATH, L"%slcdtube.pyw", dir);
    GetPrivateProfileStringW(L"python", L"script", script, script, MAX_PATH, ini);

    if (!GetPrivateProfileStringW(L"python", L"home", L"", home, MAX_PATH, ini) || !home[0]) {
        swprintf(msg, 1024, L"Missing [python] home= in\n%s\n\nRun install.cmd.", ini);
        fail(msg);
        return 1;
    }
    if (GetFileAttributesW(script) == INVALID_FILE_ATTRIBUTES) {
        swprintf(msg, 1024, L"Cannot find\n%s\n\nSet [python] script= in\n%s", script, ini);
        fail(msg);
        return 1;
    }

    /* Find python3XX.dll in the home folder. */
    swprintf(dll, MAX_PATH, L"%s\\python3*.dll", home);
    h = FindFirstFileW(dll, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (_wcsicmp(fd.cFileName, L"python3.dll") != 0) {
                swprintf(dll, MAX_PATH, L"%s\\%s", home, fd.cFileName);
                break;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    /* Let Python find its stdlib and the DLLs a conda/regular install needs. */
    SetEnvironmentVariableW(L"PYTHONHOME", home);
    rc = GetEnvironmentVariableW(L"PATH", path, 32768);
    swprintf(msg, 1024, L"%s;%s\\Library\\bin;%s\\DLLs;%s\\Scripts;", home, home, home, home);
    {
        static wchar_t newpath[32768 + 1024];
        swprintf(newpath, 32768 + 1024, L"%s%s", msg, rc ? path : L"");
        SetEnvironmentVariableW(L"PATH", newpath);
    }

    py = LoadLibraryExW(dll, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    Py_Main = py ? (Py_MainFn)GetProcAddress(py, "Py_Main") : NULL;
    if (!Py_Main) {
        swprintf(msg, 1024, L"Cannot load Python from\n%s", home);
        fail(msg);
        return 1;
    }

    /* argv: LCDYouTube.exe lcdtube.pyw [extra args...] */
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    pyargv = (wchar_t **)LocalAlloc(LPTR, sizeof(wchar_t *) * (argc + 2));
    pyargv[0] = argv[0];
    pyargv[1] = script;
    for (i = 1; i < argc; i++)
        pyargv[i + 1] = argv[i];
    return Py_Main(argc + 1, pyargv);
}
