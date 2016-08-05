/* File mapping code, to read a script appended to the executable
 *
 */

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <lua.h>
#include <lauxlib.h>

#include <stdarg.h>

const int DEBUG=0;

static void
debug(char *format, ...)
{
    va_list va;

    if (DEBUG) {
        va_start(va, format);
        vprintf_s(format, va);
    }
}

static void
error(lua_State *L, char *format, ...)
{
    va_list va;

    va_start(va, format);
    lua_pushvfstring(L, format, va);
    lua_error(L);
}

/* Lua utility functions for handling wide characters
 */
void luaX_pushwstring(lua_State *L, wchar_t *s) {
    size_t sz;
    size_t ret;
    char *buf;
    luaL_Buffer b;
    sz = WideCharToMultiByte(CP_UTF8, 0, s, -1, 0, 0, NULL, NULL);
    buf = luaL_buffinitsize(L, &b, sz);
    ret = WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, sz, NULL, NULL);
    if (ret == 0) {
        lua_pushfstring(L, "Oops, could not convert string (%d)", GetLastError());
        lua_error(L);
    }
    luaL_pushresultsize(&b, sz);
}

wchar_t *luaX_newwstring(lua_State *L, int index) {
    size_t len;
    size_t wlen;
    size_t ret;
    const char *s;
    wchar_t *buf;
    s = lua_tolstring(L, index, &len);
    /* Allow for terminating NUL */
    len = len + 1;
    wlen = MultiByteToWideChar(CP_UTF8, 0, s, len, 0, 0);
    buf = lua_newuserdata(L, wlen*sizeof(wchar_t));
    ret = MultiByteToWideChar(CP_UTF8, 0, s, len, buf, wlen);
    if (ret == 0) {
        lua_pushfstring(L, "Oops, could not convert string (%d)", GetLastError());
        lua_error(L);
    }
    return buf;
}

const char MARKER[] = "#!Lua Script";

