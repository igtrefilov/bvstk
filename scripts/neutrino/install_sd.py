#!/usr/bin/env python3
"""Install a validated Neutrino BOOT.bin on an existing PS SD FAT volume.

Uses the running BVSTK HTTP API. Never formats media or writes QSPI.
Existing profiles are retained; --resources updates web assets and adds only
missing profile files. Backups and readback evidence remain on the host.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import secrets
import sys
import tarfile
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'vitis'))
from boot_common import ROOT, load_bundle, sha256


class Device:
    def __init__(self, host):
        self.base = 'http://' + host
        self.http = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def request(self, route, method='GET', data=None, **query):
        url = self.base + route + ('?' + urllib.parse.urlencode(query) if query else '')
        with self.http.open(urllib.request.Request(url, data=data, method=method), timeout=900) as response:
            return response.read()

    def get(self, volume, path):
        try:
            return self.request('/api/ui/file', volume=volume, path=path)
        except urllib.error.HTTPError as error:
            if error.code == 404:
                return None
            raise

    def put(self, volume, path, data, overwrite=False):
        result = json.loads(self.request('/api/ui/file', 'PUT', data, volume=volume,
            path=path, confirm='true', overwrite=str(overwrite).lower()))
        if not result.get('ok') or result.get('bytes') != len(data):
            raise ValueError('Incomplete upload: ' + path)
        if self.get(volume, path) != data:
            raise ValueError('Readback mismatch: ' + path)

    def action(self, volume, path, action, **query):
        result = json.loads(self.request('/api/ui/files', 'PUT', b'', volume=volume,
            path=path, action=action, confirm='true', **query))
        if not result.get('ok'):
            raise ValueError('Filesystem operation failed: ' + path)

    def mkdirs(self, volume, path):
        parts = Path(path).parent.parts
        for count in range(2, len(parts) + 1):
            directory = '/'.join(parts[:count]).replace('//', '/')
            try:
                self.action(volume, directory, 'mkdir')
            except urllib.error.HTTPError as error:
                # Verify that an existing entry really is a directory.
                info = json.loads(self.request('/api/ui/files', volume=volume, path=directory, action='stat'))
                if info.get('type') not in ('directory', 'dir') and not info.get('directory'):
                    raise error


def resources(path):
    with tarfile.open(fileobj=path) if hasattr(path, 'read') else tarfile.open(path) as archive:
        for member in archive:
            parts = Path(member.name).parts
            if not member.isfile() or not parts or parts[0] not in ('config', 'www') or '..' in parts or member.name.startswith('/'):
                raise ValueError('Invalid resource archive member: ' + member.name)
            yield member.name, archive.extractfile(member).read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='192.168.0.10')
    parser.add_argument('--bundle', type=Path, default=ROOT / 'artifacts/boot/neutrino')
    parser.add_argument('--resources', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    manifest = load_bundle(args.bundle, target='neutrino', medium='sd')
    image = (args.bundle / 'BOOT.bin').read_bytes()
    resource_bytes = (args.bundle / 'resources.tar').read_bytes()
    if hashlib.sha256(image).hexdigest() != manifest['files']['BOOT.bin'] or hashlib.sha256(resource_bytes).hexdigest() != manifest['files']['resources.tar']:
        raise ValueError('Boot bundle changed while reading installation inputs')
    files = list(resources(io.BytesIO(resource_bytes)))
    if args.dry_run:
        print(json.dumps({'target': 'neutrino', 'medium': 'sd', 'boot_bytes': len(image),
            'sha256': hashlib.sha256(image).hexdigest(), 'resources': len(files), 'qspi_writes': False}, indent=2))
        return
    # Path concatenation is explicit to keep all evidence under one private root.
    evidence = args.output or ROOT / 'artifacts/boot/sd' / (datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ-') + secrets.token_hex(4))
    evidence.mkdir(mode=0o700, parents=True, exist_ok=False)
    os.chmod(evidence, 0o700)
    device = Device(args.host)
    volumes = json.loads(device.request('/api/ui/volumes'))['volumes']
    sd = next(v for v in volumes if v['name'] == 'sd')
    if not sd.get('ready') or sd.get('free_bytes', 0) < len(image):
        raise ValueError('PS SD is unavailable or has insufficient free space')
    previous = device.get('sd', '/BOOT.bin')
    if previous is not None:
        (evidence / 'BOOT.previous.bin').write_bytes(previous)
    token = secrets.token_hex(8)
    temporary, backup = '/.bvstk-boot-' + token, '/BOOT.previous-' + token + '.bin'
    report = {'target': 'neutrino', 'medium': 'sd', 'manifest': manifest,
        'qspi_writes': False, 'boot_sha256': sha256(args.bundle / 'BOOT.bin'),
        'previous_sha256': hashlib.sha256(previous).hexdigest() if previous is not None else None,
        'remote_backup': backup if previous is not None else None, 'resources': []}
    (evidence / 'installation.json').write_text(json.dumps(report, indent=2) + '\n')
    moved = False
    print('Uploading temporary SD image; previous boot is backed up on host.', flush=True)
    try:
        device.put('sd', temporary, image)
        if previous is not None:
            device.action('sd', '/BOOT.bin', 'rename', dst_volume='sd', dst=backup)
            moved = True
        device.action('sd', temporary, 'rename', dst_volume='sd', dst='/BOOT.bin')
        if device.get('sd', '/BOOT.bin') != image:
            raise ValueError('BOOT.bin final readback mismatch')
        report['boot_readback'] = 'passed'
        print('Final SD BOOT.bin readback passed.', flush=True)
        if args.resources:
            for name, data in files:
                path = '/' + name
                old = device.get('flash', path)
                state = 'unchanged' if old == data else 'preserved' if old is not None and name.startswith('config/') else 'installed'
                if state == 'installed':
                    saved = evidence / 'resources-before' / name
                    if old is not None:
                        saved.parent.mkdir(parents=True, exist_ok=True)
                        saved.write_bytes(old)
                    device.mkdirs('flash', path)
                    device.put('flash', path, data, overwrite=old is not None)
                report['resources'].append({'path': name, 'result': state})
        report['result'] = 'installed; cold boot requires SD selection on J13 and power cycle'
    except Exception:
        report['result'] = 'failed; retain evidence and remote backup'
        if moved and device.get('sd', '/BOOT.bin') is None:
            device.action('sd', backup, 'rename', dst_volume='sd', dst='/BOOT.bin')
            report['rollback'] = 'previous boot restored'
        raise
    finally:
        (evidence / 'installation.json').write_text(json.dumps(report, indent=2) + '\n')
    print('SD BOOT.bin installed and verified. QSPI unchanged.')
    print('Select SD boot on J13 and power-cycle for cold-boot acceptance.')
    print('Evidence:', evidence)


if __name__ == '__main__':
    main()
