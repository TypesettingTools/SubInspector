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
        (self.artifact / 'licenses/libass').mkdir(parents=True)
        (self.artifact / 'licenses/libass/COPYING').write_text('Nested license fixture\n')
        with (self.artifact / 'SHA256SUMS').open('w') as checksums:
            for name in ['COPYING', 'libSubInspector.dylib', 'licenses/libass/COPYING']:
                digest = hashlib.sha256((self.artifact / name).read_bytes()).hexdigest()
                checksums.write(f'{digest}  {name}\n')
        self.env = dict(os.environ, SUBINSPECTOR_SIGNATURE='-',
                        SUBINSPECTOR_TEAM_ID='TESTTEAM01',
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
        checksums = (self.artifact / 'SHA256SUMS').read_bytes()
        (self.artifact / 'COPYING').write_text('Changed after download')
        result = self.run_tool('osx-sign.sh')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(original, (self.artifact / 'libSubInspector.dylib').read_bytes())
        self.assertEqual(checksums, (self.artifact / 'SHA256SUMS').read_bytes())

    def test_wrong_platform_is_rejected(self):
        manifest = self.artifact / 'SHA256SUMS'
        manifest.write_text(manifest.read_text().replace('libSubInspector.dylib', 'SubInspector.dll'))
        for script, args in [('osx-sign.sh', []), ('osx-notarize.sh', [self.root / 'release.zip'])]:
            with self.subTest(script=script):
                result = self.run_tool(script, *args)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn('Expected a macOS CI artifact', result.stdout)
                self.assertNotIn('Unexpected notary call', result.stdout)

    def simulate_notary(self):
        self.sign()
        # Only simulate Developer ID validation and Apple's service. ditto,
        # checksum verification, plist parsing, and output handling remain real.
        commands = self.commands
        (commands / 'codesign').write_text('''#!/usr/bin/env python3
import sys
if '--verify' in sys.argv:
    requirement = next(arg for arg in sys.argv if arg.startswith('-R='))
    if 'certificate leaf[subject.OU] = "TESTTEAM01"' not in requirement:
        sys.exit('Wrong signing team')
else:
    print('Timestamp=Sep 12, 2026')
    print('CodeDirectory v=20500 flags=0x10000(runtime)')
''')
        (commands / 'xcrun').write_text('''#!/usr/bin/env python3
import hashlib, os, pathlib, plistlib, sys, zipfile
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
        prefix = os.environ['TEST_PACKAGE_NAME'] + '/'
        for line in package.read(prefix + 'SHA256SUMS').decode().splitlines():
            digest, name = line.split('  ', 1)
            assert digest == hashlib.sha256(package.read(prefix + name)).hexdigest()
    if os.environ.get('TEST_CONCURRENT_OUTPUT'):
        pathlib.Path(os.environ['TEST_CONCURRENT_OUTPUT']).write_text('Created during notarization')
    plistlib.dump({'id': 'test-submission', 'status': os.environ['TEST_STATUS']}, sys.stdout.buffer)
    sys.exit(int(os.environ['TEST_EXIT']))
''')
        for command in commands.iterdir():
            command.chmod(0o755)
        self.env.update(SUBINSPECTOR_NOTARY_KEYCHAIN='test keychain',
                        SUBINSPECTOR_NOTARY_TIMEOUT='1m')

    def test_notary_results(self):
        self.simulate_notary()
        for status, exit_code in [('Accepted', '0'), ('Invalid', '0'), ('In Progress', '1')]:
            with self.subTest(status=status):
                self.env.update(TEST_STATUS=status, TEST_EXIT=exit_code, TEST_PACKAGE_NAME=status)
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

    def test_wrong_team_is_rejected_before_submission(self):
        self.simulate_notary()
        self.env['SUBINSPECTOR_TEAM_ID'] = 'OTHERTEAM1'
        result = self.run_tool('osx-notarize.sh', self.root / 'wrong-team.zip')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('Wrong signing team', result.stdout)
        self.assertFalse((self.root / 'wrong-team.zip').exists())

    def test_output_created_during_notarization_is_preserved(self):
        self.simulate_notary()
        output = self.root / 'concurrent.zip'
        self.env.update(TEST_STATUS='Accepted', TEST_EXIT='0',
                        TEST_PACKAGE_NAME='concurrent', TEST_CONCURRENT_OUTPUT=str(output))
        result = self.run_tool('osx-notarize.sh', output)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('appeared during notarization', result.stdout)
        self.assertEqual(output.read_text(), 'Created during notarization')


unittest.main()
