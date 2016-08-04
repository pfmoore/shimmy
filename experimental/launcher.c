/*
 * Copyright (C) 2011-2013 Vinay Sajip.
 * Licensed to PSF under a contributor agreement.
 *
 * Based on the work of:
 *
 * Mark Hammond (original author of Python version)
 * Curt Hagenlocher (job management)
 */

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <tchar.h>

#define MSGSIZE 1024

/* Error codes */

#define RC_NO_STD_HANDLES   100
#define RC_CREATE_PROCESS   101
#define RC_NO_MEMORY        104

/* Just for now - static definition */

static FILE * log_fp = NULL;

static void
debug(wchar_t * format, ...)
{
    va_list va;

    if (log_fp != NULL) {
        va_start(va, format);
        vfwprintf_s(log_fp, format, va);
    }
}

static void
winerror(int rc, wchar_t * message, int size)
{
    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, rc, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message, size, NULL);
}

static void
error(int rc, wchar_t * format, ... )
{
    va_list va;
    wchar_t message[MSGSIZE];
    wchar_t win_message[MSGSIZE];
    int len;

    va_start(va, format);
    len = _vsnwprintf_s(message, MSGSIZE, _TRUNCATE, format, va);

    if (rc == 0) {  /* a Windows error */
        winerror(GetLastError(), win_message, MSGSIZE);
        if (len >= 0) {
            _snwprintf_s(&message[len], MSGSIZE - len, _TRUNCATE, L": %ls",
                         win_message);
        }
    }

#if !defined(_WINDOWS)
    fwprintf(stderr, L"%ls\n", message);
#else
    MessageBox(NULL, message, TEXT("Python Launcher is sorry to say ..."),
               MB_OK);
#endif
    exit(rc);
}

/*
 * Process creation code
 */

static BOOL
safe_duplicate_handle(HANDLE in, HANDLE * pout)
{
    BOOL ok;
    HANDLE process = GetCurrentProcess();
    DWORD rc;

    *pout = NULL;
    ok = DuplicateHandle(process, in, process, pout, 0, TRUE,
                         DUPLICATE_SAME_ACCESS);
    if (!ok) {
        rc = GetLastError();
        if (rc == ERROR_INVALID_HANDLE) {
            debug(L"DuplicateHandle returned ERROR_INVALID_HANDLE\n");
            ok = TRUE;
        }
        else {
            debug(L"DuplicateHandle returned %d\n", rc);
        }
    }
    return ok;
}

static BOOL WINAPI
ctrl_c_handler(DWORD code)
{
    return TRUE;    /* We just ignore all control events. */
}

static void
run_child(wchar_t * cmdline)
{
    HANDLE job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    DWORD rc;
    BOOL ok;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

#if defined(_WINDOWS)
    // When explorer launches a Windows (GUI) application, it displays
    // the "app starting" (the "pointer + hourglass") cursor for a number
    // of seconds, or until the app does something UI-ish (eg, creating a
    // window, or fetching a message).  As this launcher doesn't do this
    // directly, that cursor remains even after the child process does these
    // things.  We avoid that by doing a simple post+get message.
    // See http://bugs.python.org/issue17290 and
    // https://bitbucket.org/vinay.sajip/pylauncher/issue/20/busy-cursor-for-a-long-time-when-running
    MSG msg;

    PostMessage(0, 0, 0, 0);
    GetMessage(&msg, 0, 0, 0);
#endif

    debug(L"run_child: about to run '%ls'\n", cmdline);
    job = CreateJobObject(NULL, NULL);
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                  &info, sizeof(info), &rc);
    if (!ok || (rc != sizeof(info)) || !job)
        error(RC_CREATE_PROCESS, L"Job information querying failed");
    info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                                 sizeof(info));
    if (!ok)
        error(RC_CREATE_PROCESS, L"Job information setting failed");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    ok = safe_duplicate_handle(GetStdHandle(STD_INPUT_HANDLE), &si.hStdInput);
    if (!ok)
        error(RC_NO_STD_HANDLES, L"stdin duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_OUTPUT_HANDLE), &si.hStdOutput);
    if (!ok)
        error(RC_NO_STD_HANDLES, L"stdout duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_ERROR_HANDLE), &si.hStdError);
    if (!ok)
        error(RC_NO_STD_HANDLES, L"stderr duplication failed");

    ok = SetConsoleCtrlHandler(ctrl_c_handler, TRUE);
    if (!ok)
        error(RC_CREATE_PROCESS, L"control handler setting failed");

    si.dwFlags = STARTF_USESTDHANDLES;
    ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                        0, NULL, NULL, &si, &pi);
    if (!ok)
        error(RC_CREATE_PROCESS, L"Unable to create process using '%ls'", cmdline);
    AssignProcessToJobObject(job, pi.hProcess);
    CloseHandle(pi.hThread);
    WaitForSingleObjectEx(pi.hProcess, INFINITE, FALSE);
    ok = GetExitCodeProcess(pi.hProcess, &rc);
    if (!ok)
        error(RC_CREATE_PROCESS, L"Failed to get exit code of process");
    debug(L"child process exit code: %d\n", rc);
    exit(rc);
}

#if defined(_WINDOWS)

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPWSTR lpstrCmd, int nShow)
{
    return process(__argc, __wargv);
}

#else

int cdecl wmain(int argc, wchar_t ** argv)
{
    return process(argc, argv);
}

#endif
