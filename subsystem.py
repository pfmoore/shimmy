import sys
import struct


# 
# define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b
# define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b
#
#       WORD e_magic;
#       WORD e_cblp;
#       WORD e_cp;
#       WORD e_crlc;
#       WORD e_cparhdr;
#       WORD e_minalloc;
#       WORD e_maxalloc;
#       WORD e_ss;
#       WORD e_sp;
#       WORD e_csum;
#       WORD e_ip;
#       WORD e_cs;
#       WORD e_lfarlc;
#       WORD e_ovno;
#       WORD e_res[4];
#       WORD e_oemid;
#       WORD e_oeminfo;
#       WORD e_res2[10];
#       LONG e_lfanew;
# 
#  typedef struct _IMAGE_NT_HEADERS {
#       DWORD Signature;
#       IMAGE_FILE_HEADER FileHeader;
#       IMAGE_OPTIONAL_HEADER32 OptionalHeader;
# 
#  typedef struct _IMAGE_FILE_HEADER {
#       WORD Machine;
#       WORD NumberOfSections;
#       DWORD TimeDateStamp;
#       DWORD PointerToSymbolTable;
#       DWORD NumberOfSymbols;
#       WORD SizeOfOptionalHeader;
#       WORD Characteristics;
#
#  typedef struct _IMAGE_OPTIONAL_HEADER64 {
#       WORD Magic;
#       BYTE MajorLinkerVersion;
#       BYTE MinorLinkerVersion;
#       DWORD SizeOfCode;
#       DWORD SizeOfInitializedData;
#       DWORD SizeOfUninitializedData;
#       DWORD AddressOfEntryPoint;
#       DWORD BaseOfCode;
#       ULONGLONG ImageBase; -- or DWORD
#       DWORD SectionAlignment;
#       DWORD FileAlignment;
#       WORD MajorOperatingSystemVersion;
#       WORD MinorOperatingSystemVersion;
#       WORD MajorImageVersion;
#       WORD MinorImageVersion;
#       WORD MajorSubsystemVersion;
#       WORD MinorSubsystemVersion;
#       DWORD Win32VersionValue;
#       DWORD SizeOfImage;
#       DWORD SizeOfHeaders;
#       DWORD CheckSum;
#       WORD Subsystem;
#       WORD DllCharacteristics;
#       // plus some memory size based values...

def subsystem(data):

    dosheader = struct.unpack("<HHHHHHHHHHHHHH4HHH10HL", data[:64])
    e_magic = dosheader[0]
    e_lfanew = dosheader[-1]
    optionalsigpos = e_lfanew+24
    magic, = struct.unpack("<H", data[optionalsigpos:optionalsigpos+2])
    print(hex(magic))
    subsysoff = optionalsigpos + 64
    if magic == 0x20b:
        subsysoff += 4
    subsystem, = struct.unpack("<H", data[subsysoff:subsysoff+2])
    return subsysoff, subsystem

def main(exe, new, new_ss):
    with open(exe, 'rb') as f:
        data = f.read()
    print(len(data))
    off, ss = subsystem(data)
    print(off, ss)
    with open(new, 'wb') as f:
        f.write(data[:off])
        f.write(struct.pack("<H", new_ss))
        f.write(data[off+2:])

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], 2 if sys.argv[3] == "gui" else 3)
