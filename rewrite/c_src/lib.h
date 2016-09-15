/* Library of functions for the shim executable */

#ifndef __SHIM_LIB_H
#define __SHIM_LIB_H

wchar_t *skip_initarg(wchar_t *cmdline);
void set_cwd(wchar_t *cwd);
wchar_t *find_on_path(wchar_t *name);

/* Returns allocated memory - needs free() */
wchar_t *make_absolute(wchar_t *path, wchar_t *base);

#endif
