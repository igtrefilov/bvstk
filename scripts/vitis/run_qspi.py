#!/usr/bin/env python3
"""Back up, program and read back an AX7020 QSPI boot image."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from boot_common import ROOT, SCRIPT, load_bundle, require_tools, run, sha256, tool_env


def compare_readback(before, after, image, geometry, erase_end):
    if len(before) != geometry["flash_size"] or len(after) != geometry["flash_size"]:
        raise ValueError("Incomplete QSPI snapshot")
    if after[:len(image)] != image:
        raise ValueError("QSPI readback at address 0x00000000 differs from BOOT.bin")
    if after[erase_end:] != before[erase_end:]:
        raise ValueError("QSPI content outside the erase range changed; preserve snapshots for recovery")
    return {"boot_at_zero": True, "outside_erase_unchanged": True,
            "filesystem_sha256": hashlib.sha256(after[geometry["fs_base"]:]).hexdigest()}


def verify_migrated_volume(before, geometry, expected):
    # Earlier BVSTK versions mounted FAT at 8 MiB. Programming a larger
    # boot image would destroy it, even though it preserves the new window.
    # Require an independently prepared and read-back-verified new volume.
    old_base = 8 * 1024 * 1024
    if geometry["fs_base"] > old_base and before[old_base + 510:old_base + 512] == b"\x55\xaa":
        actual = hashlib.sha256(before[geometry["fs_base"]:]).hexdigest()
        if (len(expected) != 64 or expected.lower() != actual or
            before[geometry["fs_base"] + 510:geometry["fs_base"] + 512] != b"\x55\xaa"):
            raise ValueError("Old 8 MiB FAT detected: migrate and verify flash:/ before programming; provide QSPI_MIGRATED_VOLUME_SHA256")


def main():
    parser = argparse.ArgumentParser(prog="./run.sh <freertos|neutrino> qspi", description=__doc__)
    parser.add_argument("--target", choices=["freertos", "neutrino"], default="freertos")
    parser.add_argument("--dry-run", action="store_true", help="validate bundle and display ranges without accessing hardware")
    args = parser.parse_args()
    directory = Path(os.environ.get("BOOT_OUTPUT_DIR", ROOT / "artifacts/boot" / ("neutrino-qspi" if args.target == "neutrino" else args.target))).resolve()
    manifest = load_bundle(directory, target=args.target, medium="qspi")
    if manifest.get("medium", "qspi") != "qspi":
        raise ValueError("SD bundle cannot be programmed to QSPI")
    geometry, image_info = manifest["qspi"], manifest["image"]
    print(f"Image: {directory / 'BOOT.bin'}\nProgram offset: 0x00000000\nImage bytes: {image_info['size']}\n"
          f"Erase range: [0x00000000, 0x{image_info['erase_end']:08X})\n"
          f"Preserve flash:/: [0x{geometry['fs_base']:08X}, 0x{geometry['flash_size']:08X})", flush=True)
    if args.dry_run:
        return
    env = tool_env()
    require_tools(["xsct", "program_flash"], env)
    url = os.environ.get("HW_SERVER_URL", "tcp:127.0.0.1:3121")
    parent = ROOT / "artifacts/boot/qspi"
    parent.mkdir(parents=True, exist_ok=True)
    prefix = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
    evidence = Path(tempfile.mkdtemp(prefix=prefix, dir=parent))
    os.chmod(evidence, 0o700)
    print(f"Backups and verification logs: {evidence}", flush=True)
    # Work from a private copy so a concurrent build cannot replace inputs
    # between preflight validation, programming and readback comparison.
    frozen = evidence / "bundle"
    frozen.mkdir()
    for name in list(manifest["files"]) + ["manifest.json"]:
        shutil.copy2(directory / name, frozen / name)
    if load_bundle(frozen) != manifest:
        raise ValueError("Boot bundle changed while copying it")
    directory = frozen
    snapshot = ["xsct", SCRIPT / "qspi_snapshot.tcl", directory / "ps7_init.tcl", directory / "qspi_read.elf"]
    before_path = evidence / "qspi-before.bin"
    run(snapshot + [before_path, url], env=env, cwd=evidence, log=evidence / "snapshot-before.log", timeout=600)
    if before_path.stat().st_size != geometry["flash_size"]:
        raise ValueError("Incomplete backup; Flash was not programmed")
    verify_migrated_volume(before_path.read_bytes(), geometry, os.environ.get("QSPI_MIGRATED_VOLUME_SHA256", ""))
    # Re-check after the potentially long backup, before any Flash mutation.
    if load_bundle(directory) != manifest:
        raise ValueError("Boot bundle changed while taking the backup")
    command = ["program_flash", "-f", directory / "BOOT.bin", "-offset", "0x00000000",
               "-flash_type", "qspi-x4-single", "-fsbl", directory / "fsbl.elf",
               "-blank_check", "-verify", "-url", url]
    if os.environ.get("JTAG_FREQUENCY"):
        command += ["-frequency", str(int(os.environ["JTAG_FREQUENCY"]))]
    run(command, env=env, cwd=evidence, log=evidence / "program-flash.log", timeout=900)
    after_path = evidence / "qspi-after.bin"
    run(snapshot + [after_path, url], env=env, cwd=evidence, log=evidence / "snapshot-after.log", timeout=600)
    result = compare_readback(before_path.read_bytes(), after_path.read_bytes(),
                              (directory / "BOOT.bin").read_bytes(), geometry, image_info["erase_end"])
    result.update({"qspi": geometry, "image": image_info, "before_sha256": sha256(before_path),
                   "after_sha256": sha256(after_path), "boot_manifest": manifest})
    (evidence / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print("Verified: BOOT.bin is at QSPI address 0; all bytes beyond the erase range are unchanged.")
    print("Power off AX7020, select QSPI on J13 (middle pair), then power on.")
    print(f"Verification report: {evidence / 'verification.json'}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
