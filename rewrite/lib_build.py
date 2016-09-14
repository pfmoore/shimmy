# file "lib_build.py"

# Note: we instantiate the same 'cffi.FFI' class as in the previous
# example, but call the result 'ffibuilder' now instead of 'ffi';
# this is to avoid confusion with the other 'ffi' object you get below

from cffi import FFI
ffibuilder = FFI()

ffibuilder.set_source("_lib",
        """extern void set_cwd(wchar_t *cwd); """,
    libraries=["lib"],
    library_dirs=["C:\\Work\\Projects\\shimmy\\rewrite"]
    )   # or a list of libraries to link with
    # (more arguments like setup.py's Extension class:
    # include_dirs=[..], extra_objects=[..], and so on)

ffibuilder.cdef("""     // some declarations from the man page
    void set_cwd(wchar_t *cwd);
""")

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
