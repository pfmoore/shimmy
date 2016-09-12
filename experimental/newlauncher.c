/* File mapping code, to read a script appended to the executable
 *
 */

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include <stdint.h>

/* Appended data
 * =============
 *
 * The executable must have an appended block of data describing the
 * target process to be executed. The data takes the following form.
 *
 * [unsigned int32] Marker: 0x4D494853 (b'SHIM')
 * [unsigned int32] Executable path type:
 *         0 = absolute path
 *         1 = path relative to shim executable
 *         2 = path relative to an exe on PATH
 *         3 = path relative to an environment variable
 *         4 = path relative to a registry entry
 * [wchar_t] Executable path
 * [wchar_t] Optional executable path base (ignored for types 0 and 1)
 * [wchar_t] Optional current working directory
 * Repeat until we get a lngth-0 name & value
 *     [wchar_t] Environment variable name
 *     [wchar_t] Environment variable value
 *     TODO: Special cases for things like shim path, exe path?
 *     TODO: Expand variables in the value?
 * [unsigned int32] Length of appended data block
 */

typedef struct {
    uint32_t marker;
    uint32_t path_type;
    uint32_t exe_offset;
    uint32_t exe_base_offset;
    uint32_t cwd_offset;
    uint32_t env_offset;
    wchar_t  char_data[1];
} shim_data;

#define exe(data) (&data->char_data[data->exe_offset])
#define exe_base(data) (data->exe_base_offset ? &data->char_data[data->exe_base_offset] : 0)
#define cwd(data) (data->cwd_offset ? &data->char_data[data->cwd_offset] : 0)
#define env(data) (data->env_offset ? &data->char_data[data->env_offset] : 0)

const uint32_t MARKER = 0x4D494853;

void load_appended_script()
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
    uint32_t numbytes;
    uint32_t marker;
    shim_data *d;

    /* Open the executable and return a pointer to the appended data */
    if (!GetModuleFileNameW(NULL, exe_name, MAX_PATH)) {
        assert(!"Successfully got module file name");
    }
    hFile = CreateFileW(exe_name, GENERIC_READ,
                /* Let everyone else at the file, e.g., antivirus? */
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        assert(!"Successfully opened file");
    }
    dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == 0) {
        assert(!"Successfully got file size");
    }
    hMapFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapFile == NULL) {
        assert(!"Successfully created file mapping");
    }
    lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
    if (lpMapAddress == NULL) {
        assert(!"Successfully mapped view of file");
    }

    start = ((const char *)lpMapAddress);
    printf("Start is %p\n", start);
    p = end = start + dwFileSize;
    printf("End is %p\n", end);
    p -= sizeof(uint32_t);
    numbytes = *(uint32_t *)p;
    printf("Bytes is %d\n", numbytes);
    start = end - numbytes;
    d = (shim_data *)start;
    printf("Marker is %.8x (vs %.8x)\n", d->marker, MARKER);
    if (d->marker != MARKER) {
        assert(!"Successfully found marker");
    }
    printf("Path type is %d\n", d->path_type);
    assert(d->path_type == 0);
    printf("Exe length is %zd\n", wcslen(exe(d)));
    assert(wcslen(exe(d)) > 0);

#if 0
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
#endif

    bFlag = UnmapViewOfFile(lpMapAddress);
    bFlag = CloseHandle(hMapFile);
    bFlag = CloseHandle(hFile);
    if(!bFlag) {
        // Error occurred - do we report it?
    }
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

    job = CreateJobObject(NULL, NULL);
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                  &info, sizeof(info), &rc);
    if (!ok || (rc != sizeof(info)) || !job)
        assert(0); // error(L, "Job information querying failed");
    info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                                 sizeof(info));
    if (!ok)
        assert(0); // error(L, "Job information setting failed");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    ok = safe_duplicate_handle(GetStdHandle(STD_INPUT_HANDLE), &si.hStdInput);
    if (!ok)
        assert(0); // error(L, "stdin duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_OUTPUT_HANDLE), &si.hStdOutput);
    if (!ok)
        assert(0); // error(L, "stdout duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_ERROR_HANDLE), &si.hStdError);
    if (!ok)
        assert(0); // error(L, "stderr duplication failed");

    ok = SetConsoleCtrlHandler(ctrl_c_handler, TRUE);
    if (!ok)
        assert(0); // error(L, "control handler setting failed");

    si.dwFlags = STARTF_USESTDHANDLES;
    ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                        0, NULL, NULL, &si, &pi);
    if (!ok) {
        /* TODO: Fix up - cmdline is a wchar*! */
        assert(0); // error(L, "Unable to create process using '%s'", cmdline);
    }
    AssignProcessToJobObject(job, pi.hProcess);
    CloseHandle(pi.hThread);
    WaitForSingleObjectEx(pi.hProcess, INFINITE, FALSE);
    ok = GetExitCodeProcess(pi.hProcess, &rc);
    if (!ok)
        assert(0); // error(L, "Failed to get exit code of process");
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
    load_appended_script();
    return EXIT_SUCCESS;
}

#endif
