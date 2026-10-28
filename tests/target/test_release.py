#!/usr/bin/env python3
"""Passive release acceptance; cold boot is established separately by UART/power cycle."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import socket
import struct
import sys
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/vitis'))
from boot_common import load_bundle


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host', default='192.168.0.10')
    p.add_argument('--os', choices=['neutrino', 'freertos'], default='neutrino')
    p.add_argument('--bundle', type=Path)
    p.add_argument('--config-reference', type=Path)
    p.add_argument('--allow-added-config', action='store_true')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    report = {'date': datetime.now(timezone.utc).isoformat(), 'expected_os': a.os, 'checks': []}
    def save():
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
    def passed(name):
        report['checks'].append(name)
        save()
        print('PASS:', name, flush=True)
    def get(route, **query):
        with http.open('http://' + a.host + route + ('?' + urllib.parse.urlencode(query) if query else ''), timeout=900) as r:
            return r.read()
    def obj(route, **query): return json.loads(get(route, **query))
    report['runtime'] = obj('/api/runtime')
    assert report['runtime']['os'] == a.os
    assert all(report['runtime']['capabilities'].values())
    assert all(report['runtime']['readiness'].values())
    passed('runtime/capabilities/readiness')
    report['version'] = obj('/api/version')
    report['network'] = obj('/api/net')
    for bus in ('i2c', 'mdio'):
        data = obj('/api/ui/bus', bus=bus)
        assert data['ready'] and data['devices']
    passed('I2C and MDIO')
    for route in ('/', '/js/main.js'):
        assert get(route)
    passed('shared web resources')
    with socket.create_connection((a.host, 8889), timeout=5) as s:
        s.sendall(b'DCP2' + struct.pack('>HHBBH', 2, 4, 0, 0, 1))
        data = b''
        while len(data) < 14:
            part = s.recv(14 - len(data))
            assert part
            data += part
        assert data[:4] == b'DCP2' and data[-2:] == b'\0\0'
    passed('DCP2 PING')
    hashes = {}
    def walk(path):
        offset = 0
        while True:
            result = obj('/api/ui/files', volume='flash', path=path, offset=offset, limit=128)
            assert result['ok']
            for entry in result['entries']:
                child = path + '/' + entry['name']
                if entry['directory']: walk(child)
                else: hashes[child[len('/config/'):].casefold()] = hashlib.sha256(get('/api/ui/file', volume='flash', path=child)).hexdigest()
            if result['next'] is None: break
            offset = result['next']
    walk('/config')
    report['config_sha256'] = hashes
    save()
    if a.config_reference:
        reference = json.loads(a.config_reference.read_text())
        reference = reference.get('sha256', reference.get('config_sha256', reference))
        reference = {name.casefold(): value for name, value in reference.items()}
        report['added_config'] = sorted(hashes.keys() - reference.keys())
        save()
        assert all(hashes.get(name) == digest for name, digest in reference.items()), 'configuration changed or removed'
        assert a.allow_added_config or not report['added_config'], 'configuration files added'
    passed('configuration snapshot/preservation')
    if a.bundle:
        manifest = load_bundle(a.bundle, target=a.os, medium='sd')
        remote = hashlib.sha256(get('/api/ui/file', volume='sd', path='/BOOT.bin')).hexdigest()
        assert remote == manifest['image']['sha256']
        report['sd_boot_sha256'] = remote
        passed('SD BOOT.bin matches validated bundle')
    try:
        report['sd_pl'] = obj('/api/ui/sd-pl')
    except urllib.error.HTTPError as error:
        if error.code != 504: raise
        report['sd_pl'] = {'status': 504, 'reason': 'SD-PL probe timed out'}
    report['cold_boot'] = 'requires independent UART/power-cycle evidence'
    save()


if __name__ == '__main__': main()
