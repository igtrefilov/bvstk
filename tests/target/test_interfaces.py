#!/usr/bin/env python3
"""Acceptance of shared CLI/DCP2 on a running FreeRTOS or Neutrino image.

Uses private filesystem directories; never saves profiles or writes MMIO.
Register writes are restricted to a value rejected by the current MDIO policy.
Run both images separately and retain their JSON reports for comparison.
"""
import argparse
from collections import deque
import hashlib
import io
import json
from pathlib import Path
import re
import secrets
import shlex
import socket
import struct
import subprocess
import sys
import tarfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/dcp2"))
import dcp2_client as dcp
from dcp2_cache import CacheClient


class Wire:
    """Keep interleaved events and partial frames while matching RPCs."""
    def __init__(self, sock):
        self.parser = CacheClient(sock)
        self.sock, self.sequence, self.events = sock, 0, deque()

    def request(self, service, opcode, body=b"", status=0):
        self.sequence += 1
        self.sock.sendall(dcp.build_frame(service, opcode, self.sequence, body))
        deadline = time.monotonic() + 8
        while True:
            frame = self.parser._frame(deadline)
            if frame.is_event:
                self.events.append(frame)
                continue
            assert (frame.srv, frame.op, frame.seq) == (service, opcode | 0x80, self.sequence), frame
            assert len(frame.body) >= 2
            actual = struct.unpack_from(">H", frame.body)[0]
            assert actual == status, (service, opcode, hex(actual), hex(status))
            return frame.body[2:]

    def event(self, service, timeout=5):
        deadline = time.monotonic() + timeout
        for _ in range(len(self.events)):
            frame = self.events.popleft()
            if frame.srv == service:
                return frame
            self.events.append(frame)
        while True:
            frame = self.parser._frame(deadline)
            assert frame.is_event and not frame.seq, frame
            if frame.srv == service:
                return frame
            self.events.append(frame)

    def quiet(self, duration=.35):
        assert not self.events
        try:
            frame = self.parser._frame(time.monotonic() + duration)
        except socket.timeout:
            return
        raise AssertionError(("unexpected event", frame))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("os", choices=("freertos", "neutrino"))
    ap.add_argument("--host", default="192.168.0.10")
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--known-hosts", type=Path, required=True)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"os": args.os, "checks": [], "cli": {}, "physical_register_writes": False}
    private = ".bvstk-stage6-" + secrets.token_hex(5)
    roots = [volume + ":/" + private for volume in ("flash", "sd")]
    report["private_directories"] = roots
    log = (args.output / "cli.log").open("wb")
    options = ["-i", str(ROOT / "build/neutrino/ax7020_ssh_client"), "-o", "BatchMode=yes",
               "-o", "ConnectTimeout=5", "-o", "StrictHostKeyChecking=yes",
               "-o", "UserKnownHostsFile=" + str(args.known_hosts)]

    def save():
        (args.output / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")

    def passed(name, **details):
        report["checks"].append({"name": name, "result": "passed", **details})
        save()
        print("PASS:", name, flush=True)

    def connect():
        sock = socket.create_connection((args.host, 8889), timeout=8)
        sock.settimeout(8)
        return sock

    def cli(command, success=True, binary=False):
        log.write(b"$ " + command.encode() + b"\n"); log.flush()
        if args.os == "neutrino":
            result = subprocess.run(["ssh", "-n", *options, "root@" + args.host, command],
                                    capture_output=True, timeout=30)
            raw = result.stdout
            log.write(raw + result.stderr); log.flush()
            assert bool(result.returncode == 0) == success, (command, result.returncode, raw, result.stderr)
        else:
            with socket.create_connection((args.host, 8888), timeout=5) as sock:
                sock.settimeout(8)
                banner = bytearray()
                while not banner.endswith(b"> "):
                    banner.extend(sock.recv(8192))
                sock.sendall(command.encode() + b"\r\n")
                received = bytearray()
                while not received.endswith(b"> "):
                    chunk = sock.recv(8192)
                    assert chunk, (command, received)
                    received.extend(chunk)
                # The line editor echoes the command; the last line is the prompt.
                raw = bytes(received)
                if b"\r\n" in raw:
                    raw = raw.split(b"\r\n", 1)[1]
                raw = raw.rsplit(b"bvstk/", 1)[0]
                log.write(raw); log.flush()
                assert (b"ERR" not in raw) == success, (command, raw)
        return raw if binary else raw.decode("utf-8")

    def policy():
        with connect() as sock:
            return dcp.policy_get_by_id(sock, 1, "mdio", "lan8720")

    def config_snapshot(name):
        with connect() as sock:
            data = b"".join(dcp.FsClient(sock).stream("flash:/config", dcp.FS_TAR, verify_repeats=True))
        (args.output / (name + ".tar")).write_bytes(data)
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            return {item.name.lower(): hashlib.sha256(archive.extractfile(item).read()).hexdigest()
                    for item in archive.getmembers() if item.isfile()}

    created, original_policy = [], None
    save()
    try:
        with connect() as sock:
            wire = Wire(sock)
            assert not wire.request(0, 0)
            wire.request(0, 0, b"extra", status=1)
            wire.request(4, 0, status=2)
            wire.request(5, 0, status=2)
        report["config_before"] = config_snapshot("config-before")
        assert len(report["config_before"]) >= 43
        passed("PING, malformed request, unsupported SPI/UART and configuration backup")

        for command in ("i2c list", "i2c pca9555 info", "i2c sfp policy show rules",
                        "mdio list", "mdio lan8720 info", "mdio lan8720 policy show rules",
                        "mdio lan8720 settings show", "pl list", "fs volumes"):
            report["cli"][command] = cli(command).replace("\r\n", "\n").strip()
        original_policy = policy()
        rule = {"resource": "reg_1f", "val": 0xA050, "mask": 0xA55A}
        assert rule not in original_policy["blacklist"]
        cli("mdio lan8720 policy blacklist add 31 0xF0F0 0xA55A")
        changed = policy()
        assert changed["mode"] == original_policy["mode"]
        assert rule in changed["blacklist"], changed
        cli("mdio lan8720 policy blacklist del 31 0xF0F0 0xA55A")
        assert policy() == original_policy
        passed("Shared CLI grammar, MDIO masks and CLI/DCP2 agreement in RAM")

        for root in roots:
            cli("fs mkdir " + root); created.append(root)
        leaf = roots[0] + "/данные с пробелом.txt"
        cli('fs write "' + leaf + '" "two  spaces"')
        cli('fs append "' + leaf + '" " + tail"')
        cli('fs touch "' + leaf + '"')
        assert cli('fs cat "' + leaf + '"') == "two  spaces + tail"
        content = "0123456789abcdef" * 16
        large = roots[0] + "/large.txt"
        for i in range(20):
            cli("fs " + ("write" if i == 0 else "append") + " " + large + ' "' + content + '"')
        assert cli("fs cat " + large) == content * 20
        cli("fs cp -r " + roots[0] + " " + roots[1] + "/copy")
        cli("fs mv " + roots[1] + "/copy/large.txt " + roots[1] + "/copy/moved.txt")
        cli("tar c " + roots[0] + " " + roots[1] + "/archive.tar")
        cli("tar x " + roots[1] + "/archive.tar " + roots[1] + "/restored")
        assert "данные с пробелом.txt" in cli("tar t " + roots[1] + "/archive.tar")
        with connect() as sock:
            fs = dcp.FsClient(sock)
            info = fs.info(); report["fs_info"] = info
            assert info["max_handles"] == 2 and info["max_block_size"] == 3072
            assert b"".join(fs.stream(leaf, dcp.FS_FILE, block_size=7, verify_repeats=True)) == b"two  spaces + tail"
            assert b"".join(fs.stream(roots[1] + "/restored/large.txt", dcp.FS_FILE)) == (content * 20).encode()
            entries = list(fs.list_entries(roots[1], recursive=True, block_size=19, verify_repeats=True))
            assert any(e["path"] == "copy/moved.txt" for e in entries), entries
            archive = b"".join(fs.stream(roots[1] + "/archive.tar", dcp.FS_FILE))
            handle, block, _ = fs.open(large, dcp.FS_FILE)
            assert handle and block
        if args.os == "neutrino":
            assert cli("fs cat " + roots[1] + "/archive.tar", binary=True) == archive
            # Also exercise the aliases and native cwd translation.
            native = "/flash/" + private
            assert cli("cd " + shlex.quote(native) + ' && bvstkctl fs cat "данные с пробелом.txt"') == "two  spaces + tail"
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            assert tar.extractfile("large.txt").read() == (content * 20).encode()
        passed("CLI files/TAR, Unicode, append, cross-volume copy, paged binary output and DCP2 FS clients")

        with connect() as sock:
            fs = dcp.FsClient(sock)
            try:
                fs.read_block(handle, 0, block)
            except dcp.FsError as error:
                assert error.status == 0x0102
            else:
                raise AssertionError("handle survived reconnect")
            opened = [fs.open(large, dcp.FS_FILE, open_id=50 + n) for n in range(2)]
            for item in opened:
                fs.close(item[0])
        passed("Disconnect closes file handles; reconnect starts a fresh session")

        # Choose a write provably rejected by the current policy, before any PHY I/O.
        if original_policy["mode"] == "whitelist":
            denied = next((r, v) for r in range(32) for v in (0xF13D, 0xFFFF, 0)
                          if not any(x["resource"] == "reg_%02x" % r and (v & x["mask"]) == x["val"]
                                     for x in original_policy["whitelist"]))
        else:
            x = original_policy["blacklist"][0]
            denied = (int(x["resource"].split("_")[1], 16), x["val"])
        register, value = denied
        with connect() as sock:
            wire = Wire(sock)
            wire.request(6, 0x10, struct.pack(">IIIB", 4, 1, 2, 1))
            cli("mdio lan8720 w %d %d" % denied, success=False)
            event = wire.event(6)
            assert event.op == 0x50 and len(event.body) == 28
            assert struct.unpack_from(">H", event.body, 8)[0] == 3
            assert event.body[12:14] == bytes([0, 1])
            wire.request(3, 1, struct.pack(">BBH", 1, register, value), status=3)
            wire.quiet()
            wire.request(6, 0x11)
            cli("mdio lan8720 w %d %d" % denied, success=False)
            wire.quiet()
        passed("NOTIFY passive delivery, timestamp, source/bus/class filters and unsubscribe")

        with connect() as sock:
            wire = Wire(sock)
            wire.request(2, 0x10, bytes([3]))
            cli("i2c pca9555 r 0")
            deadline = time.monotonic() + 5
            while True:
                event = wire.event(2, max(.01, deadline - time.monotonic()))
                text = dcp.decode_stream_event(event, True, True)
                assert "record=" in text and "malformed" not in text, text
                if "source=TELNET" in text and "address=0x20" in text:
                    break
                assert time.monotonic() < deadline, text
            wire.request(2, 0x11)
        with connect() as sock:
            snapshot = CacheClient(sock).snapshot("i2c", "pca9555")
            assert snapshot["resources"]["reg_00"]["valid"]
        passed("Real BVT1 STREAM observations and CACHE share the CLI service state")

        for bus in ("i2c", "mdio", "spi", "uart"):
            cli(bus + " stream fake enable 20 ramp 8")
        try:
            with connect() as sock:
                wire = Wire(sock)
                for service in range(2, 6):
                    wire.request(service, 0x10, bytes([7]))
                for service in range(2, 6):
                    # Frames may arrive in any bus order; retain them independently.
                    event = wire.event(service)
                    text = dcp.decode_stream_event(event, True, True)
                    assert "synthetic=1" in text and "data_len=32" in text and "malformed" not in text, text
                for service in range(2, 6):
                    wire.request(service, 0x11)
            with connect() as sock:
                Wire(sock).quiet(.4)
        finally:
            for bus in ("i2c", "mdio", "spi", "uart"):
                cli(bus + " stream fake disable")
        passed("Four explicit synthetic streams, unchanged client decoding and subscription reset")

        report["config_after"] = config_snapshot("config-after")
        assert report["config_before"] == report["config_after"]
        passed("All original configuration files and contents preserved")
    finally:
        for root in reversed(created):
            try:
                cli("fs rm -r " + root)
            except Exception as error:
                report.setdefault("cleanup_errors", []).append(str(error))
        report["cleanup"] = "failed" if report.get("cleanup_errors") else "complete"
        save(); log.close()


if __name__ == "__main__":
    main()
