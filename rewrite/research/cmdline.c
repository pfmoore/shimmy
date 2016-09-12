/* Display the command line used to execute this program */

#include <windows.h>
#include <string.h>
#include <stdio.h>

void write(wchar_t *str) {
    int i;
    wchar_t fname[MAX_PATH];
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD ret = GetEnvironmentVariableW(L"PROGRAM_OUTPUT", fname, MAX_PATH);
    wprintf(L"%s\n", str);
    // WriteConsoleW(out, str, wcslen(str), &i, NULL);
    // WriteConsoleW(out, L"\n", 1, &i, NULL);
    if (ret != 0) {
        HANDLE h = CreateFileW(fname, GENERIC_WRITE, 0, 0, OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, 0);
        SetFilePointer(h, 0, 0, FILE_END);
        WriteFile(h, str, wcslen(str) * sizeof(wchar_t), 0, 0);
        WriteFile(h, L"\n", sizeof(wchar_t), 0, 0);
        CloseHandle(h);
    }
}

int main()
{
    wchar_t *cmdline = GetCommandLineW();
    write(cmdline);
    return 0;
}
