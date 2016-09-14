/* Library of functions for the shim executable */

#include <windows.h>
#include <tchar.h>

#define RC_CREATE_PROCESS 1
#define RC_NO_STD_HANDLES 2
#define RC_NOT_ENOUGH_MEM 3
#define RC_LOAD_DATA      4

static void error(int rc, wchar_t *msg, ...) {
    exit(rc);
}

static wchar_t *skip_whitespace(wchar_t *p)
{
    while (*p && isspace(*p))
        ++p;
    return p;
}

static wchar_t *skip_initarg(wchar_t *cmdline)
{
    int quoted;
    wchar_t c;
    wchar_t *result = cmdline;

    quoted = cmdline[0] == L'\"';
    if (!quoted)
        c = L' ';
    else {
        c = L'\"';
        ++result;
    }
    result = wcschr(result, c);
    if (result == NULL) /* when, for example, just exe name on command line */
        result = L"";
    else {
        ++result; /* skip past space or closing quote */
        result = skip_whitespace(result);
    }
    return result;
}

typedef struct kv {
    const wchar_t *key;
    const wchar_t *value;
} kv;

static void env_from_block(kv *block) {

    while (block->key) {
        SetEnvironmentVariableW(block->key, block->value);
        ++block;
    }
}

void set_cwd(wchar_t *cwd) {
    SetCurrentDirectoryW(cwd);
}

static wchar_t *my_dir() {
    static wchar_t *ret = NULL;

    if (ret == NULL) {
        DWORD sz = GetModuleFileNameW(NULL, NULL, 0);
        wchar_t *end;
        ret = (wchar_t *)calloc(sz + 1, sizeof(wchar_t));
        GetModuleFileNameW(NULL, ret, sz + 1);
        /* Remove the filename part */
        end = wcsrchr(ret, L'\\');
        *end = L'\0';
    }
    return ret;
}

static int is_absolute(wchar_t *path) {
    /* This means "don't tack a base directory on the front".
     * So even though C:foo is *technically* a relative path,
     * we say it's absolute because D:\dir\C:foo doesn't make sense.
     */
    return path[0] == L'\\' || (path[0] != L'\0' && path[1] == L':');
}

static wchar_t *find_on_path(wchar_t *name)
{
    wchar_t *pathext;
    size_t   varsize;
    wchar_t *context = NULL;
    wchar_t *extension;
    wchar_t *result = NULL;
    DWORD    len;
    errno_t  rc;
    /* Return value storage - we don't need to be re-entrant */
    static wchar_t path_buf[MAX_PATH];

    if (wcschr(name, L'.') != NULL) {
        /* assume it has an extension. */
        len = SearchPathW(NULL, name, NULL, MAX_PATH, path_buf, NULL);
        if (len) {
            result = path_buf;
        }
    }
    else {
        /* No extension - search using registered extensions. */
        rc = _wdupenv_s(&pathext, &varsize, L"PATHEXT");
        if (rc == 0) {
            extension = wcstok_s(pathext, L";", &context);
            while (extension) {
                len = SearchPathW(NULL, name, extension, MAX_PATH, path_buf, NULL);
                if (len) {
                    result = path_buf;
                    break;
                }
                extension = wcstok_s(NULL, L";", &context);
            }
            free(pathext);
        }
    }
    if (result) {
        /* We just want the directory */
        wchar_t *end = wcsrchr(result, L'\\');
        *end = L'\0';
    }
    return result;
}

static wchar_t *make_absolute(wchar_t *path, wchar_t *base) {
    wchar_t *ret;
    size_t sz;

    if (is_absolute(path)) {
        return _wcsdup(path);
    }
    sz = wcslen(path) + wcslen(base) + 2;
    ret = (wchar_t *)calloc(sz, sizeof(wchar_t));
    if (ret == NULL) {
        error(RC_NOT_ENOUGH_MEM, L"Not enough memory in make_absolute()");
    }
    /* TODO: base might have a terminating \ which we should skip */
    wcscpy(ret, base);
    wcscat(ret, L"\\");
    wcscat(ret, path);
    return ret;
}

static wchar_t *get_target(wchar_t *path, wchar_t *base, int type) {
    if (type == 0 && base == NULL) {
        /* Relative to base, if no base then shim location */
        base = my_dir();
    }
    else if (type == 1) {
        /* Relative to an exe on PATH */
        base = find_on_path(base);
    }
    else if (type == 2) {
        /* Relative to env variable */
        base = _wgetenv(base);
    }
    else if (type == 3) {
        /* Relative to registry entry */
        /* TODO: add this later */
    }
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
            ok = TRUE;
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
    exit(rc);
}

