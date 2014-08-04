import sys
import base64
import argparse

def parse_args():
    parser = argparse.ArgumentParser(description="Create executable shims")
    parser.add_argument("-f", "--filename", default="shim.exe",
            help="The filename of the generated shim")
    parser.add_argument("-c", "--command",
            help="The command to run (use %s for where the args should go)")
    parser.add_argument("--stub",
            help="The name of the stub executable")
    args = parser.parse_args()
    if len(args.command) >= 100:
        raise ValueError("The command cannot be over 100 characters long")
    return args

def main():
    args = parse_args()
    if args.stub:
        with open(args.stub, "rb") as f:
            stub_bytes = f.read()
    else:
        stub_bytes = base64.b64decode(stub)
    marker = ('X' * 100).encode('utf-16le')
    cmd = (args.command + ('\0' * 100))[:100]
    i = stub_bytes.index(marker)
    cmd_bytes = cmd.encode('utf-16le')
    with open(args.filename, "wb") as f:
        f.write(stub_bytes[:i])
        f.write(cmd_bytes)
        f.write(stub_bytes[i+len(marker):])

stub = """\
%%STUB%%
"""

if __name__ == '__main__':
    main()
