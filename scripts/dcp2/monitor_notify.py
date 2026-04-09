#!/usr/bin/env python3
from __future__ import annotations

import argparse
import signal
import socket
import struct
import sys
from dataclasses import dataclass
from typing import Dict, Optional


MAGIC = b"DCP2"
VERSION = 0x0002
DEFAULT_PORT = 8889

SRV_PING = 0x00
SRV_NOTIFY = 0x06

OP_PING = 0x00
OP_NOTIFY_SUBSCRIBE = 0x10
OP_NOTIFY_UNSUBSCRIBE = 0x11

OP_FLAG_RESP = 0x80
OP_FLAG_EVENT = 0x40

STATUS_NAMES: Dict[int, str] = {
    0x0000: "OK",
    0x0001: "ERR_MALFORMED",
    0x0002: "ERR_UNSUPPORTED",
    0x0003: "ERR_DENIED",
    0x0004: "ERR_BUSY",
    0x0005: "ERR_TIMEOUT",
    0x0006: "ERR_RANGE",
    0x0007: "ERR_INTERNAL",
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
    payload = recv_exact(sock, dcp_len)
    if len(payload) < 4:
        raise ValueError("short DCP payload")
    return Frame(srv=payload[0], op=payload[1], seq=struct.unpack(">H", payload[2:4])[0], body=payload[4:])


def build_frame(srv: int, op: int, seq: int, body: bytes = b"") -> bytes:
    payload = bytes([srv, op]) + struct.pack(">H", seq) + body
    return struct.pack(">4sHH", MAGIC, VERSION, len(payload)) + payload


def status_name(status: int) -> str:
    return STATUS_NAMES.get(status, f"0x{status:04X}")


def parse_mask_arg(raw: str, table: Dict[str, int]) -> int:
    raw = raw.strip().lower()
    if raw.startswith("0x"):
        return int(raw, 16)
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
            print(f"[event-before-subscribe] srv=0x{frame.srv:02X} op=0x{frame.op:02X} seq={frame.seq}", flush=True)
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
        description="Connect to the DCP2 server, subscribe to NOTIFY, and print incoming events."
    )
    parser.add_argument("host", help="device IP or hostname")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"DCP2 TCP port (default: {DEFAULT_PORT})")
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

    class_mask = parse_mask_arg(args.classes, CLASS_BITS)
    source_mask = parse_mask_arg(args.sources, SOURCE_BITS)
    bus_mask = parse_mask_arg(args.buses, BUS_BITS)

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
