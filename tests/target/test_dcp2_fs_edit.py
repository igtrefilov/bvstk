#!/usr/bin/env python3
"""Exercise DCP2 FS through CLI, Python API and fragmented TCP.

Creates one unique directory on an already mounted volume and removes it.
Use --idle-check to include a real upload timeout (about 31 seconds).
"""
import argparse
from contextlib import contextmanager
import hashlib
import io
import json
from pathlib import Path
import random
import secrets
import socket
import struct
import subprocess
import sys
import tarfile
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/dcp2'))
import dcp2_client as dcp


class FragmentedSocket:
    def __init__(self, sock):
        self.sock = sock

    def sendall(self, data):
        for pos in range(0, len(data), 7):
            self.sock.sendall(data[pos:pos + 7])

    def recv(self, size):
        return self.sock.recv(min(size, 5))

    def __getattr__(self, name):
        return getattr(self.sock, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('--port', type=int, default=8889)
    parser.add_argument('--volume', default='sd')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--idle-check', action='store_true')
    parser.add_argument('--capacity-check', action='store_true',
                        help='temporarily exhaust a dedicated test volume smaller than 10 MiB')
    args = parser.parse_args()
    directory = args.volume + ':/.dcp-verify-' + secrets.token_hex(4)
    report = {'directory': directory, 'host': args.host, 'port': args.port, 'checks': []}
    args.output.parent.mkdir(parents=True, exist_ok=True)

    def passed(name, **details):
        report['checks'].append({'name': name, **details})
        print('PASS', name, flush=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')

    @contextmanager
    def connection(fragmented=False):
        with socket.create_connection((args.host, args.port), timeout=5) as sock:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            yield dcp.FsClient(FragmentedSocket(sock) if fragmented else sock, timeout=5, retries=1)

    def error(status, operation, *params, **kwargs):
        try:
            operation(*params, **kwargs)
        except dcp.FsError as exc:
            assert exc.status == status, (hex(exc.status), hex(status))
        else:
            raise AssertionError(f'expected FS status {status:x}')

    def cli(*params, expected=0):
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/dcp2/dcp2_client.py'),
            args.host, '--port', str(args.port), '--timeout', '5', *map(str, params)],
            capture_output=True, timeout=30)
        assert result.returncode == expected, (params, result.returncode, result.stderr)
        return result

    def begin(client, path, size, replace=False):
        return client._mutation(dcp.OP_FS_WRITE_BEGIN,
            struct.pack('>QB', size, int(replace)) + client._path(path), 4)

    created = False
    with tempfile.TemporaryDirectory(prefix='dcp-edit-client-') as local:
        source = Path(local) / 'source.bin'
        target = directory + '/data.bin'
        try:
            with connection() as client:
                report['info'] = client.info()
                report['limits'] = client.write_info()
                client.mkdir(directory)
                created = True
                cases = [(size, block) for size in (0, 1, 3071, 3072, 3073, 65537)
                         for block in (0, 17, 1024, 3072, 4078)] + [(257, 1), (1048579, 3072)]
                for size, block in cases:
                    payload = (bytes(range(256)) * (size // 256 + 1))[:size]
                    source.write_bytes(payload)
                    assert client.upload(source, target, block_size=block) == size
                    data = b''.join(client.stream(target, dcp.FS_FILE, block_size=1024,
                                                  verify_repeats=True))
                    assert hashlib.sha256(data).digest() == hashlib.sha256(payload).digest()
                    client.remove(target)
                passed('API size/block matrix and SHA-256', combinations=len(cases))

            cli('--fs-mkdir', directory + '/каталог с пробелами')
            unicode_path = directory + '/каталог с пробелами/данные.json'
            source.write_bytes('{"имя":"проверка"}\n'.encode())
            cli('--fs-upload', source, unicode_path, '--fs-progress', '--fs-block-size', 1)
            source.write_bytes(b'replacement\x00\xff')
            cli('--fs-upload', source, unicode_path, expected=1)
            cli('--fs-upload', source, unicode_path, '--overwrite')
            assert cli('--fs-read', unicode_path).stdout == source.read_bytes()
            cli('--fs-rename', unicode_path, directory + '/renamed.bin')
            cli('--fs-remove', directory + '/каталог с пробелами')
            cli('--fs-read', directory + '/renamed.bin', '--output', Path(local) / 'download')
            assert (Path(local) / 'download').read_bytes() == source.read_bytes()
            cli('--fs-remove', directory + '/renamed.bin')
            passed('CLI Unicode, spaces, progress, conflict, replace and download to file')

            with connection(fragmented=True) as client:
                source.write_bytes(bytes(range(256)) * 32)
                client.upload(source, target)
                assert b''.join(client.stream(target, dcp.FS_FILE)) == source.read_bytes()
                client.remove(target)
                passed('TCP requests in 7-byte fragments and responses in 5-byte reads')

            for prefix_size in (5, 11, 31):
                with connection() as client:
                    source.write_bytes(b'keep original')
                    client.upload(source, target)
                    handle = begin(client, target, 100, True)
                    frame = dcp.build_frame(dcp.SRV_FS, dcp.OP_FS_WRITE_BLOCK, 15000,
                        struct.pack('>I', client.mutation_id + 1) + handle + struct.pack('>Q', 0) + bytes(100))
                    client.sock.sendall(frame[:prefix_size])
                with connection() as client:
                    assert b''.join(client.stream(target, dcp.FS_FILE)) == b'keep original'
                    client.remove(target)
            passed('Disconnect inside frame header, operation prefix and block data')

            with connection() as client:
                randomizer = random.Random(7000)
                for index in range(100):
                    payload = randomizer.randbytes(randomizer.randrange(0, 2049))
                    source.write_bytes(payload)
                    client.upload(source, target, block_size=257)
                    renamed = directory + '/moved.bin'
                    client.rename(target, renamed)
                    assert b''.join(client.stream(renamed, dcp.FS_FILE)) == payload
                    client.remove(renamed)
                passed('100 upload/rename/read/remove cycles on one connection')

                source.write_bytes(b'original')
                client.upload(source, target)
                handle = begin(client, target, 100, True)
                client._mutation(dcp.OP_FS_WRITE_BLOCK, handle + struct.pack('>Q', 0) + b'partial', 8)
                client._mutation(dcp.OP_FS_WRITE_ABORT, handle)
                assert b''.join(client.stream(target, dcp.FS_FILE)) == b'original'
                client.mkdir(directory + '/occupied')
                error(0x106, client.rename, target, directory + '/occupied')
                error(0x103, client.upload, source, directory + '/occupied', overwrite=True)
                error(1, client.rename, target, 'other:/file')
                client.remove(directory + '/occupied')
                client.mkdir(directory + '/parent')
                client.mkdir(directory + '/parent/child')
                error(1, client.rename, directory + '/parent', directory + '/parent/child/loop')
                assert client.stat(directory + '/parent/child')['type'] == 'directory'
                client.remove(directory + '/parent/child')
                client.remove(directory + '/parent')
                with source.open('wb') as huge:
                    huge.truncate(report['limits']['max_file_size'] + 1)
                try:
                    client.upload(source, directory + '/too-large')
                except ValueError:
                    pass
                else:
                    raise AssertionError('oversized local file was accepted')
                error(0x100, client.stat, directory + '/too-large')
                handle = begin(client, target, 100, True)
                client._mutation(dcp.OP_FS_WRITE_BLOCK, handle + struct.pack('>Q', 0) + b'partial', 8)
            with connection() as client:
                assert b''.join(client.stream(target, dcp.FS_FILE)) == b'original'
                client.remove(target)
                assert not list(client.list_entries(directory))
                passed('ABORT and disconnect preserve original; invalid operations preserve tree')

                for change in ('grow', 'shrink', 'same-size', 'callback-error'):
                    source.write_bytes(bytes(8192))
                    def alter(done, total):
                        if done != 1024:
                            return
                        if change == 'callback-error':
                            raise RuntimeError('cancel from progress callback')
                        source.write_bytes(b'x' * {'grow': 10000, 'shrink': 0, 'same-size': 8192}[change])
                    try:
                        client.upload(source, target, block_size=1024, progress=alter)
                    except (ValueError, RuntimeError):
                        pass
                    else:
                        raise AssertionError('source mutation accepted: ' + change)
                    error(0x100, client.stat, target)
                passed('Source grows, shrinks, changes in place; callback cancels upload')

                client.mkdir(directory + '/tree')
                for index, size in enumerate((0, 513, 8193)):
                    source.write_bytes(bytes([index]) * size)
                    client.upload(source, directory + f'/tree/{index}.bin')
                archive = b''.join(client.stream(directory + '/tree', dcp.FS_TAR, block_size=19))
                with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
                    assert {name.casefold(): tar.extractfile(name).read() for name in tar.getnames()} == {
                        f'{i}.bin': bytes([i]) * n for i, n in enumerate((0, 513, 8193))}
                for index in range(3):
                    client.remove(directory + f'/tree/{index}.bin')
                client.remove(directory + '/tree')
                passed('Uploaded tree downloaded through TAR and verified byte for byte')

                if args.capacity_check:
                    source.write_bytes(b'preserve on full disk')
                    client.upload(source, target)
                    source.write_bytes(bytes(10 * 1024 * 1024))
                    try:
                        client.upload(source, target, overwrite=True)
                    except dcp.FsError as exc:
                        assert exc.status in (3, 0x104), exc
                    else:
                        raise AssertionError('capacity check requires a volume smaller than 10 MiB')
                    assert b''.join(client.stream(target, dcp.FS_FILE)) == b'preserve on full disk'
                    client.remove(target)
                    source.write_bytes(b'recovery')
                    client.upload(source, target)
                    assert b''.join(client.stream(target, dcp.FS_FILE)) == b'recovery'
                    client.remove(target)
                    passed('Full-volume upload preserves original and frees space for next upload')

                if args.idle_check:
                    handle = begin(client, target, 100)
                    client._mutation(dcp.OP_FS_WRITE_BLOCK, handle + struct.pack('>Q', 0) + b'x', 8)
                    time.sleep(report['limits']['idle_timeout_ms'] / 1000 + 1)
                    error(0x102, client._mutation, dcp.OP_FS_WRITE_COMMIT, handle)
                    error(0x100, client.stat, target)
                    passed('Real upload idle timeout')
                assert not list(client.list_entries(directory))
        except BaseException as exc:
            report['error'] = str(exc)
            raise
        finally:
            report['result'] = 'failed' if 'error' in report else 'passed'
            if created:
                with connection() as client:
                    # Only delete entries in this run's uniquely named directory.
                    entries = list(client.list_entries(directory, recursive=True))
                    for entry in reversed(entries):
                        client.remove(directory + '/' + entry['path'])
                    client.remove(directory)
                passed('Private fixture cleanup')


if __name__ == '__main__':
    main()
