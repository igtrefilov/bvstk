#!/usr/bin/env python3
"""Build a self-contained, checked FreeRTOS boot bundle for AX7020."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

from boot_common import ROOT, SCRIPT, layout, load_bundle, require_tools, run, sha256, tool_env, validate_image


def prepare_fsbl(source, destination):
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns("*.o", "*.d", "*.elf"))
    # Vitis 2021.2 recognizes Winbond's JEDEC ID but omits it from both EAR
    # command branches. W25Q256 uses the same C5/C8 commands as Micron.
    qspi = destination / "qspi.c"
    original = qspi.read_text()
    old = "if (QspiFlashMake == MICRON_ID || QspiFlashMake == MACRONIX_ID)"
    if original.count(old) != 2:
        raise ValueError("Unrecognized FSBL qspi.c; review Winbond EAR support for this Vitis version")
    qspi.write_text(original.replace(old, "if (QspiFlashMake == MICRON_ID || QspiFlashMake == MACRONIX_ID ||\n\t    QspiFlashMake == WINBOND_ID)"))


def main():
    parser = argparse.ArgumentParser(prog="./build_boot.sh", description=__doc__)
    parser.add_argument("target", choices=["freertos"])
    parser.add_argument("--no-build", action="store_true", help="package the already built, XSA-matched application")
    args = parser.parse_args()
    env = tool_env()
    require_tools(["bootgen", "arm-none-eabi-gcc", "make"], env)
    geometry = layout()
    if not args.no_build:
        run([ROOT / "build.sh", "freertos"], cwd=ROOT, env=env)
    workspace = ROOT / "vitis_ws"
    platform = workspace / "plat_bvstk"
    xsa = Path(os.environ.get("XSA", ROOT / "artifacts/fpga/design.xsa")).resolve()
    bit = Path(os.environ.get("BITSTREAM_FILE", ROOT / "artifacts/fpga/design.bit")).resolve()
    app = workspace / "app_bvstk/Debug/app_bvstk.elf"
    state = dict(line.split("=", 1) for line in (workspace / ".bvstk-build-state").read_text().splitlines())
    if state.get("xsa_sha256") != sha256(xsa) or sha256(platform / "hw/design.xsa") != sha256(xsa):
        raise ValueError("Vitis workspace/XSA mismatch; run ./build.sh freertos first")
    with zipfile.ZipFile(xsa) as archive:
        bit_entries = [name for name in archive.namelist() if name.endswith(".bit")]
        if len(bit_entries) != 1 or archive.read(bit_entries[0]) != bit.read_bytes():
            raise ValueError("Bitstream does not match the XSA embedded bitstream")
        if archive.read("ps7_init.c") != (platform / "zynq_fsbl/ps7_init.c").read_bytes():
            raise ValueError("FSBL initialization does not match the XSA")
        if archive.read("ps7_init.tcl") != (platform / "hw/ps7_init.tcl").read_bytes():
            raise ValueError("JTAG initialization does not match the XSA")
    ident = app.read_bytes()[:20]
    if len(ident) != 20 or ident[:7] != b"\x7fELF\x01\x01\x01" or ident[18:20] != b"\x28\x00":
        raise ValueError("Application is not a little-endian ARM ELF32")
    output = Path(os.environ.get("BOOT_OUTPUT_DIR", ROOT / "artifacts/boot/freertos")).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    build_parent = ROOT / "build/boot"
    build_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="freertos-", dir=build_parent) as work_name:
        work = Path(work_name)
        fsbl = work / "fsbl"
        prepare_fsbl(platform / "zynq_fsbl", fsbl)
        run(["make", "-j2", "CFLAGS=-DFSBL_DEBUG_INFO", "all"], cwd=fsbl, env=env, log=work / "fsbl-build.log")
        staged = work / "bundle"
        staged.mkdir()
        for source, name in [(fsbl / "fsbl.elf", "fsbl.elf"), (app, "app_bvstk.elf"),
                             (bit, "design.bit"), (xsa, "design.xsa"),
                             (platform / "hw/ps7_init.tcl", "ps7_init.tcl")]:
            shutil.copy2(source, staged / name)
        bsp = fsbl / "zynq_fsbl_bsp/ps7_cortexa9_0"
        run(["arm-none-eabi-gcc", "-O2", "-Wall", "-Wextra", "-mcpu=cortex-a9", "-mfpu=vfpv3", "-mfloat-abi=hard",
             "-I", bsp / "include", "-L", bsp / "lib", "-specs=Xilinx.spec", "-Wl,-build-id=none", "-Tlscript.ld",
             SCRIPT / "qspi_read.c", "-Wl,--start-group,-lxil,-lgcc,-lc,--end-group", "-o", staged / "qspi_read.elf"],
            cwd=fsbl, env=env, log=work / "qspi-reader-build.log")
        (staged / "boot.bif").write_text("the_ROM_image:\n{\n    [bootloader] fsbl.elf\n    design.bit\n    app_bvstk.elf\n}\n")
        run(["bootgen", "-arch", "zynq", "-image", "boot.bif", "-o", "BOOT.bin", "-w", "on"],
            cwd=staged, env=env, log=staged / "bootgen.log")
        image = validate_image(staged / "BOOT.bin", geometry)
        run(["bootgen", "-arch", "zynq", "-read", "BOOT.bin"], cwd=staged, env=env, log=staged / "headers.txt")
        for name in ["fsbl-build.log", "qspi-reader-build.log"]:
            shutil.copy2(work / name, staged / name)
        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        manifest = {"format": 1, "target": "freertos", "created_utc": datetime.now(timezone.utc).isoformat(),
                    "git_revision": revision, "worktree_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)),
                    "qspi": geometry, "image": image, "fsbl_winbond_ear_fix": True,
                    "files": {p.name: sha256(p) for p in sorted(staged.iterdir()) if p.is_file()}}
        (staged / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        load_bundle(staged)
        # Publish the manifest last: an interrupted replacement cannot pass
        # the programmer's checksum validation as a complete bundle.
        output.mkdir(parents=True, exist_ok=True)
        for path in sorted(staged.iterdir(), key=lambda p: p.name == "manifest.json"):
            shutil.copy2(path, output / path.name)
    load_bundle(output)
    print(f"BOOT image: {output / 'BOOT.bin'}\nSize: {image['size']} bytes; erase end: 0x{image['erase_end']:08X}; flash:/ starts: 0x{geometry['fs_base']:08X}")
    print(f"SHA256: {image['sha256']}\nProgram with: ./run.sh freertos qspi")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, zipfile.BadZipFile) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
