/* Library of functions for the shim executable */

#include <tchar.h>

static wchar_t *skip_whitespace(wchar_t *p)
{
    while (*p && isspace(*p))
        ++p;
    return p;
}

__declspec(dllexport)
wchar_t *skip_initarg(wchar_t *cmdline)
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
