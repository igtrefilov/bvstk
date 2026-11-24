#!/usr/bin/env python3
"""
Configure the generated FreeRTOS xilffs for long names, reentrancy and
protection of open files against conflicting opens, renames and removal.
"""
from pathlib import Path
import re
import shutil
import subprocess
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
CONFIG_SRC = REPO_ROOT / "src" / "ports" / "freertos-xilinx" / "fs-fatfs" / "xilffs_config.h"
FFSYSTEM_TEMPLATE = REPO_ROOT / "src" / "ports" / "freertos-xilinx" / "fs-fatfs" / "ffsystem_freertos.c"
FF_LOCKS_PATCH = Path(__file__).with_name("xilffs-r013b-locks.patch")

OS_TYPE_PATTERN = re.compile(r'(?m)^(#define\s+OS_TYPE\s+)(\d+)(\s*/\*.*)$')


def _patch_include_block(text: str, path: Path) -> tuple[str, bool]:
    if INCLUDE_BLOCK not in text:
        if "#ifndef FILE_SYSTEM_USE_LFN" in text:
            return text, False
        replaced, count = re.subn(
            r'#include\s+"xparameters\.h"[^\S\r\n]*\n', PATCHED_BLOCK, text, count=1)
        if count:
            return replaced, True
        raise SystemExit(f"Expected include block missing in {path}")
    return text.replace(INCLUDE_BLOCK, PATCHED_BLOCK, 1), True


def _set_option(text: str, path: Path, name: str, value: int) -> tuple[str, bool]:
    pattern = re.compile(rf'(?m)^(#define\s+{name}\s+)(\d+)(.*)$')
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"{name} definition missing in {path}")
    if match.group(2) == str(value):
        return text, False
    return pattern.sub(lambda m: f"{m.group(1)}{value}{m.group(3)}", text, 1), True


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


def _replace_ffsystem_file(path: Path) -> None:
    if not FFSYSTEM_TEMPLATE.exists():
        raise SystemExit(f"Missing FreeRTOS ffsystem template at {FFSYSTEM_TEMPLATE}")
    shutil.copy2(FFSYSTEM_TEMPLATE, path)


def _ensure_ffsystem_files(root: Path) -> None:
    if not root.exists():
        return
    for entry in sorted(root.rglob("ffsystem.c")):
        _replace_ffsystem_file(entry)
        _ensure_ffsystem_file(entry)


def _patch_object_locks(source: Path) -> None:
    # Only apply on the exact vendor implementation supported by this patch.
    # Vitis regenerates its copy, and invokes this script more than once.
    patch = ["patch", "--silent", "--fuzz=0", "--force", "--no-backup-if-mismatch",
             "--reject-file=-", "--input", str(FF_LOCKS_PATCH), str(source)]
    if subprocess.run(patch + ["--dry-run"], capture_output=True).returncode == 0:
        subprocess.run(patch, check=True)
    elif subprocess.run(patch + ["--reverse", "--dry-run"], capture_output=True).returncode:
        raise SystemExit(f"Unsupported xilffs object locking implementation in {source}")


def ensure_ffconf(path: Path) -> bool:
    if not path.exists():
        raise SystemExit(f"ffconf.h not found at {path}")
    text = path.read_text()
    modified = False
    text, patched = _patch_include_block(text, path)
    modified = modified or patched
    # Two directory readers can occupy 32 entries. Leave room for uploads,
    # HTTP, console and other native FatFs users on the three volumes.
    for name, value in (("FF_FS_REENTRANT", 1), ("FF_FS_LOCK", 64)):
        text, changed = _set_option(text, path, name, value)
        modified = modified or changed
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
    for source in freertos_libsrc.glob("xilffs_v*/src/ff.c"):
        _patch_object_locks(source)


def main():
    if len(sys.argv) < 2:
        raise SystemExit("Usage: patch_ffconf_lfn.py <ffconf.h|directory> [...]")
    for arg in sys.argv[1:]:
        patch_path(Path(arg))


if __name__ == "__main__":
    main()
