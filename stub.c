#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static wchar_t MARKER[] = L"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

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

int run_process(wchar_t *args) {
    BOOL ret = FALSE;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    int exit = 0;
    wchar_t *placeholder;
    size_t len = wcslen(MARKER) + wcslen(args) + 2;
    wchar_t *cmdline = malloc(len * sizeof(wchar_t));

    if (cmdline == 0) {
        fwprintf(stderr, L"Cannot allocate %d bytes of memory for command line\n", len * sizeof(wchar_t));
        return 1;
    }

    placeholder = wcsstr(MARKER, L"%s");
    if (placeholder) {
        swprintf(cmdline, len, L"%.*s%s%s",
            (placeholder-MARKER), MARKER, args, placeholder+2);
    } else {
        swprintf(cmdline, len, L"%s %s", MARKER, args);
    }
    /* wprintf(L"Executing: %s\n", cmdline); */

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    ret = CreateProcessW(0, cmdline, 0, 0, FALSE, 0, 0, 0, &si, &pi);
    free(cmdline);

    if (!ret) {
        fwprintf(stderr, L"CreateProcess failed: %d\n", GetLastError());
        return 1;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exit);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit;
}

int main(int argc, char *argv[]) {
    wchar_t *cmdline = GetCommandLineW();
    cmdline = skipargv0(cmdline);
    /* wprintf(L"Command line: %ls\n", cmdline); */
    return run_process(cmdline);
}
