import struct

class StringBuffer:
    def __init__(self):
        self.offset = 0
        self.data = []
    def add(self, s):
        if s is None:
            return 0
        pos = self.offset
        byte_data = s.encode('utf-16le')
        self.offset += len(byte_data) + 2
        self.data.append(byte_data)
        return pos
    def add_list(self, env):
        if not env:
            return 0
        pos = self.offset
        for k, v in env:
            self.add(k)
            self.add(v)
        # Terminator
        self.add('')
        self.add('')
        return pos
    def stringdata(self):
        self.data.append(b'')
        return b'\0\0'.join(self.data)

class Shim:
    def __init__(self, exe):
        self.exe_type = 0
        self.exe = exe
        self.exe_base = None
        self.cwd = None
        self.env = []
    def serialize(self):
        buf = StringBuffer()
        fmt = '4s5L'
        hdrlen = struct.calcsize(fmt)
        exe_offset = buf.add(self.exe)
        exe_base_offset = buf.add(self.exe_base)
        cwd_offset = buf.add(self.cwd)
        env_offset = buf.add_list(self.env)
        hdr = struct.pack(fmt, b'SHIM', self.exe_type, exe_offset,
                          exe_base_offset, cwd_offset, env_offset)
        hdr += buf.stringdata()
        footer = struct.pack('L', len(hdr) + struct.calcsize('L'))
        return hdr + footer
