"""Collect a tested binary, its API/wrapper, and sources for rebuilding it."""

import configparser
import hashlib
import shutil
import sys
from pathlib import Path

build = Path(sys.argv[1])
binary = build / 'src' / sys.argv[2]
output = build / 'artifact'
output.mkdir()

sources = list((build / 'meson-dist').glob('*.tar.xz'))
if len(sources) != 1:
    sys.exit('Expected exactly one source archive with bundled dependencies')
for path in [binary, Path('COPYING'), Path('src/SubInspector.h'),
             Path('examples/Aegisub/Inspector.moon'), sources[0]]:
    shutil.copy2(path, output / path.name)
shutil.copy2(build / 'meson-info/intro-buildoptions.json', output / 'build-options.json')

licenses = output / 'licenses'
for wrap in Path('subprojects').glob('*.wrap'):
    config = configparser.ConfigParser(interpolation=None)
    config.read(wrap)
    source = wrap.parent / config['wrap-file']['directory']
    destination = licenses / wrap.stem
    destination.mkdir(parents=True)
    for name in ['COPYING', 'LICENSE', 'LICENSE.TXT', 'docs/FTL.TXT']:
        path = source / name
        if path.is_file():
            shutil.copy2(path, destination / path.name)
    if not any(destination.iterdir()):
        sys.exit(f'No license found for {wrap.stem}')

(output / 'NOTICE.txt').write_text(
    'This software uses the FreeType library, copyright The FreeType Project\n'
    '(www.freetype.org). All rights reserved.\n\n'
    'Dependency licenses are in licenses/. The source archive includes the\n'
    'patched dependency sources and build configuration for rebuilding or\n'
    'relinking this library. The CI workflow records the build commands.\n',
    encoding='utf-8',
)
with (output / 'SHA256SUMS').open('w', encoding='utf-8') as checksums:
    for path in sorted(output.rglob('*')):
        if path.is_file() and path.name != 'SHA256SUMS':
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            checksums.write(f'{digest}  {path.relative_to(output).as_posix()}\n')
