#!/usr/bin/env python3
"""Create and verify a 16 MiB FAT16 image from an already verified file backup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from pyfatfs.PyFatFS import PyFatFS

SIZE = 16 * 1024 * 1024

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--backup', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    root = a.backup.resolve()
    if not root.is_dir() or a.output.exists(): raise ValueError('Backup must exist and output must be new')
    expected = json.loads(a.manifest.read_text())
    paths = sorted(root.rglob('*'))
    if any(x.is_symlink() or not (x.is_file() or x.is_dir()) for x in paths): raise ValueError('Unsupported entries in backup')
    actual = {x.relative_to(root).as_posix().casefold(): hashlib.sha256(x.read_bytes()).hexdigest()
              for x in paths if x.is_file()}
    if len(actual) != sum(x.is_file() for x in paths) or actual != expected:
        raise ValueError('Ambiguous or mismatched backup files')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.fat16-', dir=a.output.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, 'wb') as image: image.truncate(SIZE)
        subprocess.run(['mkfs.fat', '-F', '16', '-n', 'BVSTK', temporary], check=True)
        with PyFatFS(temporary, preserve_case=True) as fs:
            for path in paths:
                name = '/' + path.relative_to(root).as_posix()
                if path.is_dir(): fs.makedirs(name, recreate=True)
                elif path.is_file(): fs.writebytes(name, path.read_bytes())
            copied = {name.lstrip('/').casefold(): hashlib.sha256(fs.readbytes(name)).hexdigest()
                      for name in fs.walk.files('/')}
            if len(copied) != sum(1 for _ in fs.walk.files('/')) or copied != expected:
                raise ValueError('Ambiguous or mismatched restored FAT image')
        subprocess.run(['fsck.fat', '-n', temporary], check=True)
        os.replace(temporary, a.output)
        print(json.dumps({'files': len(copied), 'bytes': SIZE, 'sha256': hashlib.sha256(a.output.read_bytes()).hexdigest()}))
    finally:
        if os.path.exists(temporary): os.unlink(temporary)

if __name__ == '__main__': main()