typedef struct shim_data {
    unsigned int exe_type;
    const wchar_t *exe;
    const wchar_t *base;
    const wchar_t *cwd;
    /* Multiple entries, terminated with {0, 0} */
    kv env[1];
} shim_data;


static unsigned int read_4byte(const unsigned char *loc) {
    return (loc[0] << 24) + (loc[1] << 16) + (loc[2] << 8) + loc[3];
}

static const wchar_t *read_string(const unsigned char *loc, const unsigned char *stringtab) {
    unsigned int n = read_4byte(loc);
    return n == 0xFFFFFFFF ? NULL : (wchar_t *)(stringtab + n);
}

void load_appended_data()
{
    BOOL bFlag;
    DWORD dwFileSize;
    wchar_t exe_name[MAX_PATH+1];
    HANDLE hMapFile;
    HANDLE hFile;
    LPVOID lpMapAddress;
    const unsigned char *start;
    const unsigned char *end;
    const unsigned char *stringtab;
    unsigned int n_env;
    unsigned int i;
    shim_data *data;

    if (!GetModuleFileNameW(NULL, exe_name, MAX_PATH)) {
        error(RC_LOAD_DATA, L"Could not get executable name");
    }

    hFile = CreateFileW(exe_name, GENERIC_READ,
                /* Let everyone else at the file, e.g., antivirus? */
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        error(RC_LOAD_DATA, L"Could not open executable %d", GetLastError());
    }

    dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == 0) {
        error(RC_LOAD_DATA, L"Executable appears to be 0 bytes long");
    }

    hMapFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMapFile == NULL) {
        error(RC_LOAD_DATA, L"Mapping the file failed - error %d", GetLastError());
    }

    lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
    if (lpMapAddress == NULL) {
        error(RC_LOAD_DATA, L"Mapping the view failed - error %d", GetLastError());
    }

    /* Get the appended data. Format is:
     *
     * XXXX....XXXX Data block (see below)
     * XXXX....XXXX Character data table, all strings are encoded as UTF-16LE and null terminated.
     * XXXX         Offset of the start of the character data from the appended data start.
     * XXXX         Length of appended data.
     */
    end = ((const char *)lpMapAddress) + dwFileSize;
    start = end - read_4byte(end-4);
    stringtab = start + read_4byte(end-8);

    /* Data block format:
     *
     *  0 XXXX Marker - "SHIM"
     *  4 XXXX Type of base relative (0 = pathname, 1 = exe on PATH, 2 = env var, 3 = registry)
     *  8 XXXX Offset of exe name in string table
     * 12 XXXX Offset of base for relative exe in string table (0xFFFFFFFF means "exe name is absolute")
     * 16 XXXX Offset of cwd in string table (0xFFFFFFFF means "don't set CWD")
     * 20 XXXX Number of environment variable entries
     * n times:
     *     24 + 8*n XXXX Offset of env var name in string table
     *     28 + 8*n XXXX Offset of env var value in string table (0xFFFFFFFF means "Remove")
     */

    /* Check for marker */
    if (memcmp(start, "SHIM", 4) != 0) {
        error(RC_LOAD_DATA, L"Data block does not start with correct marker");
    }

    n_env = read_4byte(start + 20);
    data = (shim_data *)malloc(sizeof(shim_data) + n_env * sizeof(kv));
    if (data == NULL) {
        error(RC_NOT_ENOUGH_MEM, L"Not enough memory for shim data");
    }

    data->exe_type = read_4byte(start + 4);
    data->exe = read_string(start + 8, stringtab);
    data->base = read_string(start + 12, stringtab);
    data->cwd = read_string(start + 16, stringtab);
    for (i = 0; i < n_env; ++i) {
        data->env[i].key = read_string(start + 24 + 8 * i, stringtab);
        data->env[i].value = read_string(start + 28 + 8 * i, stringtab);
    }
    data->env[n_env].key = 0;
    data->env[n_env].value = 0;

    bFlag = UnmapViewOfFile(lpMapAddress);
    bFlag = CloseHandle(hMapFile);
    bFlag = CloseHandle(hFile);
    if(!bFlag) {
        // Error occurred - do we report it?
    }
}
