#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string>
#include <iostream>

void error(wchar_t *format, ...)
{
    va_list va;

    va_start(va, format);
    vfwprintf_s(stderr, format, va);
    exit(1);
}

const int DEBUG=1;

static void
debug(char *format, ...)
{
    va_list va;

    if (DEBUG) {
        va_start(va, format);
        vprintf_s(format, va);
    }
}

void print_w(std::wstring s) {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    s = s + L"\n";
    WriteConsoleW(consoleHandle, s.c_str(), s.length(), NULL, NULL); 
}

#if 0

void process_entry(std::string section, std::string key, std::string value) {
    std::cout << "Section " << section << ": " << key << " -> " << value << std::endl;
    if (section == "shim" && key == "cwd") {
        set_cwd(value);
    } else if (section = "environment") {
        set_env(key, value);
    } else if (section == "path") {
        add_path(key);
    } else if (section == "location") {
        add_loc(key, value);
    } else if (section == "shim" && key == "exe") {
        set_executable(value);
    } else {
        error("Invalid item in config: [%s] %s=%s", section.c_str(), key.c_str(), value.c_str());
    }
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

#endif

std::wstring strip(std::wstring str) {
    const auto WS = L" \t";
    auto s = str.find_first_not_of(WS);
    if (s == std::wstring::npos)
        return std::wstring();
    auto e = str.find_last_not_of(WS);
    return str.substr(s, e-s+1);
}

std::pair<std::wstring, std::wstring> split(std::wstring str, wchar_t sep) {
    auto pos = str.find(sep);
    std::wstring after;
    if (pos != std::string::npos)
        after = str.substr(pos+1);
    return std::make_pair(str.substr(0, pos), after);
}

struct LineGen
{
    LineGen(std::wstring data) : data_(data), pos_(0) {}
    std::wstring next() {
        const auto NL = L"\r\n";
        if (finished())
            return std::wstring();
        auto end = data_.find_first_of(NL, pos_);
        auto ret = data_.substr(pos_, end-pos_);
        if (end != std::wstring::npos)
            end = data_.find_first_not_of(NL, end);
        pos_ = end;
        return ret;
    }

    bool finished() {
        return pos_ == std::wstring::npos;
    }

    private:
    std::wstring data_;
    std::wstring::size_type pos_;
};

std::wstring make_wstring(const char *start, const char *end) {
    size_t len = end - start;
    size_t wlen;
    wchar_t *buf;
    wlen = MultiByteToWideChar(CP_UTF8, 0, start, len, 0, 0);
    buf = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (buf == 0)
        error(L"No memory to convert string");
    wlen = MultiByteToWideChar(CP_UTF8, 0, start, len, buf, wlen);
    if (wlen == 0)
        error(L"Oops, could not convert string (%d)", GetLastError());
    /* Don't include the terminating null character */
    if (buf[wlen-1] == 0) wlen -= 1;
    auto ret = std::wstring(buf, wlen);
    free(buf);
    return ret;
}

const char MARKER[] = "[shim]";

std::wstring load_appended_config()
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
    std::wstring config;

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
            config = make_wstring(p, end);
            debug("Successful conversion\n");
            break;
        }
    }

    bFlag = UnmapViewOfFile(lpMapAddress);
    bFlag = CloseHandle(hMapFile);
    bFlag = CloseHandle(hFile);
    if(!bFlag) {
        // Error occurred - do we report it?
    }
    return config;
}

void set_cwd(std::wstring dir) {
    if (!SetCurrentDirectoryW(dir.c_str()))
        error(L"Couldn't set directory to %ls", dir.c_str());
}

void set_env(std::wstring name, std::wstring value) {
    if (!SetEnvironmentVariableW(name.c_str(), value.c_str()))
        error(L"Couldn't set %ls to %ls", name.c_str(), value.c_str());
}

void add_path(std::wstring entry) {

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
run_child(std::wstring cmd) {
    wchar_t * cmdline = const_cast<wchar_t*>(cmd.c_str());
    HANDLE job;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    DWORD rc;
    BOOL ok;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    print_w(cmd);

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

    debug("run_child: about to run '%s'\n", cmdline);
    job = CreateJobObject(NULL, NULL);
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                  &info, sizeof(info), &rc);
    if (!ok || (rc != sizeof(info)) || !job)
        error(L"Job information querying failed");
    info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                             JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info,
                                 sizeof(info));
    if (!ok)
        error(L"Job information setting failed");
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    ok = safe_duplicate_handle(GetStdHandle(STD_INPUT_HANDLE), &si.hStdInput);
    if (!ok)
        error(L"stdin duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_OUTPUT_HANDLE), &si.hStdOutput);
    if (!ok)
        error(L"stdout duplication failed");
    ok = safe_duplicate_handle(GetStdHandle(STD_ERROR_HANDLE), &si.hStdError);
    if (!ok)
        error(L"stderr duplication failed");

    ok = SetConsoleCtrlHandler(ctrl_c_handler, TRUE);
    if (!ok)
        error(L"control handler setting failed");

    si.dwFlags = STARTF_USESTDHANDLES;
    ok = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE,
                        0, NULL, NULL, &si, &pi);
    if (!ok) {
        /* TODO: Fix up - cmdline is a wchar*! */
        error(L"Unable to create process using '%ls'", cmdline);
    }
    AssignProcessToJobObject(job, pi.hProcess);
    CloseHandle(pi.hThread);
    WaitForSingleObjectEx(pi.hProcess, INFINITE, FALSE);
    ok = GetExitCodeProcess(pi.hProcess, &rc);
    if (!ok)
        error(L"Failed to get exit code of process");
    debug("child process exit code: %d\n", rc);
    exit(rc);
}

int main(int argc, char *argv[])
{
    std::wstring config = load_appended_config();
    auto lines = LineGen(config);
    std::wstring section;
    std::wstring executable;
    while (!lines.finished()) {
        auto line = lines.next();
        if (line.front() == L'[' && line.back() == L']') {
            section = line.substr(1, line.length()-2);
            continue;
        }
        auto p = split(line, L'=');
        auto key = strip(p.first);
        auto value = strip(p.second);
        print_w(section);
        print_w(key);
        print_w(value);
        if (section == L"shim") {
            if (key == L"executable") {
                debug("executable is %ls\n", value.c_str());
                executable = value;
            }
            else if (key == L"cwd")
                set_cwd(value);
        } else if (section == L"environment") {
            set_env(key, value);
        } else if (section == L"path") {
            add_path(key);
        }
    }
    run_child(executable);
}