int load_appended_script(lua_State *L)
{
    BOOL bFlag;
    DWORD dwFileSize;
    wchar_t exe_name[MAX_PATH+1];
    HANDLE hMapFile;
    HANDLE hFile;
    LPVOID lpMapAddress;
    const char *start;
    const char *end;
    const char *p;
    int rc = LUA_OK;

    /* Otherwise, open the executable and return a pointer to the appended
     * data */

    if (!GetModuleFileNameW(NULL, exe_name, MAX_PATH)) {
        lua_pushfstring(L, "Could not get executable name");
        lua_error(L);
    }

    hFile = CreateFileW(exe_name, GENERIC_READ,
                /* Let everyone else at the file, e.g., antivirus? */
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    debug("Handle: %p\n", hFile);
    if (hFile == INVALID_HANDLE_VALUE) {
        lua_pushfstring(L, "Could not open executable %d", GetLastError());
        lua_error(L);
    }

    dwFileSize = GetFileSize(hFile, NULL);
    debug("File Size: %ld\n", dwFileSize);
    if (dwFileSize == 0) {
        lua_pushstring(L, "Executable appears to be 0 bytes long");
        lua_error(L);
    }

    hMapFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    debug("Mapping Handle: %p\n", hMapFile);
    if (hMapFile == NULL) {
        lua_pushfstring(L, "Mapping the file failed - error %d", GetLastError());
        lua_error(L);
    }

    lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
    debug("Mapping Address: %p\n", lpMapAddress);
    if (lpMapAddress == NULL) {
        lua_pushfstring(L, "Mapping the view failed - error %d", GetLastError());
        lua_error(L);
    }

    start = ((const char *)lpMapAddress);
    p = end = start + dwFileSize;
    p -= sizeof(MARKER) - 1;
    while (p > start) {
        if (*--p == *MARKER) {
            debug("Found a # at %p [%.20s] (%lld)\n", p, p, sizeof(MARKER)-1);
            if (memcmp(p, MARKER, sizeof(MARKER)-1) == 0) {
                /* Remove the shebang */
                p += sizeof(MARKER)-1;
                while (p < end && *p++ != '\n')
                    ;
                debug("Got the chunk:\n%.*s\n", (unsigned int)(end-p), p);
                luaX_pushwstring(L, exe_name);
                rc = luaL_loadbuffer(L, p, end-p, lua_tostring(L, -1));
                if (rc != LUA_OK)
                    lua_error(L);
                /* Drop the executable name */
                lua_remove(L, lua_gettop(L)-1);
                break;
            }
            debug("Not the marker :-(\n");
        }
    }

    bFlag = UnmapViewOfFile(lpMapAddress);
    bFlag = CloseHandle(hMapFile);
    bFlag = CloseHandle(hFile);
    if(!bFlag) {
        // Error occurred - do we report it?
    }
    return rc;
}

/*
 * Copyright (C) 2011-2013 Vinay Sajip.
 * Licensed to PSF under a contributor agreement.
 *
 * Based on the work of:
 *
 * Mark Hammond (original author of Python version)
 * Curt Hagenlocher (job management)
 */

// #include <shlobj.h>

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
            debug("DuplicateHandle returned ERROR_INVALID_HANDLE\n");
            ok = TRUE;
        }
        else {
            debug("DuplicateHandle returned %d\n", rc);
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
run_child(lua_State *L, wchar_t * cmdline)
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

    debug("run_child: about to run '%ls'\n", cmdline);
    job = CreateJobObject(NULL, NULL);
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                  &info, sizeof(info), &rc);
    if (!ok || (rc != sizeof(info)) || !job)
        error(L, "Job information querying failed");
    info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                                 sizeof(info));
    if (!ok)
        error(L, "Job information setting failed");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    ok = safe_duplicate_handle(GetStdHandle(STD_INPUT_HANDLE), &si.hStdInput);
    if (!ok)
        error(L, "stdin duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_OUTPUT_HANDLE), &si.hStdOutput);
    if (!ok)
        error(L, "stdout duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_ERROR_HANDLE), &si.hStdError);
    if (!ok)
        error(L, "stderr duplication failed");

    ok = SetConsoleCtrlHandler(ctrl_c_handler, TRUE);
    if (!ok)
        error(L, "control handler setting failed");

    si.dwFlags = STARTF_USESTDHANDLES;
    ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                        0, NULL, NULL, &si, &pi);
    if (!ok) {
        /* TODO: Fix up - cmdline is a wchar*! */
        error(L, "Unable to create process using '%s'", cmdline);
    }
    AssignProcessToJobObject(job, pi.hProcess);
    CloseHandle(pi.hThread);
    WaitForSingleObjectEx(pi.hProcess, INFINITE, FALSE);
    ok = GetExitCodeProcess(pi.hProcess, &rc);
    if (!ok)
        error(L, "Failed to get exit code of process");
    debug("child process exit code: %d\n", rc);
    exit(rc);
}

static int
luaX_execute(lua_State *L) {
    wchar_t *cmdline = luaX_newwstring(L, 1);
    run_child(L, cmdline);
    return 0;
}

/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack. It assumes that the error object
** is a string, as it was either generated by Lua or by 'msghandler'.
*/
static int report (lua_State *L, int status) {
  if (status != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    printf("PROBLEM: %s\n", msg);
    lua_pop(L, 1);  /* remove message */
  }
  return status;
}

static int pmain (lua_State *L) {
    luaL_openlibs(L);
    lua_register(L, "execute", luaX_execute);
    load_appended_script(L);
    lua_call(L, 0, 0);
    lua_pushboolean(L, 1);  /* Result: True is success */
    return 1; /* Number of returned values */
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
    int status;
    int result;
    lua_State *L = luaL_newstate();
    lua_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
    status = lua_pcall(L, 0, 1, 0);  /* do the call */
    result = lua_toboolean(L, -1);  /* get result */
    report(L, status);
    lua_close(L);
    return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
