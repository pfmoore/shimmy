/* A config block must have the following format:
 *
 * [shim]
 * executable=path/name/to/app.exe
 * base=basedir
 * cwd=directory
 * [env]
 * key=value
 * key=value
 * [path]
 * added
 * added
 *
 * In the [shim] section, only the 3 given keys are allowed, but only
 * executable is mandatory.
 * The [env] and [path] sections can be in either order, but [shim] must be
 * first. The only mandatory section is [shim].
 * Whitespace at the start and end of lines is ignored.
 * Whitespace around the = sign is ignored.
 * Comment lines (starting with ;) are ignored, as are blank lines.
 * Content is in UTF-8 and lines end with CRLF or LF.
 *
 * The executable can be an absolute pathname (in which case base must
 * not be present) or a relative one. If relative, it defaults to being
 * relative to the location of the shim, but that can be modified with
 * the base element. The base element must take one of the following
 * forms:
 *
 *     cmd:xxx
 *         The base directory is the directory where command xxx is
 *         located (found by searching PATH)
 *     reg:HKLM\Key\Key\Key:Value
 *         The registry value specified. Value (and the colon) may be
 *         omitted, in which case the default value of the key is used.
 *     env:VAR
 *         The value of the environment variable VAR
 *
 * Multiple base elements are allowed. They are tried in turn.
 */
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <string>
#include <iostream>
#include <vector>

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

std::vector<std::string> split(std::string data, char sep) {
    std::vector<std::string> ret;
    size_t pos = 0;
    for (;;) {
        std::cout << "pos = " << pos << std::endl;
        size_t newpos = data.find(sep, pos);
        std::string next = data.substr(pos, newpos - pos);
        ret.push_back(next);
        if (newpos == std::string::npos)
            break;
        pos = newpos + 1;
    }
    return ret;
}

#include <windows.h>
struct CPSaver {
    unsigned int old;
    CPSaver() {
        old = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
    }
    ~CPSaver() {
        SetConsoleOutputCP(old);
    }
};

int main() {
    CPSaver sv;
    std::string s("  an example \n   of \r\nsome \nda€ta\n");
    auto v = split(s, '\n');
    for (auto str : v) {
        trim(str);
        std::cout << '"' << str << '"' << std::endl;
    }
}
