// emurun: runs programs in the Device Emulator (MAIN/WinCE_Emulator...) on a command file, so they can be started
// from the PC without clicks in its window (it takes none from PostMessage). Started once: by hand or as the shared
// folder's \StaticStore\2577\autorun.exe (CE runs it when the card appears). Every 300 ms it looks for
// \StaticStore\emurun\cmd.txt (ANSI lines), renames it to cmd.run, carries it out and writes done.txt:
//   run <exe> [args]    start, do not wait
//   wait <exe> [args]   start and wait (at most 15 min), the exit code in done.txt
//   kill                end the programs it started
//   close <title>       WM_CLOSE to the window of that title (a program started by another, e.g. GpsSim's OSMapsCE)
//   sleep <ms>
//   quit                end emurun
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const wchar_t kDir[] = L"\\StaticStore\\emurun\\";
static HANDLE s_started[32];
static int s_nStarted;

static void Path(wchar_t* out, const wchar_t* name)
{
    wcscpy(out, kDir);
    wcscat(out, name);
}

static void Note(FILE* f, const char* fmt, const wchar_t* w, DWORD v)
{
    char a[600];
    WideCharToMultiByte(CP_ACP, 0, w, -1, a, sizeof(a), NULL, NULL);
    fprintf(f, fmt, a, v);
    fflush(f);
}

// "exe args" -> exe and the rest; the exe may be quoted
static void Split(wchar_t* line, wchar_t** exe, wchar_t** args)
{
    wchar_t* p = line;
    if (*p == L'"') {
        *exe = ++p;
        while (*p && *p != L'"')
            p++;
    } else {
        *exe = p;
        while (*p && *p != L' ')
            p++;
    }
    if (*p)
        *p++ = 0;
    while (*p == L' ')
        p++;
    *args = p;
}

static void Start(wchar_t* rest, bool wait, FILE* out)
{
    wchar_t *exe, *args;
    Split(rest, &exe, &args);
    PROCESS_INFORMATION pi;
    if (!CreateProcess(exe, args, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
        Note(out, "fail %s: %lu\n", exe, GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    if (!wait) {
        if (s_nStarted < 32)
            s_started[s_nStarted++] = pi.hProcess;
        else
            CloseHandle(pi.hProcess);
        Note(out, "started %s (%lu)\n", exe, 0);
        return;
    }
    DWORD t0 = GetTickCount(), code = 0;
    WaitForSingleObject(pi.hProcess, 15 * 60 * 1000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    char a[600];
    WideCharToMultiByte(CP_ACP, 0, exe, -1, a, sizeof(a), NULL, NULL);
    fprintf(out, "exit %s: %lu (%lu ms)\n", a, code, GetTickCount() - t0);
    fflush(out);
}

static void Kill(FILE* out)
{
    for (int i = 0; i < s_nStarted; i++) {
        TerminateProcess(s_started[i], 0);
        CloseHandle(s_started[i]);
    }
    fprintf(out, "killed %d\n", s_nStarted);
    s_nStarted = 0;
}

// One command file; false: quit
static bool RunFile(const wchar_t* file)
{
    FILE* in = _wfopen(file, L"rb");
    if (!in)
        return true;
    wchar_t donePath[MAX_PATH];
    Path(donePath, L"done.tmp");
    FILE* out = _wfopen(donePath, L"wb");
    bool go = true;
    char line[600];
    while (go && fgets(line, sizeof(line), in)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' '))
            line[--n] = 0;
        wchar_t w[600];
        MultiByteToWideChar(CP_ACP, 0, line, -1, w, 600);
        if (!wcsncmp(w, L"run ", 4))
            Start(w + 4, false, out);
        else if (!wcsncmp(w, L"wait ", 5))
            Start(w + 5, true, out);
        else if (!wcscmp(w, L"kill"))
            Kill(out);
        else if (!wcsncmp(w, L"close ", 6)) {
            HWND h = FindWindow(NULL, w + 6);
            if (h)
                PostMessage(h, WM_CLOSE, 0, 0);
            fprintf(out, "close: %s\n", h ? "sent" : "no such window");
        }
        else if (!wcsncmp(w, L"sleep ", 6))
            Sleep(_wtoi(w + 6));
        else if (!wcscmp(w, L"quit"))
            go = false;
    }
    fclose(in);
    fprintf(out, "done %lu\n", GetTickCount());
    fclose(out);
    wchar_t done[MAX_PATH];
    Path(done, L"done.txt");
    DeleteFile(done);
    MoveFile(donePath, done);   // (whole: the PC reads it only once it is there)
    return go;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR cmd, int)
{
    if (cmd && !wcscmp(cmd, L"uninstall"))   // (autorun: the card taken out)
        return 0;
    HANDLE m = CreateMutex(NULL, FALSE, L"VestaEmuRun");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
    wchar_t cmdPath[MAX_PATH], runPath[MAX_PATH], alive[MAX_PATH];
    Path(cmdPath, L"cmd.txt");
    Path(runPath, L"cmd.run");
    Path(alive, L"alive.txt");
    FILE* a = _wfopen(alive, L"wb");
    if (a) {
        fprintf(a, "emurun %lu\n", GetTickCount());
        fclose(a);
    }
    for (;;) {
        Sleep(300);
        if (GetFileAttributes(cmdPath) == 0xFFFFFFFF)
            continue;
        DeleteFile(runPath);
        if (!MoveFile(cmdPath, runPath))
            continue;
        if (!RunFile(runPath))
            break;
    }
    CloseHandle(m);
    return 0;
}
