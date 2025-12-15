#!/usr/bin/env python3
"""
Ensure the xilffs-generated ffconf.h includes xilffs_config.h even when SDT is
not defined so FILE_SYSTEM_USE_LFN remains set, and make the config header
available where the compiler can find it.
"""
from pathlib import Path
import re
import shutil
import sys


INCLUDE_BLOCK = (
    '#ifdef SDT\n'
    '#include "xilffs_config.h"\n'
    '#else\n'
    '#include "xparameters.h"\n'
    '#endif\n'
)

PATCHED_BLOCK = (
    '#ifdef SDT\n'
    '#include "xilffs_config.h"\n'
    '#else\n'
    '#include "xparameters.h"\n'
    '#ifndef FILE_SYSTEM_USE_LFN\n'
    '#include "xilffs_config.h"\n'
    '#endif\n'
    '#endif\n'
)

REPO_ROOT = Path(__file__).resolve().parents[2]
CONFIG_SRC = REPO_ROOT / "src" / "fs" / "xilffs_config.h"

FF_FS_REENTRANT_PATTERN = re.compile(r'(?m)^(#define\s+FF_FS_REENTRANT\s+)(\d+)(.*)$')
OS_TYPE_PATTERN = re.compile(r'(?m)^(#define\s+OS_TYPE\s+)(\d+)(\s*/\*.*)$')


def _patch_include_block(text: str, path: Path) -> tuple[str, bool]:
    if INCLUDE_BLOCK not in text:
        if "#ifndef FILE_SYSTEM_USE_LFN" in text:
            return text, False
        raise SystemExit(f"Expected include block missing in {path}")
    return text.replace(INCLUDE_BLOCK, PATCHED_BLOCK, 1), True


def _ensure_reentrant(text: str, path: Path) -> tuple[str, bool]:
    match = FF_FS_REENTRANT_PATTERN.search(text)
    if not match:
        raise SystemExit(f"FF_FS_REENTRANT definition missing in {path}")
    if match.group(2) == "1":
        return text, False
    new_text = FF_FS_REENTRANT_PATTERN.sub(
        lambda m: f"{m.group(1)}1{m.group(3)}", text, 1)
    return new_text, True


def _ensure_ffsystem_file(path: Path) -> bool:
    text = path.read_text()
    match = OS_TYPE_PATTERN.search(text)
    if not match:
        return False
    if match.group(2) == "3":
        return False
    new_text = OS_TYPE_PATTERN.sub(lambda m: f"{m.group(1)}3{m.group(3)}", text, 1)
    path.write_text(new_text)
    print(f"Updated OS_TYPE to FreeRTOS in {path}")
    return True


def _ensure_ffsystem_files(root: Path) -> None:
    if not root.exists():
        return
    for entry in sorted(root.rglob("ffsystem.c")):
        _ensure_ffsystem_file(entry)


def ensure_ffconf(path: Path) -> bool:
    if not path.exists():
        raise SystemExit(f"ffconf.h not found at {path}")
    text = path.read_text()
    modified = False
    text, patched = _patch_include_block(text, path)
    modified = modified or patched
    text, reentrant_updated = _ensure_reentrant(text, path)
    modified = modified or reentrant_updated
    if modified:
        path.write_text(text)
    return modified


def ensure_config_in(dir_path: Path) -> None:
    if not CONFIG_SRC.exists():
        raise SystemExit(f"Missing source xilffs_config.h at {CONFIG_SRC}")
    dest = dir_path / "xilffs_config.h"
    shutil.copy2(CONFIG_SRC, dest)


def patch_path(path: Path) -> None:
    if not path.exists():
        return
    workspace_root = path if path.is_dir() else path.parent
    if path.is_file():
        targets = [path]
    else:
        targets = [entry for entry in sorted(path.rglob("ffconf.h"))
                   if "freertos10_xilinx_domain" in entry.parts]
    for entry in targets:
        if not entry.is_file():
            continue
        ensure_ffconf(entry)
        ensure_config_in(entry.parent)
    freertos_libsrc = workspace_root / "plat_bvstk" / "ps7_cortexa9_0" / "freertos10_xilinx_domain" / "bsp" / "ps7_cortexa9_0" / "libsrc"
    _ensure_ffsystem_files(freertos_libsrc)


def main():
    if len(sys.argv) < 2:
        raise SystemExit("Usage: patch_ffconf_lfn.py <ffconf.h|directory> [...]")
    for arg in sys.argv[1:]:
        patch_path(Path(arg))


if __name__ == "__main__":
    main()
