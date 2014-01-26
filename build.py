import base64

MARKER = ('X' * 100).encode('utf-16le')

def main():
    with open('stub.exe', 'rb') as f:
        stub = f.read()
    if MARKER not in stub:
        raise ValueError("Invalid stub: no marker bytes")
    stub_txt = base64.b64encode(stub).decode('ASCII')
    with open('template.py') as template:
        script = template.read()
        script = script.replace('%%STUB%%', stub_txt)
    with open('mkwrapper.py', 'w') as scriptfile:
        scriptfile.write(script)

if __name__ == '__main__':
    main()
