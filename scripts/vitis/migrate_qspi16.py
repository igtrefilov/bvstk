#!/usr/bin/env python3
"""Verify and migrate AX7020 QSPI from 8/24 MiB to 16/16 MiB.

Requires a fresh full-chip JTAG snapshot, a previously backed-up file tree,
its SHA-256 manifest, and a FAT image restored from that tree. No hardware
is touched unless --execute is specified. The pre-migration chip image is
retained and an attempted rollback restores it if a later operation fails.
"""
import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys

from boot_common import ROOT, SCRIPT, layout, load_bundle, require_tools, run, sha256, tool_env
from pyfatfs.PyFatFS import PyFatFS

OLD_BASE = 8 * 1024 * 1024
NEW_BASE = 16 * 1024 * 1024


def files_in_image(path, offset=0):
    with PyFatFS(str(path), offset=offset, read_only=True, preserve_case=True) as fs:
        result = {}
        for name in fs.walk.files('/'):
            key = name.lstrip('/').casefold()
            if key in result: raise ValueError('Ambiguous filenames in FAT image: ' + name)
            result[key] = hashlib.sha256(fs.readbytes(name)).hexdigest()
        return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--snapshot', type=Path, required=True)
    p.add_argument('--volume', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--bundle', type=Path, default=ROOT / 'artifacts/boot/neutrino-qspi')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--execute', action='store_true')
    a = p.parse_args()
    a.snapshot, a.volume, a.bundle, a.output = (x.resolve() for x in (a.snapshot, a.volume, a.bundle, a.output))
    geometry = layout()
    if geometry['fs_base'] != NEW_BASE or geometry['flash_size'] != 2 * NEW_BASE:
        raise ValueError('Current firmware contract must reserve 16 MiB and provide 16 MiB FAT')
    bundle = load_bundle(a.bundle, target='neutrino', medium='qspi')
    before = a.snapshot.read_bytes()
    if len(before) != geometry['flash_size'] or a.volume.stat().st_size != NEW_BASE:
        raise ValueError('Incomplete chip snapshot or wrong FAT volume size')
    expected = json.loads(a.manifest.read_text())
    if files_in_image(a.snapshot, OLD_BASE) != expected or files_in_image(a.volume) != expected:
        raise ValueError('Snapshot, restored volume and file backup do not match')
    subprocess.run(['fsck.fat', '-n', str(a.volume)], check=True, stdout=subprocess.DEVNULL)
    volume_hash = sha256(a.volume)
    report = {'source_snapshot': str(a.snapshot), 'source_sha256': sha256(a.snapshot),
              'volume_sha256': volume_hash, 'files': len(expected),
              'boot_sha256': bundle['image']['sha256'], 'geometry': geometry,
              'result': 'validated; no hardware changes'}
    a.output.mkdir(mode=0o700, parents=True, exist_ok=False)
    os.chmod(a.output, 0o700)
    def save(): (a.output / 'migration.json').write_text(json.dumps(report, indent=2) + '\n')
    save()
    if not a.execute:
        print(json.dumps(report, indent=2)); return
    # Freeze all inputs in the private evidence directory. A concurrent build
    # or backup refresh cannot change the data between verification and flash.
    frozen = a.output / 'bundle'
    frozen.mkdir(mode=0o700)
    for name in list(bundle['files']) + ['manifest.json']:
        shutil.copy2(a.bundle / name, frozen / name)
    if load_bundle(frozen, target='neutrino', medium='qspi') != bundle:
        raise ValueError('Boot bundle changed while copying')
    frozen_snapshot = a.output / 'original-qspi.bin'
    frozen_volume = a.output / 'new-fat.img'
    shutil.copy2(a.snapshot, frozen_snapshot)
    shutil.copy2(a.volume, frozen_volume)
    if sha256(frozen_snapshot) != report['source_sha256'] or sha256(frozen_volume) != volume_hash:
        raise ValueError('Snapshot or FAT image changed while copying')
    a.bundle, a.snapshot, a.volume = frozen, frozen_snapshot, frozen_volume
    env = tool_env()
    require_tools(['xsct', 'program_flash'], env)
    url = os.environ.get('HW_SERVER_URL', 'tcp:127.0.0.1:3121')
    snapshot_command = ['xsct', SCRIPT / 'qspi_snapshot.tcl', a.bundle / 'ps7_init.tcl', a.bundle / 'qspi_read.elf']
    current = a.output / 'qspi-prewrite.bin'
    run(snapshot_command + [current, url], cwd=a.output, env=env, log=a.output / 'prewrite.log', timeout=600)
    if sha256(current) != report['source_sha256']:
        raise ValueError('QSPI changed since the snapshot; take a new backup')
    program = ['program_flash', '-flash_type', 'qspi-x4-single', '-fsbl', a.bundle / 'fsbl.elf', '-blank_check', '-verify', '-url', url]
    attempted = False
    try:
        attempted = True
        report['result'] = 'programming new FAT volume; original snapshot retained'
        save()
        run(program + ['-f', a.volume, '-offset', '0x01000000'], cwd=a.output, env=env, log=a.output / 'volume-program.log', timeout=1200)
        after_fat = a.output / 'qspi-after-volume.bin'
        run(snapshot_command + [after_fat, url], cwd=a.output, env=env, log=a.output / 'volume-readback.log', timeout=600)
        after = after_fat.read_bytes()
        if len(after) != len(before) or after[:NEW_BASE] != before[:NEW_BASE] or hashlib.sha256(after[NEW_BASE:]).hexdigest() != volume_hash:
            raise ValueError('FAT readback differs or bytes below the new FAT were modified')
        report['volume_readback'] = 'passed'
        report['result'] = 'volume verified; programming boot image'
        save()
        boot_env = env.copy()
        boot_env['BOOT_OUTPUT_DIR'] = str(a.bundle)
        boot_env['QSPI_MIGRATED_VOLUME_SHA256'] = volume_hash
        run([sys.executable, SCRIPT / 'run_qspi.py', '--target', 'neutrino'], cwd=ROOT, env=boot_env, log=a.output / 'boot-program.log', timeout=1800)
        report['result'] = 'volume and boot programmed and independently read back'
    except Exception as exc:
        report['result'] = 'failed: ' + str(exc)
        if attempted:
            try:
                run(program + ['-f', a.snapshot, '-offset', '0x00000000'], cwd=a.output, env=env, log=a.output / 'rollback-program.log', timeout=1800)
                restored = a.output / 'qspi-after-rollback.bin'
                run(snapshot_command + [restored, url], cwd=a.output, env=env, log=a.output / 'rollback-readback.log', timeout=600)
                if sha256(restored) != report['source_sha256']: raise ValueError('Rollback readback mismatch')
                report['rollback'] = 'entire original QSPI restored and verified'
            except Exception as rollback:
                report['rollback'] = 'FAILED: ' + str(rollback) + '; retain original snapshot for recovery'
        save(); raise
    save()
    print('QSPI 16/16 MiB programmed; select QSPI boot and power-cycle.')
    print('Report:', a.output / 'migration.json')


if __name__ == '__main__':
    main()
