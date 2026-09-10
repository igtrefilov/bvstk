#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import struct
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional


MAGIC = b"DCP2"
VERSION = 0x0002
DEFAULT_PORT = 8889

SRV_PING = 0x00
SRV_MEM = 0x01
SRV_I2C = 0x02
SRV_SMI = 0x03
SRV_SPI = 0x04
SRV_UART = 0x05
SRV_NOTIFY = 0x06
SRV_FS = 0x07

OP_FS_INFO, OP_FS_STAT, OP_FS_OPEN, OP_FS_READ, OP_FS_CLOSE = range(5)
FS_FILE, FS_TREE, FS_TAR = 1, 2, 3
FS_RECURSIVE = 1
FS_UNKNOWN_SIZE = (1 << 64) - 1

OP_PING = 0x00
OP_MEM_READ = 0x00
OP_MEM_WRITE = 0x01
OP_NOTIFY_SUBSCRIBE = 0x10
OP_NOTIFY_UNSUBSCRIBE = 0x11
OP_STREAM_SUBSCRIBE = 0x10
OP_STREAM_UNSUBSCRIBE = 0x11

OP_FLAG_RESP = 0x80
OP_FLAG_EVENT = 0x40
MEM_FLAG_AUTOINC = 1 << 0
STREAM_FLAG_RAW_WORDS = 1 << 0
STREAM_FLAG_WITH_TIMESTAMP = 1 << 1
STREAM_FLAG_RESET_LOST_COUNTERS = 1 << 2
STREAM_EVENT_OVERFLOW = 1 << 0

DCP2_MIN_PAYLOAD = 4
DCP2_MAX_PAYLOAD = 4096
MEM_WIDTHS = (8, 16, 32, 64)

STREAM_SERVICES: Dict[str, int] = {
    "i2c": SRV_I2C,
    "smi": SRV_SMI,
    "spi": SRV_SPI,
    "uart": SRV_UART,
}

STREAM_SERVICE_NAMES: Dict[int, str] = {
    value: key.upper() for key, value in STREAM_SERVICES.items()
}

STREAM_FLAG_BITS: Dict[str, int] = {
    "raw": STREAM_FLAG_RAW_WORDS,
    "raw-words": STREAM_FLAG_RAW_WORDS,
    "raw_words": STREAM_FLAG_RAW_WORDS,
    "timestamp": STREAM_FLAG_WITH_TIMESTAMP,
    "with-timestamp": STREAM_FLAG_WITH_TIMESTAMP,
    "with_timestamp": STREAM_FLAG_WITH_TIMESTAMP,
    "reset-lost": STREAM_FLAG_RESET_LOST_COUNTERS,
    "reset_lost": STREAM_FLAG_RESET_LOST_COUNTERS,
}

STATUS_NAMES: Dict[int, str] = {
    0x0000: "OK",
    0x0001: "ERR_MALFORMED",
    0x0002: "ERR_UNSUPPORTED",
    0x0003: "ERR_DENIED",
    0x0004: "ERR_BUSY",
    0x0005: "ERR_TIMEOUT",
    0x0006: "ERR_RANGE",
    0x0007: "ERR_INTERNAL",
    0x0100: "ERR_FS_NOT_FOUND",
    0x0101: "ERR_FS_NOT_READY",
    0x0102: "ERR_FS_BAD_HANDLE",
    0x0103: "ERR_FS_TYPE",
    0x0104: "ERR_FS_IO",
    0x0105: "ERR_FS_CHANGED",
}

NOTIFY_FLAG_WITH_TIMESTAMP = 1 << 0
NOTIFY_FLAG_SNAPSHOT_ON_SUBSCRIBE = 1 << 1

NOTIFY_CLASS_REG_ATTEMPT = 1 << 0
NOTIFY_CLASS_REG_COMMIT = 1 << 1
NOTIFY_CLASS_REG_DENIED = 1 << 2
NOTIFY_CLASS_STATE_CHANGED = 1 << 3
NOTIFY_CLASS_FAULT = 1 << 4

SOURCE_BITS: Dict[str, int] = {
    "telnet": 1 << 0,
    "host": 1 << 1,
    "dcp": 1 << 2,
    "internal": 1 << 3,
}

BUS_BITS: Dict[str, int] = {
    "i2c": 1 << 0,
    "smi": 1 << 1,
    "spi": 1 << 2,
    "uart": 1 << 3,
    "sys": 1 << 4,
}

CLASS_BITS: Dict[str, int] = {
    "attempt": NOTIFY_CLASS_REG_ATTEMPT,
    "commit": NOTIFY_CLASS_REG_COMMIT,
    "denied": NOTIFY_CLASS_REG_DENIED,
    "state": NOTIFY_CLASS_STATE_CHANGED,
    "fault": NOTIFY_CLASS_FAULT,
    "all": (
        NOTIFY_CLASS_REG_ATTEMPT
        | NOTIFY_CLASS_REG_COMMIT
        | NOTIFY_CLASS_REG_DENIED
        | NOTIFY_CLASS_STATE_CHANGED
        | NOTIFY_CLASS_FAULT
    ),
}

EVENT_NAMES: Dict[int, str] = {
    0x0001: "REG_ATTEMPT",
    0x0002: "REG_COMMIT",
    0x0003: "REG_DENIED",
    0x0004: "STATE_CHANGED",
    0x0005: "FAULT",
}

