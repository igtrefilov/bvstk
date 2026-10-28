#!/usr/bin/env python3
"""Build a self-contained, checked boot bundle for AX7020."""
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
import tarfile
import io

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


def source_fingerprint():
    paths = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard"], cwd=ROOT, text=True).splitlines()
    return {p: sha256(ROOT / p) for p in sorted(set(paths))
            if (ROOT / p).is_file() and p != "scripts/neutrino/gost/flash-gost-sign-test.sh"}


def main():
    parser = argparse.ArgumentParser(prog="./build_boot.sh", description=__doc__)
    parser.add_argument("target", choices=["freertos", "neutrino"])
    parser.add_argument("--no-build", action="store_true", help="package the already built, XSA-matched application")
    parser.add_argument("--medium", choices=["qspi", "sd"], help="default: FreeRTOS QSPI, Neutrino PS SD")
    args = parser.parse_args()
    medium = args.medium or ("sd" if args.target == "neutrino" else "qspi")
    env = tool_env()
    epoch = env.setdefault("SOURCE_DATE_EPOCH", subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=ROOT, text=True).strip())
    if not epoch.isdigit():
        raise ValueError("SOURCE_DATE_EPOCH must be a nonnegative integer")
    require_tools(["bootgen", "arm-none-eabi-gcc", "make"], env)
    geometry = layout()
    if not args.no_build:
        run([ROOT / "build.sh", "freertos" if args.target == "freertos" else "neutrino-image"], cwd=ROOT, env=env)
    workspace = ROOT / "vitis_ws"
    platform = workspace / "plat_bvstk"
    if not (workspace / ".bvstk-build-state").is_file() or not (platform / "zynq_fsbl").is_dir():
        raise ValueError("Missing Vitis FSBL workspace; prepare matching XSA with ./build.sh freertos first")
    xsa = Path(os.environ.get("XSA", ROOT / "artifacts/fpga/design.xsa")).resolve()
    bit = Path(os.environ.get("BITSTREAM_FILE", ROOT / "artifacts/fpga/design.bit")).resolve()
    app = workspace / "app_bvstk/Debug/app_bvstk.elf" if args.target == "freertos" else Path(os.environ.get("NEUTRINO_IFS_FILE", ROOT / "build/neutrino/ifs-zynq7000-ax7020-bvstk.raw"))
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
    if args.target == "freertos" and (len(ident) != 20 or ident[:7] != b"\x7fELF\x01\x01\x01" or ident[18:20] != b"\x28\x00"):
        raise ValueError("Application is not a little-endian ARM ELF32")
    if medium == "qspi" and args.target == "neutrino" and app.stat().st_size + bit.stat().st_size >= geometry["fs_base"]:
        raise ValueError("Neutrino IFS + PL exceeds QSPI boot window; use --medium sd (existing QSPI filesystem is preserved)")
    bundle_name = "neutrino-qspi" if args.target == "neutrino" and medium == "qspi" else args.target
    output = Path(os.environ.get("BOOT_OUTPUT_DIR", ROOT / "artifacts/boot" / bundle_name)).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(output, 0o700)
    build_parent = ROOT / "build/boot"
    build_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=args.target + "-", dir=build_parent) as work_name:
        work = Path(work_name)
        fsbl = work / "fsbl"
        prepare_fsbl(platform / "zynq_fsbl", fsbl)
        run(["make", "-j2", "CFLAGS=-DFSBL_DEBUG_INFO", "all"], cwd=fsbl, env=env, log=work / "fsbl-build.log")
        staged = work / "bundle"
        staged.mkdir()
        for source, name in [(fsbl / "fsbl.elf", "fsbl.elf"), (app, "app_bvstk.elf" if args.target == "freertos" else "ifs.raw"),
                             (bit, "design.bit"), (xsa, "design.xsa"),
                             (platform / "hw/ps7_init.tcl", "ps7_init.tcl")]:
            shutil.copy2(source, staged / name)
        bsp = fsbl / "zynq_fsbl_bsp/ps7_cortexa9_0"
        run(["arm-none-eabi-gcc", "-O2", "-Wall", "-Wextra", "-mcpu=cortex-a9", "-mfpu=vfpv3", "-mfloat-abi=hard",
             "-I", bsp / "include", "-L", bsp / "lib", "-specs=Xilinx.spec", "-Wl,-build-id=none", "-Tlscript.ld",
             SCRIPT / "qspi_read.c", "-Wl,--start-group,-lxil,-lgcc,-lc,--end-group", "-o", staged / "qspi_read.elf"],
            cwd=fsbl, env=env, log=work / "qspi-reader-build.log")
        application = "app_bvstk.elf"
        if args.target == "neutrino":
            # Raw IFS includes executable ARM startup code at its first byte,
            # exactly as in run_jtag.tcl. Wrap bytes in one loadable ELF segment.
            (staged / "ifs.S").write_text('.section .text, "ax"\n.global _start\n_start:\n.incbin "ifs.raw"\n')
            run(["arm-none-eabi-gcc", "-c", "-mcpu=cortex-a9", "-marm", "ifs.S", "-o", "ifs.o"], cwd=staged, env=env)
            run(["arm-none-eabi-gcc", "-nostdlib", "-Wl,-Ttext=0x00100000,-e,_start,--build-id=none", "ifs.o", "-o", "ifs.elf"], cwd=staged, env=env)
            (staged / "ifs.o").unlink()
            application = "ifs.elf"
            # Reproducible public resources; no credentials or generated files.
            with tarfile.open(staged / "resources.tar", "w", format=tarfile.PAX_FORMAT) as archive:
                for root, prefix in [(ROOT / "configs", "config"), (ROOT / "web/assets", "www")]:
                    for path in sorted(root.rglob("*")):
                        if path.is_file():
                            data = path.read_bytes()
                            info = tarfile.TarInfo(prefix + "/" + path.relative_to(root).as_posix())
                            info.size, info.mode, info.mtime = len(data), 0o644, 0
                            archive.addfile(info, io.BytesIO(data))
        (staged / "boot.bif").write_text("the_ROM_image:\n{\n    [bootloader] fsbl.elf\n    design.bit\n    " + application + "\n}\n")
        run(["bootgen", "-arch", "zynq", "-image", "boot.bif", "-o", "BOOT.bin", "-w", "on"],
            cwd=staged, env=env, log=staged / "bootgen.log")
        image = validate_image(staged / "BOOT.bin", geometry, medium)
        run(["bootgen", "-arch", "zynq", "-read", "BOOT.bin"], cwd=staged, env=env, log=staged / "headers.txt")
        for name in ["fsbl-build.log", "qspi-reader-build.log"]:
            shutil.copy2(work / name, staged / name)
        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        manifest = {"format": 1, "target": args.target, "medium": medium, "created_utc": datetime.now(timezone.utc).isoformat(),
                    "git_revision": revision, "worktree_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)),
                    "qspi": geometry, "image": image,
                    "source_sha256": source_fingerprint(),
                    "tools": {name: shutil.which(name, path=env["PATH"]) for name in ("bootgen", "arm-none-eabi-gcc", "qcc", "mkifs")},
                    "source_date_epoch": int(epoch),
                    "compiler_version": subprocess.check_output(["arm-none-eabi-gcc", "--version"], env=env, text=True).splitlines()[0], "fsbl_winbond_ear_fix": True,
                    "files": {p.name: sha256(p) for p in sorted(staged.iterdir()) if p.is_file()}}
        (staged / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        load_bundle(staged)
        # Publish the manifest last: an interrupted replacement cannot pass
        # the programmer's checksum validation as a complete bundle.
        output.mkdir(parents=True, exist_ok=True)
        for path in sorted(staged.iterdir(), key=lambda p: p.name == "manifest.json"):
            shutil.copy2(path, output / path.name)
    load_bundle(output)
    print(f"Target: {args.target}; medium: {medium}")
    print(f"BOOT image: {output / 'BOOT.bin'}\nSize: {image['size']} bytes")
    if medium == "qspi":
        print(f"Erase end: 0x{image['erase_end']:08X}; flash:/ starts: 0x{geometry['fs_base']:08X}")
    print(f"SHA256: {image['sha256']}\nInstall with: ./run.sh {args.target} {medium}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError, zipfile.BadZipFile) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
