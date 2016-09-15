# file "lib_build.py"

# Note: we instantiate the same 'cffi.FFI' class as in the previous
# example, but call the result 'ffibuilder' now instead of 'ffi';
# this is to avoid confusion with the other 'ffi' object you get below

from cffi import FFI
ffibuilder = FFI()

ffibuilder.set_source("_lib",
        """
        #include "lib.h"
        """,
    libraries=["lib"],
    include_dirs=["c_src"],
    library_dirs=["."]
    )   # or a list of libraries to link with
    # (more arguments like setup.py's Extension class:
    # include_dirs=[..], extra_objects=[..], and so on)

ffibuilder.cdef("""     // some declarations from the man page
    wchar_t *skip_initarg(wchar_t *cmdline);
    void set_cwd(wchar_t *cwd);
    wchar_t *find_on_path(wchar_t *name);

    wchar_t *make_absolute(wchar_t *path, wchar_t *base);
""")

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
