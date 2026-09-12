"""Stage a normal Aegisub include directory and run the LuaJIT wrapper tests."""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

luajit, wrapper, binary, tests = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix='SubInspector wrapper ') as directory:
    include = Path(directory) / 'include'
    libraries = include / 'SubInspector' / 'Inspector'
    libraries.mkdir(parents=True)
    (include / 'fonts').mkdir()
    shutil.copy2(binary, libraries / Path(binary).name)
    subprocess.run([luajit, tests, wrapper, str(include)], check=True)
