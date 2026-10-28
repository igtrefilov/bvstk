#!/usr/bin/env python3
"""Exercise QSPI migration ordering and full-chip rollback without hardware."""
import json
import shutil
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/vitis'))
package = types.ModuleType('pyfatfs')
module = types.ModuleType('pyfatfs.PyFatFS')
module.PyFatFS = object
with patch.dict(sys.modules, {'pyfatfs': package, 'pyfatfs.PyFatFS': module}):
    import migrate_qspi16 as migration


class MigrationTests(unittest.TestCase):
    def run_case(self, case):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            before = b'\x11' * (32 * 1024 * 1024)
            fat = b'\x22' * (16 * 1024 * 1024)
            snapshot, volume, manifest = root / 'before.bin', root / 'fat.bin', root / 'manifest.json'
            snapshot.write_bytes(before); volume.write_bytes(fat)
            manifest.write_text(json.dumps({'device.json': 'expected'}))
            output = root / 'report'
            bundle = root / 'bundle'
            bundle.mkdir()
            for name in ('BOOT.bin', 'fsbl.elf', 'qspi_read.elf', 'ps7_init.tcl', 'manifest.json'):
                (bundle / name).write_bytes(b'fixture')
            bundle_info = {'image': {'sha256': 'fixture'}, 'files': {name: 'fixture' for name in ('BOOT.bin', 'fsbl.elf', 'qspi_read.elf', 'ps7_init.tcl')}}
            state = bytearray(before)
            actions = []
            if case == 'changed': state[-1] ^= 1
            original_copy = shutil.copy2
            def copy_and_change(source, destination):
                result = original_copy(source, destination)
                if case == 'changed_input' and Path(destination).name == 'new-fat.img':
                    with open(destination, 'r+b') as stream: stream.write(b'changed')
                return result
            def fake_run(command, **kwargs):
                command = list(map(str, command)); actions.append(command[0])
                if command[0] == 'xsct':
                    Path(command[-2]).write_bytes(state)
                elif command[0] == 'program_flash':
                    source = Path(command[command.index('-f') + 1]).read_bytes()
                    offset = int(command[command.index('-offset') + 1], 0)
                    state[offset:offset + len(source)] = source
                elif command[0] == sys.executable:
                    if case == 'failed_boot': raise OSError('simulated boot failure')
                else: raise AssertionError(command)
            args = ['migration', '--snapshot', str(snapshot), '--volume', str(volume),
                    '--manifest', str(manifest), '--bundle', str(bundle), '--output', str(output)]
            if case != 'dry': args.append('--execute')
            with patch.object(sys, 'argv', args), patch.object(migration, 'run', fake_run), \
                 patch.object(migration, 'files_in_image', return_value={'device.json': 'expected'}), \
                 patch.object(migration, 'load_bundle', return_value=bundle_info), \
                 patch.object(migration, 'require_tools'), patch.object(migration.subprocess, 'run'), \
                 patch.object(migration.shutil, 'copy2', side_effect=copy_and_change):
                if case in ('failed_boot', 'changed', 'changed_input'):
                    with self.assertRaises((OSError, ValueError)): migration.main()
                else: migration.main()
            report = json.loads((output / 'migration.json').read_text())
            if case == 'failed_boot':
                self.assertEqual(state, before)
                self.assertEqual(report['rollback'], 'entire original QSPI restored and verified')
                self.assertEqual(actions.count('program_flash'), 2)
            elif case in ('changed', 'changed_input'):
                self.assertNotIn('program_flash', actions)
            elif case == 'dry':
                self.assertEqual(actions, [])
            else:
                self.assertEqual(state[:migration.NEW_BASE], before[:migration.NEW_BASE])
                self.assertEqual(state[migration.NEW_BASE:], fat)
                self.assertEqual(report['volume_readback'], 'passed')
                self.assertEqual(actions, ['xsct', 'program_flash', 'xsct', sys.executable])

    def test_dry_run_does_not_touch_hardware(self): self.run_case('dry')
    def test_success_verifies_volume_before_boot(self): self.run_case('success')
    def test_changed_input_refuses_to_program(self): self.run_case('changed_input')
    def test_changed_chip_refuses_to_program(self): self.run_case('changed')
    def test_boot_failure_restores_full_snapshot(self): self.run_case('failed_boot')


if __name__ == '__main__': unittest.main()
