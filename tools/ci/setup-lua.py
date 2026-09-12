"""Install LuaJIT and MoonScript for the native Aegisub wrapper tests in CI."""

import hashlib
import os
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path


def run(*args, **kwargs):
    subprocess.run(args, check=True, **kwargs)


temporary = Path(os.environ['RUNNER_TEMP'])
paths = []

if sys.platform == 'linux':
    run('sudo', 'apt-get', 'install', '-y', 'luajit', 'luarocks', 'liblua5.1-0-dev')
    run('sudo', 'luarocks', '--lua-version=5.1', 'install', 'moonscript', '0.6.0')
elif sys.platform == 'darwin':
    run('brew', 'install', 'luajit', 'luarocks')
    luajit = subprocess.check_output(['brew', '--prefix', 'luajit'], text=True).strip()
    tree = temporary / 'luarocks'
    run('luarocks', '--lua-version=5.1', f'--lua-dir={luajit}', f'--tree={tree}',
        'install', 'moonscript', '0.6.0')
    paths.append(tree / 'bin')
elif sys.platform == 'win32':
    luajit = temporary / 'luajit'
    run('git', 'clone', '--depth', '1', '--branch', 'v2.1',
        'https://github.com/LuaJIT/LuaJIT.git', luajit)
    run('cmd', '/c', 'msvcbuild.bat', cwd=luajit / 'src')
    paths.append(luajit / 'src')

    archive = temporary / 'moonscript.zip'
    urllib.request.urlretrieve(
        'https://github.com/leafo/moonscript/releases/download/v0.6.0/'
        'moonscript-v0.6.0-windows-x86_64.zip', archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != (
            'd91b2aec25caf71a551268c1d50d635708e556641ba0e19dd7f24d4a68d93e7c'):
        sys.exit('MoonScript archive checksum mismatch')
    with zipfile.ZipFile(archive) as compiler:
        compiler.extractall(temporary / 'moonscript')
    paths.append(temporary / 'moonscript')
else:
    sys.exit(f'Unsupported CI platform: {sys.platform}')

with open(os.environ['GITHUB_PATH'], 'a', encoding='utf-8') as github_path:
    for path in paths:
        github_path.write(f'{path}\n')
