#!/usr/bin/env python3
"""Boot-header, flashing-range and dispatch checks; no hardware access."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/vitis"))
from boot_common import layout, load_bundle, validate_image
from run_qspi import compare_readback
import run_qspi


def boot_image():
    image = bytearray(0x3200)
    words = [0] * 19
    words[8:19] = [0xAA995566, 0x584C4E58, 0, 0x01010000, 0x1700, 256, 0, 0, 256, 1, 0]
    words[18] = ~sum(words[8:18]) & 0xFFFFFFFF
    struct.pack_into("<19I", image, 0, *words)
    struct.pack_into("<2I", image, 0x98, 0x8C0, 0xC80)
    struct.pack_into("<I", image, 0x8C4, 3)
    for index, (offset, size, load, flags) in enumerate([(0x1700, 256, 0, 0x10), (0x2000, 256, 0, 0x20), (0x3000, 512, 0x100000, 0x10)]):
        part = [size // 4] * 3 + [load, load, offset // 4, flags, 1] + [0] * 8
        part[15] = ~sum(part[:15]) & 0xFFFFFFFF
        struct.pack_into("<16I", image, 0xC80 + index * 64, *part)
    return image


class BootImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "BOOT.bin"
        self.path.write_bytes(boot_image())

    def test_partition_order_and_erase_boundary(self):
        metadata = validate_image(self.path)
        self.assertEqual(metadata["erase_end"], 65536)
        self.assertEqual(metadata["partitions"][2]["entry"], 0x100000)

    def test_reject_corrupt_header_and_partitions(self):
        for offset in (0x20, 0x48, 0x98, 0x8C4, 0xC80, 0xCF0):
            with self.subTest(offset=offset):
                image = boot_image()
                image[offset] ^= 1
                self.path.write_bytes(image)
                with self.assertRaises(ValueError):
                    validate_image(self.path)

    def test_reject_truncated_partition(self):
        self.path.write_bytes(boot_image()[:-1])
        with self.assertRaisesRegex(ValueError, "Truncated partition"):
            validate_image(self.path)

    def test_never_erase_filesystem(self):
        with self.path.open("ab") as stream:
            stream.truncate(layout()["fs_base"] + 1)
        with self.assertRaisesRegex(ValueError, "overlaps"):
            validate_image(self.path)

    def test_readback_address_and_tail(self):
        geometry = {"flash_size": 128, "fs_base": 64}
        before = b"z" * 128
        image = b"boot" * 8
        after = image + b"\xff" * 32 + before[64:]
        self.assertTrue(compare_readback(before, after, image, geometry, 64)["boot_at_zero"])
        with self.assertRaisesRegex(ValueError, "address 0"):
            compare_readback(before, b"x" + after[:-1], image, geometry, 64)
        with self.assertRaisesRegex(ValueError, "outside"):
            compare_readback(before, after[:-1] + b"x", image, geometry, 64)
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            compare_readback(before, after[:-1], image, geometry, 64)

    def make_bundle(self):
        directory = self.path.parent
        for name in ("fsbl.elf", "qspi_read.elf", "ps7_init.tcl", "design.bit", "app_bvstk.elf", "design.xsa", "boot.bif"):
            (directory / name).write_bytes(b"fixture")
        manifest = {"format": 1, "target": "freertos", "qspi": layout(), "image": validate_image(self.path),
                    "files": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.iterdir()}}
        (directory / "manifest.json").write_text(json.dumps(manifest))
        return manifest

    def test_manifest_and_root_dispatch(self):
        import os
        directory = self.path.parent
        manifest = self.make_bundle()
        self.assertEqual(load_bundle(directory), manifest)
        result = subprocess.run([ROOT / "run.sh", "freertos", "qspi", "--dry-run"],
                                env={**os.environ, "BOOT_OUTPUT_DIR": str(directory)}, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Program offset: 0x00000000", result.stdout)
        self.assertIn("0x00800000", result.stdout)
        self.path.write_bytes(self.path.read_bytes()[:-1])
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            load_bundle(directory)

    def test_programmer_uses_frozen_bundle_and_checks_full_readback(self):
        import os
        self.make_bundle()
        root = self.path.parent / "fake-root"
        image = self.path.read_bytes()
        flash = bytearray(b"\xff") * layout()["flash_size"]
        flash[layout()["fs_base"]:] = b"\x5a" * (len(flash) - layout()["fs_base"])
        calls = []

        def command(args, **kwargs):
            calls.append(args)
            if args[0] == "xsct":
                Path(args[4]).write_bytes(flash)
                # Simulate another build replacing the original bundle while
                # the programmer is taking its backup. The private copy wins.
                self.path.write_bytes(b"replaced during backup")
            else:
                self.assertEqual(args[0], "program_flash")
                self.assertEqual(args[args.index("-offset") + 1], "0x00000000")
                self.assertIn("-blank_check", args)
                self.assertIn("-verify", args)
                source = Path(args[args.index("-f") + 1])
                self.assertNotEqual(source, self.path)
                self.assertEqual(source.read_bytes(), image)
                flash[:len(image)] = image

        with patch.dict(os.environ, {"BOOT_OUTPUT_DIR": str(self.path.parent)}), \
             patch.object(sys, "argv", ["run_qspi.py"]), \
             patch.object(run_qspi, "ROOT", root), \
             patch.object(run_qspi, "require_tools"), \
             patch.object(run_qspi, "run", side_effect=command):
            run_qspi.main()
        self.assertEqual([c[0] for c in calls], ["xsct", "program_flash", "xsct"])
        reports = list(root.glob("artifacts/boot/qspi/*/verification.json"))
        self.assertEqual(len(reports), 1)
        self.assertTrue(json.loads(reports[0].read_text())["outside_erase_unchanged"])


if __name__ == "__main__":
    unittest.main()
