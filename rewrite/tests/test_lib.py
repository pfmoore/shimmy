from ctypes import cdll, c_wchar_p

def test_skip_initarg():
    l = cdll.lib
    l.skip_initarg.restype = c_wchar_p
    assert l.skip_initarg('a b c') == 'b c'
    assert l.skip_initarg('"a better test" b c') == 'b c'
    assert l.skip_initarg('" a better test" b c') == 'b c'
