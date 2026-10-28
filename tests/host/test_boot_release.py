#!/usr/bin/env python3
"""Boot-medium isolation, manifest integrity and rollback of SD installation."""
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'scripts/neutrino'), str(ROOT / 'scripts/vitis'), str(ROOT / 'tests/host')]
import boot_common
import install_sd
import run_qspi
from test_boot_image import boot_image


class ReleaseTests(unittest.TestCase):
    def test_sd_size_does_not_relax_qspi_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'BOOT.bin'
            image = boot_image()
            image.extend(b'\0' * (boot_common.layout()['fs_base'] + 1 - len(image)))
            path.write_bytes(image)
            self.assertGreater(boot_common.validate_image(path, medium='sd')['size'], boot_common.layout()['fs_base'])
            with self.assertRaisesRegex(ValueError, 'overlaps'):
                boot_common.validate_image(path)
            with self.assertRaises(ValueError):
                boot_common.validate_image(path, medium='invalid')

    def test_neutrino_payload_must_match_raw_ifs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = boot_image()
            for name in ('fsbl.elf', 'qspi_read.elf', 'ps7_init.tcl', 'design.bit', 'ifs.elf', 'design.xsa', 'boot.bif', 'resources.tar'):
                (root / name).write_bytes(b'fixture')
            (root / 'BOOT.bin').write_bytes(image)
            (root / 'ifs.raw').write_bytes(image[0x3000:])
            manifest = {'format': 1, 'target': 'neutrino', 'medium': 'sd', 'qspi': boot_common.layout(),
                        'image': boot_common.validate_image(root / 'BOOT.bin', medium='sd'),
                        'files': {p.name: boot_common.sha256(p) for p in root.iterdir()}}
            (root / 'manifest.json').write_text(json.dumps(manifest))
            boot_common.load_bundle(root, target='neutrino', medium='sd')
            with self.assertRaises(ValueError): boot_common.load_bundle(root, medium='qspi')
            (root / 'ifs.raw').write_bytes(b'other-ifs')
            manifest['files']['ifs.raw'] = boot_common.sha256(root / 'ifs.raw')
            (root / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'IFS payload'):
                boot_common.load_bundle(root)

    def test_legacy_volume_requires_verified_new_fat(self):
        before = bytearray(32 * 1024 * 1024)
        old, new = 8 * 1024 * 1024, 16 * 1024 * 1024
        geometry = {'fs_base': new}
        before[old + 510:old + 512] = b'\x55\xaa'
        with self.assertRaisesRegex(ValueError, 'Old 8 MiB FAT'):
            run_qspi.verify_migrated_volume(before, geometry, '')
        before[new + 510:new + 512] = b'\x55\xaa'
        digest = __import__('hashlib').sha256(before[new:]).hexdigest()
        with self.assertRaisesRegex(ValueError, 'Old 8 MiB FAT'):
            run_qspi.verify_migrated_volume(before, geometry, '0' * 64)
        run_qspi.verify_migrated_volume(before, geometry, digest)
        before[old + 510:old + 512] = b'\0\0'
        run_qspi.verify_migrated_volume(before, geometry, '')

    def test_qspi_programmer_rejects_sd_before_hardware(self):
        with patch.object(sys, 'argv', ['run_qspi.py']), patch.object(run_qspi, 'load_bundle', return_value={'medium': 'sd'}), patch.object(run_qspi, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'SD bundle'):
                run_qspi.main()
            run.assert_not_called()

    def test_archive_rejects_traversal_and_links(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'resources.tar'
            for name, kind in [('config/../evil', tarfile.REGTYPE), ('www/link', tarfile.SYMTYPE), ('/www/evil', tarfile.REGTYPE)]:
                with tarfile.open(path, 'w') as archive:
                    item = tarfile.TarInfo(name)
                    item.type = kind
                    archive.addfile(item, io.BytesIO())
                with self.assertRaises(ValueError):
                    list(install_sd.resources(path))

    def install(self, fail=False):
        class Device:
            files = {('sd', '/BOOT.bin'): b'old-boot', ('flash', '/config/device.json'): b'user-profile'}
            fail_commit = fail
            def __init__(self, host): pass
            def request(self, route):
                return json.dumps({'volumes': [{'name': 'sd', 'ready': True, 'free_bytes': 1024}]}).encode()
            def get(self, volume, path): return self.files.get((volume, path))
            def put(self, volume, path, data, overwrite=False): self.files[volume, path] = data
            def mkdirs(self, *args): pass
            def action(self, volume, path, action, **query):
                dst = query['dst']
                if self.fail_commit and dst == '/BOOT.bin':
                    self.fail_commit = False
                    raise OSError('rename interrupted')
                self.files[volume, dst] = self.files.pop((volume, path))
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory)
            (bundle / 'BOOT.bin').write_bytes(b'new-boot')
            (bundle / 'resources.tar').touch()
            output = bundle / 'evidence'
            argv = ['install_sd.py', '--bundle', directory, '--output', str(output), '--resources']
            with patch.object(sys, 'argv', argv), patch.object(install_sd, 'Device', Device), patch.object(install_sd, 'load_bundle', return_value={'files': {'BOOT.bin': boot_common.sha256(bundle / 'BOOT.bin'), 'resources.tar': boot_common.sha256(bundle / 'resources.tar')}}), patch.object(install_sd, 'resources', return_value=[('config/device.json', b'default'), ('www/index.html', b'web')]):
                if fail:
                    with self.assertRaisesRegex(OSError, 'interrupted'):
                        install_sd.main()
                else:
                    install_sd.main()
            report = json.loads((output / 'installation.json').read_text())
            self.assertEqual(Device.files['flash', '/config/device.json'], b'user-profile')
            self.assertEqual((output / 'BOOT.previous.bin').read_bytes(), b'old-boot')
            self.assertFalse(report['qspi_writes'])
            self.assertEqual(Device.files['sd', '/BOOT.bin'], b'old-boot' if fail else b'new-boot')
            if fail:
                self.assertEqual(report['rollback'], 'previous boot restored')
            else:
                self.assertEqual(Device.files['sd', report['remote_backup']], b'old-boot')
                self.assertEqual(Device.files['flash', '/www/index.html'], b'web')

    def test_install_preserves_profiles_and_previous_boot(self): self.install()
    def test_failed_commit_restores_previous_boot(self): self.install(fail=True)


if __name__ == '__main__': unittest.main()
