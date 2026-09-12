"""Reject artifacts with the wrong architecture or non-system DLL dependencies."""

import re
import struct
import subprocess
import sys
from pathlib import Path

binary = Path(sys.argv[1])
arch = sys.argv[2]

if sys.platform == 'darwin':
    actual = subprocess.check_output(['lipo', '-archs', binary], text=True).strip()
    if actual != arch:
        sys.exit(f'Expected {arch}, got {actual}')
    output = subprocess.check_output(['otool', '-L', binary], text=True)
    print(output)
    # The first load command is the library's own install name.
    dependencies = [line.strip().split(' (', 1)[0] for line in output.splitlines()[2:]]
    unexpected = [dep for dep in dependencies
                  if not dep.startswith(('/usr/lib/', '/System/Library/'))]
elif sys.platform == 'win32':
    data = binary.read_bytes()
    pe_offset = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe_offset:pe_offset + 4] != b'PE\0\0':
        sys.exit('Not a PE binary')
    machine = struct.unpack_from('<H', data, pe_offset + 4)[0]
    if arch != 'x64' or machine != 0x8664:
        sys.exit(f'Unexpected PE machine type: {machine:#x}')
    output = subprocess.check_output(['dumpbin', '/dependents', binary], text=True)
    print(output)
    dependencies = re.findall(r'^\s+(\S+\.dll)\s*$', output, re.MULTILINE | re.IGNORECASE)
    system_dlls = {'advapi32.dll', 'bcrypt.dll', 'dwrite.dll', 'gdi32.dll',
                   'kernel32.dll', 'ole32.dll', 'shell32.dll', 'user32.dll',
                   'usp10.dll'}
    unexpected = [dep for dep in dependencies if dep.lower() not in system_dlls]
else:
    sys.exit('This check is for macOS and Windows artifacts')

if unexpected:
    sys.exit(f'Unexpected runtime dependencies: {unexpected}')
