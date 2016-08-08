#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include <iostream>
#include <sstream>

void error(wchar_t *format, ...)
{
    va_list va;

    va_start(va, format);
    vfwprintf_s(stderr, format, va);
    exit(1);
}

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

void process_entry(std::string section, std::string key, std::string value) {
    std::cout << "Section " << section << ": " << key << " -> " << value << std::endl;
}

void parse_config(const char *p, size_t len) {
    const char *end = p+len;
    const char *section_start;
    const char *section_end;
    const char *key_start;
    const char *key_end;
    const char *value_start;
    const char *value_end;

    while (p < end) {
        /* Skip whitespace */
        while (p < end && isspace(*p))
            ++p;
        if (p >= end)
            break;

        /* Section header? */
        if (*p == '[') {
            const char *q = p+1;
            section_start = q;
            while (q < end && *q != ']')
                ++q;
            section_end = q;
            ++q;
            while (q < end && isspace(*q))
                ++q;
            if (q >= end)
                break;
            if (q[-1] !='\n')
                error(L"Unexpected non-whitespace at end of section line");
            p = q;
        }

        while (p < end && isspace(*p))
            ++p;
        if (p >= end)
            break;
        key_start = p;
        while (p < end && *p != '=')
            ++p;
        if (p >= end)
            break;
        key_end = p;
        ++p;
        while (p < end && isspace(*p))
            ++p;
        if (p >= end)
            break;
        value_start = p;
        while (p < end && *p != '\n')
            ++p;
        if (p >= end)
            break;
        value_end = (p[-1] == '\r' ? p-1 : p);
        process_entry(std::string(section_start, section_end), std::string(key_start, key_end), std::string(value_start, value_end));
        ++p;
    }
}

const char MARKER[] = "[shim]";

void load_appended_config()
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

    /* There doesn't seem to be a way to handle paths longer than MAX_PATH */
    if (!GetModuleFileNameW(NULL, exe_name, MAX_PATH))
        error(L"Could not get executable name");

    hFile = CreateFileW(exe_name, GENERIC_READ,
                /* Let everyone else at the file, e.g., antivirus? */
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    debug("Handle: %p\n", hFile);
    if (hFile == INVALID_HANDLE_VALUE)
        error(L"Could not open executable %d", GetLastError());

    dwFileSize = GetFileSize(hFile, NULL);
    debug("File Size: %ld\n", dwFileSize);
    if (dwFileSize == 0)
        error(L"Executable appears to be 0 bytes long");

    hMapFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    debug("Mapping Handle: %p\n", hMapFile);
    if (hMapFile == NULL)
        error(L"Mapping the file failed - error %d", GetLastError());

    lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
    debug("Mapping Address: %p\n", lpMapAddress);
    if (lpMapAddress == NULL)
        error(L"Mapping the view failed - error %d", GetLastError());

    start = ((const char *)lpMapAddress);
    p = end = start + dwFileSize;
    if (end[-1] != '\n')
        error(L"The file does not have an appended config\n");
    p -= sizeof(MARKER) - 1;
    while (p > start) {
        if (*--p == *MARKER && memcmp(p, MARKER, sizeof(MARKER)-1) == 0) {
            debug("Got the chunk:\n%.*s\n", (unsigned int)(end-p), p);
            parse_config(p, end-p);
            break;
        }
    }

    bFlag = UnmapViewOfFile(lpMapAddress);
    bFlag = CloseHandle(hMapFile);
    bFlag = CloseHandle(hFile);
    if(!bFlag) {
        // Error occurred - do we report it?
    }
}


int main(int argc, char *argv[])
{
    load_appended_config();
}
