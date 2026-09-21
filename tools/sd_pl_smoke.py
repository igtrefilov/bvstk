#!/usr/bin/env python3
"""UART smoke test of the integrated FreeRTOS PL-SD filesystem/service.

Close other UART readers first. Requires Linux, a matching initialized
PS/DDR/PL, and (for --load) xsct. ``--load`` uses the normal BVSTK JTAG
launcher and therefore programs the selected existing bitstream; it does not
build or modify the FPGA design.
"""
import argparse
import fcntl
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import termios
import time


class Uart:
    """Small 115200 8N1 transport; preserves modem lines and restores termios."""
    def __init__(self, device):
        self.fd = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.saved = termios.tcgetattr(self.fd)
        try:
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            attrs = termios.tcgetattr(self.fd)
            attrs[0] = attrs[1] = attrs[3] = 0
            attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            attrs[4] = attrs[5] = termios.B115200
            attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
            termios.tcflush(self.fd, termios.TCIFLUSH)
        except Exception:
            self.close()
            raise

    def read(self):
        ready, _, _ = select.select([self.fd], [], [], 0.1)
        return os.read(self.fd, 4096) if ready else b""

    def write(self, data):
        while data:
            _, ready, _ = select.select([], [self.fd], [], 2)
            if not ready:
                raise RuntimeError("UART write timeout")
            # The board's polling UART console has no RX flow control and a
            # 64-byte FIFO. Pace pasted commands across FreeRTOS ticks.
            data = data[os.write(self.fd, data[:16]):]
            if data:
                time.sleep(0.01)

    def close(self):
        if self.fd is not None:
            try:
                termios.tcsetattr(self.fd, termios.TCSANOW, self.saved)
            finally:
                os.close(self.fd)
                self.fd = None


