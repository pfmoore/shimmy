import sys
import os

sys.path.append('C:\\Work\\Projects\\shimmy\\rewrite')

from _lib import lib, ffi

def test_skip_initarg():
    tests = [
        'a b c',
        '"a better test" b c',
        '" a better test" b c'
            ]
    for t in tests:
        # Some memory management issues here.
        #   skip_initarg returns a pointer into the original string
        #   so we need to keep that string, and that means
        #   explicitly allocating it with ffi.new
        x = ffi.new('wchar_t[]', t)
        assert ffi.string(lib.skip_initarg(x)) == 'b c'

def test_set_cwd():
    dest = "C:\\"
    lib.set_cwd(dest)
    assert os.getcwd() == dest

def test_find_on_path():
    sysexe = ffi.string(lib.find_on_path("python"))
    assert sysexe == os.path.dirname(sys.executable)