SOURCE_NAMES: Dict[int, str] = {
    0x00: "TELNET",
    0x01: "HOST",
    0x02: "DCP",
    0x03: "INTERNAL",
}

BUS_NAMES: Dict[int, str] = {
    0x00: "I2C",
    0x01: "SMI",
    0x02: "SPI",
    0x03: "UART",
    0x04: "SYS",
}

OP_NAMES: Dict[int, str] = {
    0x00: "READ",
    0x01: "WRITE",
    0x02: "POLICY_CHANGE",
    0x03: "CONFIG_APPLY",
    0x04: "STATE_TOGGLE",
}


@dataclass
class Frame:
    srv: int
    op: int
    seq: int
    body: bytes

    @property
    def is_response(self) -> bool:
        return bool(self.op & OP_FLAG_RESP)

    @property
    def is_event(self) -> bool:
        return bool(self.op & OP_FLAG_EVENT)

    @property
    def opcode(self) -> int:
        return self.op & 0x3F


def recv_exact(sock: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("connection closed by peer")
        data.extend(chunk)
    return bytes(data)


def read_frame(sock: socket.socket) -> Frame:
    hdr = recv_exact(sock, 8)
    magic, version, dcp_len = struct.unpack(">4sHH", hdr)
    if magic != MAGIC:
        raise ValueError(f"bad magic: {magic!r}")
    if version != VERSION:
        raise ValueError(f"unsupported version: 0x{version:04X}")
    if dcp_len < DCP2_MIN_PAYLOAD or dcp_len > DCP2_MAX_PAYLOAD:
        raise ValueError(f"invalid DCP payload length: {dcp_len}")
    payload = recv_exact(sock, dcp_len)
    if len(payload) < DCP2_MIN_PAYLOAD:
        raise ValueError("short DCP payload")
    return Frame(srv=payload[0], op=payload[1], seq=struct.unpack(">H", payload[2:4])[0], body=payload[4:])


def build_frame(srv: int, op: int, seq: int, body: bytes = b"") -> bytes:
    payload = bytes([srv, op]) + struct.pack(">H", seq) + body
    if len(payload) < DCP2_MIN_PAYLOAD or len(payload) > DCP2_MAX_PAYLOAD:
        raise ValueError(f"invalid DCP payload length: {len(payload)}")
    return struct.pack(">4sHH", MAGIC, VERSION, len(payload)) + payload


def status_name(status: int) -> str:
    return STATUS_NAMES.get(status, f"0x{status:04X}")


def parse_mask_arg(raw: str, table: Dict[str, int]) -> int:
    raw = raw.strip().lower()
    if raw.startswith("0x"):
        return int(raw, 16)
    if raw.isdigit():
        return int(raw, 10)
    value = 0
    for part in raw.split(","):
        key = part.strip()
        if not key:
            continue
        if key not in table:
            valid = ", ".join(sorted(table))
            raise argparse.ArgumentTypeError(f"unknown value '{key}', valid: {valid} or hex mask")
        value |= table[key]
    return value


def decode_notify_event(body: bytes, with_timestamp: bool) -> str:
    offset = 0
    time_us: Optional[int] = None
    if with_timestamp:
        if len(body) < 28:
            return f"malformed NOTIFY_EVENT: expected at least 28 bytes, got {len(body)}"
        time_us = struct.unpack_from(">Q", body, offset)[0]
        offset += 8
    else:
        if len(body) < 20:
            return f"malformed NOTIFY_EVENT: expected at least 20 bytes, got {len(body)}"

    ev_type, status = struct.unpack_from(">HH", body, offset)
    offset += 4
    source, bus, op_kind, _reserved = struct.unpack_from(">BBBB", body, offset)
    offset += 4
    arg0, arg1, arg2 = struct.unpack_from(">III", body, offset)

    parts = []
    if time_us is not None:
        parts.append(f"time_us={time_us}")
    parts.append(f"ev={EVENT_NAMES.get(ev_type, f'0x{ev_type:04X}')}")
    parts.append(f"status={status_name(status)}")
    parts.append(f"source={SOURCE_NAMES.get(source, hex(source))}")
    parts.append(f"bus={BUS_NAMES.get(bus, hex(bus))}")
    parts.append(f"op={OP_NAMES.get(op_kind, hex(op_kind))}")

    if bus == 0x00 and op_kind in (0x00, 0x01):
        if op_kind == 0x00:
            parts.append(f"addr7=0x{arg0:02X}")
            parts.append(f"reg=0x{arg1:02X}")
            parts.append(f"val=0x{arg2:02X}")
        else:
            parts.append(f"addr7=0x{arg0:02X}")
            parts.append(f"reg=0x{arg1:02X}")
            parts.append(f"val=0x{arg2:02X}")
    elif bus == 0x01 and op_kind in (0x00, 0x01):
        parts.append(f"phy=0x{arg0:02X}")
        parts.append(f"reg=0x{arg1:02X}")
        parts.append(f"val=0x{arg2:04X}")
    else:
        parts.append(f"arg0=0x{arg0:08X}")
        parts.append(f"arg1=0x{arg1:08X}")
        parts.append(f"arg2=0x{arg2:08X}")

    return " ".join(parts)


def wait_for_response(sock: socket.socket, expected_srv: int, expected_opcode: int, expected_seq: int) -> Frame:
    while True:
        frame = read_frame(sock)
        if frame.is_event:
            print(f"[event-before-response] srv=0x{frame.srv:02X} op=0x{frame.op:02X} seq={frame.seq}", flush=True)
            continue
        if not frame.is_response:
            raise ValueError(f"unexpected non-response frame op=0x{frame.op:02X}")
        if frame.srv != expected_srv or frame.opcode != expected_opcode or frame.seq != expected_seq:
            raise ValueError(
                f"unexpected response srv=0x{frame.srv:02X} op=0x{frame.op:02X} seq={frame.seq}, "
                f"expected srv=0x{expected_srv:02X} opcode=0x{expected_opcode:02X} seq={expected_seq}"
            )
        return frame


def parse_status_from_response(frame: Frame) -> int:
    if len(frame.body) < 2:
        raise ValueError("response body has no status field")
    return struct.unpack(">H", frame.body[:2])[0]


def parse_uint(raw: str, field: str, maximum: int) -> int:
    try:
        value = int(raw, 0)
    except ValueError as exc:
        raise ValueError(f"invalid {field}: {raw!r}") from exc
    if value < 0 or value > maximum:
        raise ValueError(f"{field} is out of range: {raw!r}")
    return value


def validate_mem_parameters(address: int,
                            width: int,
                            count: int,
                            autoinc: bool,
                            data_size: int = 0) -> int:
    if width not in MEM_WIDTHS:
        raise ValueError(f"unsupported MEM width: {width}; use one of {MEM_WIDTHS}")
    if count < 1 or count > 0xFFFF:
        raise ValueError("MEM count must be in range 1..65535")
    if address < 0 or address > 0xFFFFFFFF:
        raise ValueError("MEM address must be in range 0..0xFFFFFFFF")

    width_bytes = width // 8
    if data_size > DCP2_MAX_PAYLOAD - 12:
        raise ValueError("MEM_WRITE request is too large for DCP2")
    if count * width_bytes > DCP2_MAX_PAYLOAD - 6:
        raise ValueError("MEM_READ response is too large for DCP2")
    if autoinc and address + (count - 1) * width_bytes > 0xFFFFFFFF:
        raise ValueError("MEM address range overflows 32-bit address space")
    return width_bytes


def mem_read(sock: socket.socket,
             seq: int,
             address: int,
             width: int,
             count: int,
             autoinc: bool) -> list[int]:
    width_bytes = validate_mem_parameters(address, width, count, autoinc)
    flags = MEM_FLAG_AUTOINC if autoinc else 0
    body = struct.pack(">BBIH", flags, width, address, count)
    sock.sendall(build_frame(SRV_MEM, OP_MEM_READ, seq, body))
    frame = wait_for_response(sock, SRV_MEM, OP_MEM_READ, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        raise RuntimeError(f"MEM_READ failed: {status_name(status)}")

    data = frame.body[2:]
    expected_size = count * width_bytes
    if len(data) != expected_size:
        raise ValueError(
            f"invalid MEM_READ response length: expected {expected_size}, got {len(data)}"
        )
    return [
        int.from_bytes(data[offset:offset + width_bytes], "big")
        for offset in range(0, expected_size, width_bytes)
    ]


def mem_write(sock: socket.socket,
              seq: int,
              address: int,
              width: int,
              values: list[int],
              autoinc: bool) -> None:
    count = len(values)
    width_bytes = validate_mem_parameters(
        address,
        width,
        count,
        autoinc,
        data_size=count * (width // 8),
    )
    max_value = (1 << width) - 1
    if any(value < 0 or value > max_value for value in values):
        raise ValueError(f"MEM value must fit in {width} bits")

    flags = MEM_FLAG_AUTOINC if autoinc else 0
    data = b"".join(value.to_bytes(width_bytes, "big") for value in values)
    body = struct.pack(">BBIH", flags, width, address, count) + data
    sock.sendall(build_frame(SRV_MEM, OP_MEM_WRITE, seq, body))
    frame = wait_for_response(sock, SRV_MEM, OP_MEM_WRITE, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        raise RuntimeError(f"MEM_WRITE failed: {status_name(status)}")
    if len(frame.body) != 2:
        raise ValueError(f"invalid MEM_WRITE response length: {len(frame.body)}")


def ping(sock: socket.socket, seq: int) -> None:
    sock.sendall(build_frame(SRV_PING, OP_PING, seq))
    frame = wait_for_response(sock, SRV_PING, OP_PING, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        raise RuntimeError(f"PING failed: {status_name(status)}")


def subscribe_notify(sock: socket.socket, seq: int, class_mask: int, source_mask: int, bus_mask: int, flags: int) -> None:
    body = struct.pack(">IIIB", class_mask, source_mask, bus_mask, flags)
    sock.sendall(build_frame(SRV_NOTIFY, OP_NOTIFY_SUBSCRIBE, seq, body))
    frame = wait_for_response(sock, SRV_NOTIFY, OP_NOTIFY_SUBSCRIBE, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        raise RuntimeError(f"NOTIFY_SUBSCRIBE failed: {status_name(status)}")


def unsubscribe_notify(sock: socket.socket, seq: int) -> None:
    sock.sendall(build_frame(SRV_NOTIFY, OP_NOTIFY_UNSUBSCRIBE, seq))
    frame = wait_for_response(sock, SRV_NOTIFY, OP_NOTIFY_UNSUBSCRIBE, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        raise RuntimeError(f"NOTIFY_UNSUBSCRIBE failed: {status_name(status)}")


def stream_flag_names(flags: int) -> str:
    names = []
    if flags & STREAM_FLAG_RAW_WORDS:
        names.append("raw")
    if flags & STREAM_FLAG_WITH_TIMESTAMP:
        names.append("timestamp")
    if flags & STREAM_FLAG_RESET_LOST_COUNTERS:
        names.append("reset-lost")
    return ",".join(names) if names else "none"


def subscribe_stream(sock: socket.socket, seq: int, service: int, flags: int) -> None:
    body = bytes([flags])
    sock.sendall(build_frame(service, OP_STREAM_SUBSCRIBE, seq, body))
    frame = wait_for_response(sock, service, OP_STREAM_SUBSCRIBE, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        name = STREAM_SERVICE_NAMES.get(service, f"0x{service:02X}")
        raise RuntimeError(f"{name} STREAM_SUBSCRIBE failed: {status_name(status)}")
    if len(frame.body) != 2:
        raise ValueError(f"invalid STREAM_SUBSCRIBE response length: {len(frame.body)}")


def unsubscribe_stream(sock: socket.socket, seq: int, service: int) -> None:
    sock.sendall(build_frame(service, OP_STREAM_UNSUBSCRIBE, seq))
    frame = wait_for_response(sock, service, OP_STREAM_UNSUBSCRIBE, seq)
    status = parse_status_from_response(frame)
    if status != 0:
        name = STREAM_SERVICE_NAMES.get(service, f"0x{service:02X}")
        raise RuntimeError(f"{name} STREAM_UNSUBSCRIBE failed: {status_name(status)}")
    if len(frame.body) != 2:
        raise ValueError(f"invalid STREAM_UNSUBSCRIBE response length: {len(frame.body)}")


def decode_stream_event(frame: Frame, with_timestamp: bool, raw_words: bool) -> str:
    if not frame.is_event or frame.opcode != OP_STREAM_SUBSCRIBE:
        return f"unexpected stream frame srv=0x{frame.srv:02X} op=0x{frame.op:02X} seq={frame.seq}"
    if frame.seq != 0:
        return f"malformed stream event: expected seq=0, got {frame.seq}"

    offset = 0
    time_us: Optional[int] = None
    if with_timestamp:
        if len(frame.body) < 15:
            return f"malformed PL_STREAM_EVENT: expected at least 15 bytes, got {len(frame.body)}"
        time_us = struct.unpack_from(">Q", frame.body, offset)[0]
        offset += 8
    if len(frame.body) < offset + 7:
        return (
            f"malformed PL_STREAM_EVENT: expected at least {offset + 7} bytes, "
            f"got {len(frame.body)}"
        )

    ev_flags = frame.body[offset]
    offset += 1
    lost_delta = struct.unpack_from(">I", frame.body, offset)[0]
    offset += 4
    data_len = struct.unpack_from(">H", frame.body, offset)[0]
    offset += 2
    data = frame.body[offset:]
    if len(data) != data_len:
        return (
            f"malformed PL_STREAM_EVENT: data_len={data_len}, "
            f"actual={len(data)}"
        )
    if raw_words and data_len % 4 != 0:
        return f"malformed PL_STREAM_EVENT: RAW_WORDS data_len={data_len} is not divisible by 4"

    parts = [
        f"stream={STREAM_SERVICE_NAMES.get(frame.srv, f'0x{frame.srv:02X}')}",
    ]
    if time_us is not None:
        parts.append(f"time_us={time_us}")
    parts.append(f"ev_flags=0x{ev_flags:02X}")
    if ev_flags & STREAM_EVENT_OVERFLOW:
        parts.append("overflow=1")
    parts.append(f"lost_delta={lost_delta}")
    parts.append(f"data_len={data_len}")
    if raw_words:
        words = [
            int.from_bytes(data[index:index + 4], "big")
            for index in range(0, data_len, 4)
        ]
        parts.append("words=" + ",".join(f"0x{word:08X}" for word in words))
    else:
        parts.append(f"data={data.hex(' ')}")
    return " ".join(parts)


def run_stream_monitor(args: argparse.Namespace) -> int:
    try:
        flags = parse_mask_arg(args.stream_flags, STREAM_FLAG_BITS)
        if flags & ~(STREAM_FLAG_RAW_WORDS | STREAM_FLAG_WITH_TIMESTAMP | STREAM_FLAG_RESET_LOST_COUNTERS):
            raise ValueError("stream flags contain reserved bits; use raw,timestamp,reset-lost or a mask 0..0x07")

        services = []
        for service_name in args.streams:
            service = STREAM_SERVICES[service_name]
            if service not in services:
                services.append(service)

        stop = False

        def handle_signal(_signum: int, _frame: object) -> None:
            nonlocal stop
            stop = True

        signal.signal(signal.SIGINT, handle_signal)
        signal.signal(signal.SIGTERM, handle_signal)

        seq = 1
        subscribed = []
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            sock.settimeout(args.timeout)
            print(f"connected to {args.host}:{args.port}")
            ping(sock, seq)
            print("PING -> OK")
            seq += 1

            for service in services:
                subscribe_stream(sock, seq, service, flags)
                subscribed.append(service)
                print(
                    f"STREAM_SUBSCRIBE -> OK service={STREAM_SERVICE_NAMES[service]} "
                    f"flags=0x{flags:02X} ({stream_flag_names(flags)})"
                )
                seq += 1

            print("monitoring streams, press Ctrl+C to stop")
            try:
                while not stop:
                    try:
                        frame = read_frame(sock)
                        if frame.is_event and frame.srv in services and frame.opcode == OP_STREAM_SUBSCRIBE:
                            print(
                                decode_stream_event(
                                    frame,
                                    with_timestamp=bool(flags & STREAM_FLAG_WITH_TIMESTAMP),
                                    raw_words=bool(flags & STREAM_FLAG_RAW_WORDS),
                                ),
                                flush=True,
                            )
                        else:
                            print_frame(frame, with_timestamp=False)
                    except socket.timeout:
                        continue
            finally:
                for service in subscribed:
                    try:
                        unsubscribe_stream(sock, seq, service)
                        print(f"STREAM_UNSUBSCRIBE -> OK service={STREAM_SERVICE_NAMES[service]}")
                    except Exception as exc:
                        print(
                            f"stream unsubscribe failed service={STREAM_SERVICE_NAMES.get(service, service)}: {exc}",
                            file=sys.stderr,
                        )
                    seq += 1
    except KeyboardInterrupt:
        stop = True
    except (ConnectionError, OSError, RuntimeError, ValueError, argparse.ArgumentTypeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


def run_ping(args: argparse.Namespace) -> int:
    try:
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            sock.settimeout(args.timeout)
            print(f"connected to {args.host}:{args.port}")
            ping(sock, 1)
            print("PING -> OK")
    except (ConnectionError, OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


class FsError(RuntimeError):
    def __init__(self, status: int):
        self.status = status
        super().__init__(status_name(status))


class FsClient:
    """DCP2 FS v1 reader. A socket and its receive buffer belong to this client.

    Partial frames survive response timeouts. Retries use a fresh seq but the
    same open_id or block number, and late responses are consumed and ignored.
    """

    def __init__(self, sock: socket.socket, timeout: float = 5.0, retries: int = 2):
        self.sock = sock
        self.timeout = timeout
        self.retries = retries
        self.sequence = 0
        self.open_id = 0
        self.received = bytearray()

    def _read_frame(self, deadline: float) -> Frame:
        while True:
            if len(self.received) >= 8:
                magic, version, length = struct.unpack_from(">4sHH", self.received)
                if magic != MAGIC or version != VERSION or not 4 <= length <= DCP2_MAX_PAYLOAD:
                    raise ValueError("invalid DCP2 frame header")
                if len(self.received) >= 8 + length:
                    payload = bytes(self.received[8:8 + length])
                    del self.received[:8 + length]
                    return Frame(payload[0], payload[1], struct.unpack_from(">H", payload, 2)[0], payload[4:])
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise socket.timeout("FS response deadline expired")
            self.sock.settimeout(remaining)
            data = self.sock.recv(8192)
            if not data:
                raise ConnectionError("connection closed before the FS response")
            self.received.extend(data)

    def request(self, opcode: int, body: bytes = b"") -> bytes:
        for attempt in range(self.retries + 1):
            self.sequence = self.sequence % 0xFFFF + 1
            seq = self.sequence
            self.sock.settimeout(self.timeout)
            # A failed send may have transmitted only a prefix: do not resend it.
            self.sock.sendall(build_frame(SRV_FS, opcode, seq, body))
            deadline = time.monotonic() + self.timeout
            try:
                while True:
                    frame = self._read_frame(deadline)
                    if frame.is_event:
                        continue
                    if not frame.is_response:
                        raise ValueError("unexpected request from FS server")
                    if frame.seq != seq:
                        continue
                    if frame.srv != SRV_FS or frame.opcode != opcode:
                        raise ValueError("FS response service/opcode mismatch")
                    status = parse_status_from_response(frame)
                    if status:
                        if len(frame.body) != 2:
                            raise ValueError("FS error response has unexpected data")
                        raise FsError(status)
                    return frame.body[2:]
            except (socket.timeout, FsError) as exc:
                if isinstance(exc, FsError) and exc.status not in (4, 5, 0x0101):
                    raise
                if attempt == self.retries:
                    raise
                time.sleep(min(0.05 * (attempt + 1), self.timeout))
        raise RuntimeError("FS retry loop exhausted")

    @staticmethod
    def _path(path: str) -> bytes:
        encoded = path.encode("utf-8")
        if not encoded or len(encoded) > 0xFFFF or b"\0" in encoded:
            raise ValueError("invalid FS path length or embedded NUL")
        return struct.pack(">H", len(encoded)) + encoded

    def info(self) -> dict:
        body = self.request(OP_FS_INFO)
        if len(body) < 20:
            raise ValueError("short FS_INFO response")
        version, capabilities, block, path, depth, handles, idle, count = struct.unpack_from(">HIHHHHIH", body)
        if version != 1 or not 1 <= block <= 4078 or not path or not depth or not handles or not idle:
            raise ValueError("unsupported FS version or invalid limits")
        offset, volumes = 20, []
        for _ in range(count):
            if offset + 2 > len(body):
                raise ValueError("truncated FS volume")
            state, length = body[offset:offset + 2]
            offset += 2
            if state > 2 or not length or offset + length > len(body):
                raise ValueError("invalid FS volume record")
            name = body[offset:offset + length].decode("utf-8")
            offset += length
            volumes.append({"root": name + ":/", "state": ("not_ready", "ready", "unsupported")[state]})
        if offset != len(body):
            raise ValueError("trailing bytes in FS_INFO")
        return {"fs_version": version, "capabilities": capabilities, "max_block_size": block,
                "max_path_bytes": path, "max_depth": depth, "max_handles": handles,
                "idle_timeout_ms": idle, "volumes": volumes}

    def stat(self, path: str) -> dict:
        body = self.request(OP_FS_STAT, self._path(path))
        if len(body) != 10:
            raise ValueError("invalid FS_STAT response")
        kind, attributes, size = struct.unpack(">BBQ", body)
        if kind not in (1, 2) or (kind == 2 and size):
            raise ValueError("invalid FS object type/size")
        return {"path": path, "type": "file" if kind == 1 else "directory", "size": size, "attributes": attributes}

    def open(self, path: str, kind: int, recursive: bool = False, block_size: int = 3072,
             open_id: Optional[int] = None) -> tuple[int, int, int]:
        if not 0 <= block_size <= 4078:
            raise ValueError("FS block size must be in range 0..4078")
        if open_id is None:
            self.open_id += 1
            open_id = self.open_id
        body = self.request(OP_FS_OPEN, struct.pack(">IBBH", open_id, kind, int(recursive), block_size) + self._path(path))
        if len(body) != 14:
            raise ValueError("invalid FS_OPEN response")
        handle, accepted, total = struct.unpack(">IHQ", body)
        if not handle or not 1 <= accepted <= 4078 or (block_size and accepted > block_size):
            raise ValueError("invalid FS handle or block size")
        if kind == FS_FILE and total == FS_UNKNOWN_SIZE:
            raise ValueError("FILE stream has unknown size")
        return handle, accepted, total

    def close(self, handle: int) -> None:
        if self.request(OP_FS_CLOSE, struct.pack(">I", handle)):
            raise ValueError("unexpected FS_CLOSE response data")

    def read_block(self, handle: int, block_no: int, block_size: int) -> tuple[bytes, bool]:
        body = self.request(OP_FS_READ, struct.pack(">II", handle, block_no))
        if len(body) < 12:
            raise ValueError("short FS_READ response")
        received_handle, received_no, flags, reserved, length = struct.unpack_from(">IIBBH", body)
        if (received_handle != handle or received_no != block_no or reserved or flags & ~1 or
                length != len(body) - 12 or length > block_size or (not length and not flags & 1)):
            raise ValueError("invalid FS_READ response")
        return body[12:], bool(flags & 1)

    def stream(self, path: str, kind: int, recursive: bool = False, block_size: int = 3072,
               verify_repeats: bool = False):
        handle, accepted, total = self.open(path, kind, recursive, block_size)
        received = 0
        try:
            for block_no in range(1 << 32):
                data, eof = self.read_block(handle, block_no, accepted)
                if verify_repeats and self.read_block(handle, block_no, accepted) != (data, eof):
                    raise ValueError("repeated FS block differs")
                received += len(data)
                if total != FS_UNKNOWN_SIZE and (received > total or (eof and received != total)):
                    raise ValueError("FS stream size differs from FS_OPEN")
                yield data
                if eof:
                    break
            else:
                raise ValueError("FS block number exhausted")
        finally:
            # Preserve the original exception on a failed or abandoned transfer.
            if sys.exc_info()[0] is None:
                self.close(handle)
            else:
                try:
                    self.close(handle)
                except (ConnectionError, OSError, RuntimeError, ValueError):
                    pass

    def list_entries(self, path: str, recursive: bool = False, block_size: int = 3072,
                     verify_repeats: bool = False):
        pending = bytearray()
        stream = self.stream(path, FS_TREE, recursive, block_size, verify_repeats)
        try:
            for data in stream:
                pending.extend(data)
                while len(pending) >= 2:
                    entry_len = struct.unpack_from(">H", pending)[0]
                    if entry_len < 15:
                        raise ValueError("invalid TREE entry length")
                    if len(pending) < entry_len:
                        break
                    _, kind, reserved, size, length = struct.unpack_from(">HBBQH", pending)
                    if kind not in (1, 2) or reserved or entry_len != 14 + length or (kind == 2 and size):
                        raise ValueError("invalid TREE entry")
                    name = bytes(pending[14:entry_len]).decode("utf-8")
                    if (not name or name.startswith("/") or ":" in name or "\\" in name or
                            any(part in ("", ".", "..") for part in name.split("/")) or "\0" in name):
                        raise ValueError("invalid TREE relative path")
                    del pending[:entry_len]
                    yield {"path": name, "type": "file" if kind == 1 else "directory", "size": size}
            if pending:
                raise ValueError("TREE ended inside a record")
        finally:
            stream.close()


def run_fs_operation(args: argparse.Namespace) -> int:
    temporary = None
    try:
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            client = FsClient(sock, args.timeout, args.fs_retries)
            if args.fs_info:
                print(json.dumps(client.info(), ensure_ascii=False, indent=2))
            elif args.fs_stat:
                print(json.dumps(client.stat(args.fs_stat), ensure_ascii=False, indent=2))
            elif args.fs_list:
                for entry in client.list_entries(args.fs_list, args.recursive, args.fs_block_size, args.fs_verify_repeats):
                    print(json.dumps(entry, ensure_ascii=False), flush=True)
            else:
                path = args.fs_read or args.fs_tar
                kind = FS_FILE if args.fs_read else FS_TAR
                stream = client.stream(path, kind, block_size=args.fs_block_size, verify_repeats=args.fs_verify_repeats)
                count = 0
                try:
                    if args.output:
                        destination = Path(args.output)
                        with tempfile.NamedTemporaryFile(mode="wb", prefix=destination.name + ".", suffix=".part",
                                                         dir=destination.parent, delete=False) as output:
                            temporary = output.name
                            for data in stream:
                                output.write(data)
                                count += len(data)
                        os.replace(temporary, destination)
                        temporary = None
                        print(f"FS -> OK {count} bytes saved to {destination}", file=sys.stderr)
                    else:
                        for data in stream:
                            sys.stdout.buffer.write(data)
                        sys.stdout.buffer.flush()
                finally:
                    stream.close()
    except (ConnectionError, OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    finally:
        if temporary is not None:
            os.unlink(temporary)
    return 0


def run_memory_operation(args: argparse.Namespace) -> int:
    try:
        if args.mem_read is not None:
            address = parse_uint(args.mem_read, "MEM address", 0xFFFFFFFF)
            count = args.count if args.count is not None else 1
        else:
            if args.mem_write is None or len(args.mem_write) < 2:
                raise ValueError("--mem-write requires ADDRESS and at least one VALUE")
            address = parse_uint(args.mem_write[0], "MEM address", 0xFFFFFFFF)
            values = [
                parse_uint(raw, "MEM value", (1 << args.width) - 1)
                for raw in args.mem_write[1:]
            ]
            if args.count is not None and args.count != len(values):
                raise ValueError(
                    f"--count={args.count} does not match number of write values ({len(values)})"
                )

        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            sock.settimeout(args.timeout)
            print(f"connected to {args.host}:{args.port}")
            ping(sock, 1)
            print("PING -> OK")

            if args.mem_read is not None:
                values = mem_read(
                    sock,
                    2,
                    address,
                    args.width,
                    count,
                    args.autoinc,
                )
                print(
                    f"MEM_READ -> OK address=0x{address:08X} width={args.width} "
                    f"count={count} autoinc={int(args.autoinc)}"
                )
                width_digits = args.width // 4
                width_bytes = args.width // 8
                for index, value in enumerate(values):
                    current_address = address + (index * width_bytes if args.autoinc else 0)
                    print(
                        f"  [{index}] 0x{current_address:08X} = "
                        f"0x{value:0{width_digits}X}"
                    )
            else:
                mem_write(sock, 2, address, args.width, values, args.autoinc)
                print(
                    f"MEM_WRITE -> OK address=0x{address:08X} width={args.width} "
                    f"count={len(values)} autoinc={int(args.autoinc)}"
                )
    except (ConnectionError, OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


def print_frame(frame: Frame, with_timestamp: bool) -> None:
    if frame.is_event:
        if frame.srv == SRV_NOTIFY and frame.opcode == OP_NOTIFY_SUBSCRIBE:
            print(decode_notify_event(frame.body, with_timestamp), flush=True)
        else:
            print(
                f"event srv=0x{frame.srv:02X} op=0x{frame.op:02X} seq={frame.seq} len={len(frame.body)}",
                flush=True,
            )
        return

    if frame.is_response:
        status = parse_status_from_response(frame)
        print(
            f"response srv=0x{frame.srv:02X} opcode=0x{frame.opcode:02X} seq={frame.seq} status={status_name(status)}",
            flush=True,
        )
        return

    print(f"request-from-server? srv=0x{frame.srv:02X} op=0x{frame.op:02X} seq={frame.seq}", flush=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "DCP2 client: monitor NOTIFY or PL streams, or execute one-shot "
            "MEM read/write or FS read/tree/TAR operations."
        )
    )
    parser.add_argument("host", help="device IP or hostname")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"DCP2 TCP port (default: {DEFAULT_PORT})")
    operation = parser.add_mutually_exclusive_group()
    operation.add_argument(
        "--ping",
        "--ping-only",
        dest="ping_only",
        action="store_true",
        help="send PING and exit",
    )
    operation.add_argument(
        "--mem-read",
        metavar="ADDRESS",
        help="read MEM at ADDRESS and exit (hex or decimal)",
    )
    operation.add_argument(
        "--mem-write",
        nargs="+",
        metavar="VALUE",
        help="write MEM: ADDRESS VALUE [VALUE ...] and exit",
    )
    operation.add_argument(
        "--stream",
        dest="streams",
        action="append",
        choices=tuple(STREAM_SERVICES),
        help="subscribe to a PL stream; repeat for i2c, smi, spi or uart",
    )
    operation.add_argument("--fs-info", action="store_true", help="show FS limits and volumes as JSON")
    operation.add_argument("--fs-stat", metavar="PATH", help="show file/directory metadata as JSON")
    operation.add_argument("--fs-list", metavar="PATH", help="list directory records as JSON lines")
    operation.add_argument("--fs-read", metavar="PATH", help="read a file to stdout or --output")
    operation.add_argument("--fs-tar", metavar="PATH", help="export a recursive TAR to stdout or --output")
    parser.add_argument("--recursive", action="store_true", help="recursively traverse --fs-list")
    parser.add_argument("--output", metavar="FILE", help="save --fs-read/--fs-tar atomically after successful completion")
    parser.add_argument("--fs-block-size", type=int, default=3072, help="FS block bytes (0 selects server default)")
    parser.add_argument("--fs-retries", type=int, default=2, help="FS retries per request (default: 2)")
    parser.add_argument("--fs-verify-repeats", action="store_true", help="read every FS block twice and compare")
    parser.add_argument(
        "--width",
        type=int,
        choices=MEM_WIDTHS,
        default=32,
        help="MEM element width in bits (default: 32)",
    )
    parser.add_argument(
        "--count",
        type=int,
        default=None,
        help="MEM read element count; for write it must match the number of values",
    )
    parser.add_argument(
        "--autoinc",
        action="store_true",
        help="increment MEM address by width/8 for each element",
    )
    parser.add_argument(
        "--stream-flags",
        default="raw,timestamp,reset-lost",
        help="stream flags: raw,timestamp,reset-lost or a hex mask (default: raw,timestamp,reset-lost)",
    )
    parser.add_argument(
        "--classes",
        default="all",
        help="notify class mask: comma list of attempt,commit,denied,state,fault,all or hex mask",
    )
    parser.add_argument(
        "--sources",
        default="telnet,host,dcp,internal",
        help="source mask: comma list of telnet,host,dcp,internal or hex mask",
    )
    parser.add_argument(
        "--buses",
        default="i2c",
        help="bus mask: comma list of i2c,smi,spi,uart,sys or hex mask",
    )
    parser.add_argument("--no-timestamp", action="store_true", help="do not request time_us in NOTIFY_EVENT")
    parser.add_argument(
        "--snapshot",
        action="store_true",
        help="request SNAPSHOT_ON_SUBSCRIBE if the server supports it",
    )
    parser.add_argument("--timeout", type=float, default=5.0, help="socket timeout in seconds for connect and I/O")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.host.isdigit() and args.port == DEFAULT_PORT:
        parser.error(
            f"'{args.host}' looks like a port, not a host. "
            "Use: monitor_notify.py <device-ip> --port <port>"
        )

    fs_operation = args.fs_info or args.fs_stat or args.fs_list or args.fs_read or args.fs_tar
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if fs_operation:
        if args.count is not None or args.autoinc or args.width != 32:
            parser.error("FS operations cannot be combined with MEM options")
        if args.recursive and not args.fs_list:
            parser.error("--recursive requires --fs-list; TAR always includes subdirectories")
        if args.output and not (args.fs_read or args.fs_tar):
            parser.error("--output requires --fs-read or --fs-tar")
        if not 0 <= args.fs_block_size <= 4078 or not 0 <= args.fs_retries <= 10:
            parser.error("FS block size must be 0..4078 and retries 0..10")
        return run_fs_operation(args)
    if args.recursive or args.output or args.fs_verify_repeats or args.fs_block_size != 3072 or args.fs_retries != 2:
        parser.error("FS options require an --fs-* operation")

    if args.ping_only:
        if args.count is not None or args.autoinc or args.width != 32:
            parser.error("--ping cannot be combined with MEM options")
        return run_ping(args)

    if args.streams:
        if args.count is not None or args.autoinc or args.width != 32:
            parser.error("--stream cannot be combined with MEM options")
        return run_stream_monitor(args)

    class_mask = parse_mask_arg(args.classes, CLASS_BITS)
    source_mask = parse_mask_arg(args.sources, SOURCE_BITS)
    bus_mask = parse_mask_arg(args.buses, BUS_BITS)

    if args.mem_read is not None or args.mem_write is not None:
        if args.count is not None and (args.count < 1 or args.count > 0xFFFF):
            parser.error("--count must be in range 1..65535")
        return run_memory_operation(args)

    flags = 0
    if not args.no_timestamp:
        flags |= NOTIFY_FLAG_WITH_TIMESTAMP
    if args.snapshot:
        flags |= NOTIFY_FLAG_SNAPSHOT_ON_SUBSCRIBE

    stop = False

    def handle_signal(_signum: int, _frame: object) -> None:
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    seq = 1

    try:
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            sock.settimeout(args.timeout)
            print(f"connected to {args.host}:{args.port}")
            ping(sock, seq)
            print("PING -> OK")
            seq += 1

            subscribe_notify(sock, seq, class_mask, source_mask, bus_mask, flags)
            print(
                "NOTIFY_SUBSCRIBE -> OK "
                f"class_mask=0x{class_mask:08X} source_mask=0x{source_mask:08X} bus_mask=0x{bus_mask:08X} flags=0x{flags:02X}"
            )
            seq += 1
            print("monitoring events, press Ctrl+C to stop")

            while not stop:
                try:
                    print_frame(read_frame(sock), with_timestamp=bool(flags & NOTIFY_FLAG_WITH_TIMESTAMP))
                except socket.timeout:
                    continue

            try:
                unsubscribe_notify(sock, seq)
                print("NOTIFY_UNSUBSCRIBE -> OK")
            except Exception as exc:
                print(f"unsubscribe failed: {exc}", file=sys.stderr)
    except KeyboardInterrupt:
        stop = True
    except (ConnectionError, OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