def pattern(lba, seed):
    state = seed ^ lba ^ 0x9E3779B9
    state = state or 1
    result = bytearray()
    for _ in range(512):
        state ^= (state << 13) & 0xFFFFFFFF
        state ^= state >> 17
        state ^= (state << 5) & 0xFFFFFFFF
        result.append(state & 255)
    result[:4] = lba.to_bytes(4, "little")
    result[4:8] = seed.to_bytes(4, "little")
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--load", action="store_true", help="program existing PL and load the full BVSTK ELF")
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--quiet", action="store_true", help="print host steps only; keep full --log")
    parser.add_argument("--lba", type=lambda v: int(v, 0), default=4096)
    parser.add_argument("--count", type=int, default=2)
    parser.add_argument("--seed", type=lambda v: int(v, 0), default=0x12345678)
    parser.add_argument("--write-test", action="store_true", help="DESTROY data in the selected range")
    parser.add_argument("--fs-test", action="store_true", help="test files in a new unique sd-pl:/ directory; never format")
    parser.add_argument("--command", action="append", help="run explicit CLI commands instead of smoke sequence")
    args = parser.parse_args()
    if args.fs_test and (args.write_test or args.command):
        parser.error("--fs-test cannot be combined with --write-test or --command")
    if not (0 <= args.lba <= 0xFFFFFFFF and 1 <= args.count <= 8 and
            args.lba + args.count - 1 <= 0xFFFFFFFF and 0 <= args.seed <= 0xFFFFFFFF):
        parser.error("invalid LBA/count/seed")
    root = Path(__file__).resolve().parents[1]
    if args.log:
        args.log.parent.mkdir(parents=True, exist_ok=True)
    log = args.log.open("w", encoding="utf-8") if args.log else None

    def output(text):
        if not args.quiet or text.startswith("\nHOST"):
            print(text, end="", flush=True)
        if log:
            log.write(text)
            log.flush()

    port = None

    def prompt(timeout=30):
        reply = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = port.read()
            if data:
                reply.extend(data)
                output(data.decode("ascii", errors="replace"))
                if re.search(rb"(?:^|[\r\n])(?:Zynq/[^\r\n>]*|sd-pl)> ", reply):
                    return reply.decode("ascii", errors="replace")
        raise RuntimeError("UART prompt timeout")

    def command(text, expected=None):
        output(f"\nHOST command: {text}\n")
        port.write(text.encode("ascii") + b"\r")
        # At most 16 commands in test(count=8), each with 20s init + 5s I/O.
        reply = prompt(420)
        if expected and expected not in reply:
            raise RuntimeError(f"command failed: {text}")
        return reply

    def read_sector(lba):
        reply = command(f"sd-pl read {lba}", "OK: sd-pl read")
        lines = re.findall(r"^([0-9A-F]{3}):((?: [0-9A-F]{2}){16})\r?$", reply, re.M)
        if [int(offset, 16) for offset, _ in lines] != list(range(0, 512, 16)):
            raise RuntimeError("incomplete or unordered sector dump")
        return bytes.fromhex("".join(data for _, data in lines))

    def load_application():
        env = os.environ.copy()
        if args.elf:
            env["ELF_FILE"] = str(args.elf.resolve())
        result = subprocess.run(["xsct", str(root / "scripts/vitis/run_jtag.tcl")],
                                env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=45)
        output(result.stdout)
        result.check_returncode()
        prompt(120)

    def mounted():
        for _ in range(30):
            reply = command("sd-pl info")
            if "sd-pl:/ mounted=1" in reply:
                return reply
            time.sleep(0.2)
        raise RuntimeError("sd-pl:/ not mounted; inspect boot log (never auto-format)")

    def fs_command(text):
        reply = command(text)
        if re.search(r"(?:^|\r?\n)(?:ERR|FS not ready|FS busy)", reply):
            raise RuntimeError(f"filesystem command failed: {text}")
        return reply

    def check_file(path, expected):
        text = f"cat {path}"
        reply = fs_command(text)
        payload = reply.partition(text + "\r\n")[2].split("\r\nZynq/", 1)[0]
        if payload != expected:
            raise RuntimeError(f"file content/length mismatch: {path}")

    def fs_test():
        mounted()
        fs_command("ls sd-pl:/")
        directory = "sd-pl:/bvstk-fs-" + time.strftime("%Y%m%d-%H%M%S") + f"-{os.getpid()}"
        path = directory + "/long-file-name.txt"
        # Fits the 255-character UART line limit, grows past a 4-KiB cluster.
        chunk = "PL-SD:" + "0123456789abcdef" * 9
        if len("fs append " + path + " " + chunk) > 255:
            raise RuntimeError("test command exceeds UART line limit")
        fs_command(f"mkdir {directory}")
        fs_command(f"mkdir {directory}/subdir")
        fs_command(f"fs write {path} {chunk}")
        for _ in range(31):
            fs_command(f"fs append {path} {chunk}")
        expected = chunk * 32
        check_file(path, expected)
        fs_command(f"cp {path} {directory}/copy.txt")
        fs_command(f"mv {directory}/copy.txt {directory}/subdir/moved.txt")
        check_file(directory + "/subdir/moved.txt", expected)
        fs_command(f"fs write {path} replaced")
        check_file(path, "replaced")
        for text in ("sd-pl init", "sd-pl read 0", "sd-pl write 0 1", "sd-pl test 0 1 1"):
            command(text, "ERR: raw commands disabled")
        command("fs format sd-pl confirm", "ERR: formatting sd-pl is disabled")
        if args.load:
            output("\nHOST reload: verify persistence after PS reset + existing bitstream load\n")
            load_application()
            mounted()
            check_file(path, "replaced")
            check_file(directory + "/subdir/moved.txt", expected)
        fs_command(f"touch {directory}/empty.txt")
        check_file(directory + "/empty.txt", "")
        fs_command(f"rm {directory}/empty.txt")
        fs_command(f"ls {directory}")
        output(f"\nHOST PASS: filesystem mkdir/write/append/read/copy/move/truncate/delete/protection"
               + ("/restart-persistence" if args.load else "")
               + f"; test files retained in {directory}\n")

    try:
        port = Uart(args.port)
        if args.load:
            load_application()
        else:
            port.write(b"\x03")
            prompt(30)
        if args.command:
            for text in args.command:
                command(text)
            return 0
        if args.fs_test:
            fs_test()
            return 0
        reply = command("sd-pl info")
        if "filesystem_owned=1" in reply:
            if args.write_test:
                raise RuntimeError("raw writes disabled in filesystem mode; use --fs-test")
            mounted()
            fs_command("ls sd-pl:/")
            output("\nHOST PASS: existing filesystem auto-init/mount/list (read-only check)\n")
            return 0
        command("sd-pl init", "OK: sd-pl init")
        read_sector(args.lba)
        if args.write_test:
            # Check adjacent blocks as well as address-dependent test data.
            neighbors = [n for n in (args.lba - 1, args.lba + args.count)
                         if 0 <= n <= 0xFFFFFFFF]
            before = {lba: read_sector(lba) for lba in neighbors}
            command(f"sd-pl test {args.lba} {args.count} 0x{args.seed:08x}", "OK: sd-pl test")
            # Read after a fresh controller/card init to exclude a BRAM-only echo.
            command("sd-pl init", "OK: sd-pl init")
            for lba in range(args.lba, args.lba + args.count):
                actual = read_sector(lba)
                if actual != pattern(lba, args.seed):
                    raise RuntimeError(f"host-side readback mismatch at LBA {lba}")
            for lba, expected in before.items():
                if read_sector(lba) != expected:
                    raise RuntimeError(f"neighbor sector changed at LBA {lba}")
        output("\nHOST PASS: " + ("write/readback/reinit/readback/neighbors" if args.write_test else "init/read") + "\n")
        return 0
    except (RuntimeError, OSError, subprocess.SubprocessError) as exc:
        output(f"\nHOST FAIL: {exc}\n")
        return 1
    finally:
        if port:
            port.close()
        if log:
            log.close()


if __name__ == "__main__":
    sys.exit(main())
