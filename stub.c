#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void db(char *msg, char *p) {
    fprintf(stderr, "%s: %.20s\n", msg, p);
}

void terminate(wchar_t *message, ...) {
    va_list argptr;
    va_start(argptr, message);
    vfwprintf(stderr, message, argptr);
    va_end(argptr);
    fwprintf(stderr, L"\n");

    exit(1);
}

int my_subsystem() {
    char *me = GetModuleHandle(NULL);
    PIMAGE_DOS_HEADER pdosheader = (PIMAGE_DOS_HEADER)me;
    PIMAGE_NT_HEADERS pntheaders = (PIMAGE_NT_HEADERS)(me + pdosheader->e_lfanew);
    return pntheaders->OptionalHeader.Subsystem;
}

#define MAX_APPENDED 1024
const char MARKER[] = "#! Shim Data"; /* Note, not wide string */

struct val {
    char *key;
    char *value;
};

wchar_t *exe_dir = 0;

struct val *get_appended_data(void) {
    FILE *fp = NULL;
    static char buf[MAX_APPENDED+1];
    wchar_t exe_name[MAX_PATH+1];
    wchar_t *wp;
    size_t n;
    int i;
    char *p;
    char *end;
    char *found;
    char *current_key;
    char *current_value;
    static struct val vals[100];

    /* Read the last MAX_APPENDED bytes from the exe */
    if (!GetModuleFileNameW(NULL, exe_name, MAX_PATH))
        terminate(L"Cannot get executable name: %d", GetLastError());
    if ((fp = _wfopen(exe_name, L"rb")) == NULL)
        terminate(L"Open failed: %d", GetLastError());
    if (fseek(fp, -MAX_APPENDED, SEEK_END))
        terminate(L"Seek failed");
    n = fread(buf, 1, MAX_APPENDED, fp);
    buf[n] = '\0';

    /* Get the executable's directory */
    wp = exe_name + wcslen(exe_name);
    while (wp > exe_name) {
        --wp;
        if (*wp == '\\') {
            *wp = '\0';
            break;
        }
    }
    exe_dir = wcsdup(exe_name);

    /* Find the start of the appended data */
    p = buf;
    found = 0;
    end = buf + n;
    db("Starting", p);
    while (p < end) {
        char *next = memchr(p, MARKER[0], end-p);
        db("Next", next);
        if (next == 0)
            break;
        if (memcmp(next, MARKER, sizeof(MARKER) - 1) == 0) {
            db("Found", next);
            found = next + sizeof(MARKER) - 1;
            while (*found == '\r' || *found == '\n')
                ++found;
        }
        p = next + 1;
    }

    if (!found)
        terminate(L"Appended shim data not found");

    wprintf(L"Appended data: %S\n", found);

    /* The appended data has the form:
     *     key=value\nkey=value\n...\0
     * Split into key, value pairs.
     * We modify the buffer to do this, by inserting nulls as needed.
     */
    p = found;
    i = 0;
    current_key = p;
    current_value = 0;
    while (p < end) {
        if (!current_value && (*p == '=')) {
            *p++ = '\0';
            current_value = p;
        }
        else if (*p == '\n' || *p == '\r') {
            *p++ = '\0';
            if (!current_value)
                terminate(L"Invalid data: %S has no value", current_key);
            vals[i].key = current_key;
            vals[i].value = current_value;
            ++i;
            /* Skip blank lines */
            while (p < end && (*p == '\n' || *p == '\r'))
                ++p;
            current_key = p;
            current_value = 0;
        }
        else {
            ++p;
        }
    }
    vals[i].key = 0;
    vals[i].value = 0;
    return vals;
}

