"""Exercise real ad-hoc signing and the release flow without Apple credentials."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

binary = Path(sys.argv.pop(1)).resolve()
tools = Path(__file__).resolve().parents[1] / 'tools'


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='SubInspector signing ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.artifact = self.root / 'macOS artifact'
        self.artifact.mkdir()
        shutil.copy2(binary, self.artifact / 'libSubInspector.dylib')
        (self.artifact / 'COPYING').write_text('License fixture\n')
        with (self.artifact / 'SHA256SUMS').open('w') as checksums:
            for name in ['COPYING', 'libSubInspector.dylib']:
                digest = hashlib.sha256((self.artifact / name).read_bytes()).hexdigest()
                checksums.write(f'{digest}  {name}\n')
        self.env = dict(os.environ, SUBINSPECTOR_SIGNATURE='-',
                        SUBINSPECTOR_NOTARY_PROFILE='test-profile')
        self.commands = self.root / 'commands'
        self.commands.mkdir()
        # Never contact Apple, even if a validation regression reaches xcrun.
        (self.commands / 'xcrun').write_text('#!/bin/sh\necho "Unexpected notary call"\nexit 1\n')
        (self.commands / 'xcrun').chmod(0o755)
        self.env['PATH'] = str(self.commands) + os.pathsep + self.env['PATH']

    def run_tool(self, name, *args):
        return subprocess.run([tools / name, self.artifact, *args], env=self.env,
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def sign(self):
        result = self.run_tool('osx-sign.sh')
        self.assertEqual(result.returncode, 0, result.stdout)
        for line in (self.artifact / 'SHA256SUMS').read_text().splitlines():
            digest, name = line.split('  ', 1)
            self.assertEqual(digest, hashlib.sha256((self.artifact / name).read_bytes()).hexdigest())

    def test_sign_and_reject_adhoc_notarization(self):
        self.sign()
        output = self.root / 'release.zip'
        result = self.run_tool('osx-notarize.sh', output)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse(output.exists())
        self.assertNotIn('Unexpected notary call', result.stdout)

    def test_corrupt_download_is_not_signed(self):
        original = (self.artifact / 'libSubInspector.dylib').read_bytes()
        (self.artifact / 'COPYING').write_text('Changed after download')
        result = self.run_tool('osx-sign.sh')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(original, (self.artifact / 'libSubInspector.dylib').read_bytes())

    def test_notary_results(self):
        self.sign()
        # Only simulate Developer ID validation and Apple's service. ditto,
        # checksum verification, plist parsing, and output handling remain real.
        commands = self.commands
        (commands / 'codesign').write_text('''#!/bin/sh
echo 'Timestamp=Sep 12, 2026'
echo 'CodeDirectory v=20500 flags=0x10000(runtime)'
''')
        (commands / 'xcrun').write_text('''#!/usr/bin/env python3
import hashlib, os, plistlib, sys, zipfile
assert sys.argv[1] == 'notarytool'
assert sys.argv[sys.argv.index('--keychain-profile') + 1] == 'test-profile'
assert sys.argv[sys.argv.index('--keychain') + 1] == 'test keychain'
if sys.argv[2] == 'log':
    print('Notary service diagnostic')
else:
    assert sys.argv[2] == 'submit'
    assert sys.argv[sys.argv.index('--timeout') + 1] == '1m'
    archive = next(arg for arg in sys.argv if arg.endswith('.zip'))
    with zipfile.ZipFile(archive) as package:
        for line in package.read('macOS artifact/SHA256SUMS').decode().splitlines():
            digest, name = line.split('  ', 1)
            assert digest == hashlib.sha256(package.read('macOS artifact/' + name)).hexdigest()
    plistlib.dump({'id': 'test-submission', 'status': os.environ['TEST_STATUS']}, sys.stdout.buffer)
    sys.exit(int(os.environ['TEST_EXIT']))
''')
        for command in commands.iterdir():
            command.chmod(0o755)
        self.env.update(SUBINSPECTOR_NOTARY_KEYCHAIN='test keychain',
                        SUBINSPECTOR_NOTARY_TIMEOUT='1m')
        for status, exit_code in [('Accepted', '0'), ('Invalid', '0'), ('In Progress', '1')]:
            with self.subTest(status=status):
                self.env.update(TEST_STATUS=status, TEST_EXIT=exit_code)
                output = self.root / f'{status}.zip'
                result = self.run_tool('osx-notarize.sh', output)
                accepted = status == 'Accepted'
                self.assertEqual(result.returncode == 0, accepted, result.stdout)
                self.assertEqual(output.exists(), accepted)
                if accepted:
                    original = output.read_bytes()
                    self.assertNotEqual(self.run_tool('osx-notarize.sh', output).returncode, 0)
                    self.assertEqual(original, output.read_bytes())
                else:
                    self.assertIn('Notary service diagnostic', result.stdout)


unittest.main()
