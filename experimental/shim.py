import struct

class Shim:
    def __init__(self, exe):
        self.exe_type = 0
        self.exe = exe
        self.exe_base = None
        self.cwd = None
        self.env = []
    def serialize(self):
        fmt = '4s5L'
        hdrlen = struct.calcsize(fmt)
        parts = []
        exe_offset = 0
        exe_base_offset = 0
        cwd_offset = 0
        env_offset = 0
        parts.append(self.exe)
        parts.append('\0')
        if self.exe_base:
            exe_base_offset = sum(len(p) for p in parts)
            parts.append(self.exe_base)
            parts.append('\0')
        if self.cwd:
            cwd_offset = sum(len(p) for p in parts)
            parts.append(self.cwd)
            parts.append('\0')
        if self.env:
            env_offset = sum(len(p) for p in parts)
            for k, v in self.env:
                parts.append(k)
                parts.append('\0')
                parts.append(v)
                parts.append('\0')
            parts.append('\0')
            parts.append('\0')
        hdr = struct.pack(fmt, b'SHIM', self.exe_type, exe_offset,
                          exe_base_offset, cwd_offset, env_offset)
        char_data = b''.join(s.encode('utf-16le') for s in parts)
        footer = struct.pack('L', len(hdr) + len(char_data) + struct.calcsize('L'))
        return hdr + char_data + footer