void read_values(struct val *vals, int *wait, wchar_t **command) {
    while (vals->key) {
        if (strcmp(vals->key, "wait") == 0) {
            if (strcmp(vals->value, "0") == 0)
                *wait = 0;
            else if (strcmp(vals->value, "1") == 0)
                *wait = 1;
            else
                terminate(L"Invalid value for wait: %S", vals->value);
        }
        else if (strcmp(vals->key, "command") == 0) {
            wchar_t *out;
            int n = MultiByteToWideChar(CP_UTF8, 0, vals->value, -1, 0, 0);
            out = malloc(n * sizeof(wchar_t));
            if (out == NULL)
                terminate(L"Cannot allocate memory for command");
            MultiByteToWideChar(CP_UTF8, 0, vals->value, -1, out, n);
            *command = out;
        }
        else {
            terminate(L"Invalid key in shim data: '%S'", vals->key);
        }
        ++vals;
    }
}

/* Skip a quoted string: on entry, p points just after the opening quote.
 * Return value: pointer to the character after the terminating quote.
 */
wchar_t *skipquoted(wchar_t *p) {
    while (*p) {
        if (*p == '"')
            return p+1;

        /* Skip a backslash-escaped quote */
        if (*p == '\\' && p[1] == '"')
            p += 2;
        else
            ++p;
    }

    /* We didn't find a terminating quote - return the end of string */
    return p;
}

wchar_t *skipargv0(wchar_t *p) {
    while (isspace(*p))
        ++p;
    if (*p == '"')
        p = skipquoted(p+1);
    else {
        while (*p && !isspace(*p))
            ++p;
    }
    while (isspace(*p))
        ++p;

    return p;
}

wchar_t *interpolate(wchar_t *template, wchar_t *args) {
    wchar_t *p;
    wchar_t *q;
    size_t n = 0;
    wchar_t *result;

    wprintf(L"Template: %s\nArgs: %s\n", template, args);
    for (p = template; *p; ++p) {
        if (*p == '%') {
            ++p;
            switch (*p) {
                case '\0': ++n; break;
                case '%': ++n; break;
                case 'a': n += wcslen(args); break;
                case 'd':
                    if (exe_dir == 0)
                        terminate(L"Unable to determine executable directory");
                    n += wcslen(exe_dir);
                    break;
                default: terminate(L"Invalid format character '%c'", *p);
            }
            continue;
        }
        ++n;
    }
    wprintf(L"Result len: %d\n", n);

    result = malloc((n+1) * sizeof(wchar_t));
    if (result == 0)
        terminate(L"Cannot allocate %d bytes of memory for result", (n+1) * sizeof(wchar_t));

    q = result;
    for (p = template; *p; ++p) {
        if (*p == '%') {
            ++p;
            switch (*p) {
                case '\0':
                case '%': *q++ = '%'; break;
                case 'a': wcscpy(q, args); q += wcslen(args); break;
                case 'd': wcscpy(q, exe_dir); q += wcslen(exe_dir); break;
                default: terminate(L"Invalid format character '%c'", *p);
            }
            continue;
        }

        *q++ = *p;
    }

    *q = '\0';

    return result;
}

int run_process(wchar_t *args) {
    BOOL ret = FALSE;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    int exit = 0;
    wchar_t *placeholder;
    /* Subsystem 2 is GUI, 3 is console.
     * Don't wait if we're a GUI executable
     */
    int wait = my_subsystem() == 2 ? 0 : 1;
    wchar_t *cmdline;
    wchar_t *command;

    read_values(get_appended_data(), &wait, &command);
    fwprintf(stderr, L"Wait: %d, Command: %s\n", wait, command);

    cmdline = interpolate(command, args);
    wprintf(L"Executing (wait=%d): %s\n", wait, cmdline);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    ret = CreateProcessW(0, cmdline, 0, 0, FALSE, 0, 0, 0, &si, &pi);
    free(cmdline);

    if (!ret) {
        terminate(L"CreateProcess failed: %d", GetLastError());
    }

    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exit);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit;
}

int main(int argc, char *argv[]) {
    wchar_t *cmdline = GetCommandLineW();
    cmdline = skipargv0(cmdline);
    wprintf(L"Command line: %ls\n", cmdline);
    my_subsystem();
    return run_process(cmdline);
}
