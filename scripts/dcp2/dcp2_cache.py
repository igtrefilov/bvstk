"""Bus-independent DCP2 CACHE v1 client (Python standard library only).

The caller owns a single socket through CacheClient. Resources are keyed by
profile ID; values are Python integers, including full-width uint64 values.
No operation in this module reads or writes the physical bus.
"""
from collections import deque
import json
import re
import socket
import struct
import sys
import time

from dcp2_client import (MAGIC, VERSION, DCP2_MAX_PAYLOAD, Frame, build_frame,
                         status_name, SRV_CACHE, OP_CACHE_GET,
                         OP_CACHE_SUBSCRIBE, OP_CACHE_UNSUBSCRIBE)

PAGE = struct.Struct(">BBHIIQQHHHHB")
EVENT = struct.Struct(">BBHIQQBBH")
ENTRY = struct.Struct(">BBBQQ")
SOURCES = {0: "unknown", 1: "settings", 2: "device"}
PRESENCE = {0: "unknown", 1: "ready", 2: "unavailable"}
REASONS = {1: "overflow", 2: "reconfigured", 3: "snapshot_expired"}


class CacheError(RuntimeError):
    def __init__(self, status):
        self.status = status
        super().__init__(status_name(status))


class CacheResyncRequired(RuntimeError):
    pass


def _name(value):
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,31}", value):
        raise ValueError("cache names must contain 1..31 ASCII letters, digits, _, - or .")
    encoded = value.encode("ascii")
    return bytes([len(encoded)]) + encoded


def _entries(body, offset, count):
    result = {}
    for _ in range(count):
        if offset >= len(body):
            raise ValueError("truncated cache resource ID")
        length = body[offset]
        offset += 1
        if not 1 <= length <= 31 or offset + length + ENTRY.size > len(body):
            raise ValueError("truncated cache resource")
        name = body[offset:offset + length].decode("ascii")
        _name(name)
        offset += length
        bits, valid, source, value, updated = ENTRY.unpack_from(body, offset)
        offset += ENTRY.size
        if (name in result or not 1 <= bits <= 64 or valid not in (0, 1)
                or source not in SOURCES or value >= 1 << bits
                or (not valid and (value or source or updated))):
            raise ValueError("invalid cache resource fields")
        result[name] = dict(id=name, width_bits=bits, valid=bool(valid),
                            source=SOURCES[source], value=value, updated_ms=updated)
    if offset != len(body):
        raise ValueError("unexpected bytes after cache resources")
    return result


def decode_page(body):
    if len(body) < PAGE.size:
        raise ValueError("short cache page")
    version, kind, sub, token, generation, revision, captured, total, first, count, next_, presence = PAGE.unpack_from(body)
    if (version != 1 or kind != 0 or not token or not generation or presence not in PRESENCE
            or first + count > total or (not count and first != total)
            or next_ != (0xFFFF if first + count == total else first + count)):
        raise ValueError("invalid cache page header")
    return dict(subscription_id=sub, snapshot_id=token, generation=generation,
                revision=revision, captured_ms=captured, total=total, first=first,
                next=next_, presence=PRESENCE[presence],
                resources=_entries(body, PAGE.size, count))


def decode_event(body):
    if len(body) < EVENT.size:
        raise ValueError("short cache event")
    version, kind, sub, generation, revision, time_ms, presence, reason, count = EVENT.unpack_from(body)
    if (version != 1 or kind not in (1, 2, 3) or not sub or not generation
            or presence not in PRESENCE or count != (1 if kind == 1 else 0)
            or (kind == 3 and reason not in REASONS) or (kind != 3 and reason)):
        raise ValueError("invalid cache event header")
    return dict(kind={1: "update", 2: "presence", 3: "resync_required"}[kind],
                subscription_id=sub, generation=generation, revision=revision,
                time_ms=time_ms, presence=PRESENCE[presence],
                reason=REASONS.get(reason), resources=_entries(body, EVENT.size, count))


class CacheClient:
    """Multiple subscriptions may share this client. RPCs retain interleaved events.

    Response timeouts are fatal to an RPC: reconnect and take a fresh snapshot.
    Idle event timeouts retain partial frames and are safe to retry.
    """
    def __init__(self, sock, timeout=5.0):
        self.sock, self.timeout = sock, timeout
        self.sequence = 0
        self.received = bytearray()
        self.events = deque()

    def _frame(self, deadline):
        while True:
            if len(self.received) >= 8:
                magic, version, length = struct.unpack_from(">4sHH", self.received)
                if magic != MAGIC or version != VERSION or not 4 <= length <= DCP2_MAX_PAYLOAD:
                    raise ValueError("invalid DCP2 header")
                if len(self.received) >= 8 + length:
                    payload = bytes(self.received[8:8 + length])
                    del self.received[:8 + length]
                    return Frame(payload[0], payload[1], struct.unpack_from(">H", payload, 2)[0], payload[4:])
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise socket.timeout("CACHE receive deadline expired")
            self.sock.settimeout(remaining)
            chunk = self.sock.recv(8192)
            if not chunk:
                raise ConnectionError("DCP2 connection closed; obtain a new cache snapshot after reconnecting")
            self.received.extend(chunk)

    @staticmethod
    def _event(frame):
        if frame.srv != SRV_CACHE or frame.op != (OP_CACHE_SUBSCRIBE | 0x40) or frame.seq:
            raise ValueError("unexpected frame in CACHE stream")
        return decode_event(frame.body)

    def request(self, opcode, body, service=SRV_CACHE):
        self.sequence = self.sequence % 65535 + 1
        self.sock.settimeout(self.timeout)
        self.sock.sendall(build_frame(service, opcode, self.sequence, body))
        deadline = time.monotonic() + self.timeout
        while True:
            frame = self._frame(deadline)
            if frame.is_event:
                if len(self.events) >= 2048:
                    raise CacheResyncRequired("client event queue full; reconnect and obtain new snapshots")
                self.events.append(self._event(frame))
                continue
            if frame.srv != service or frame.op != (opcode | 0x80) or frame.seq != self.sequence or len(frame.body) < 2:
                raise ValueError("unexpected CACHE response")
            status = struct.unpack_from(">H", frame.body)[0]
            if status:
                raise CacheError(status)
            return frame.body[2:]

    def snapshot(self, subsystem, device, subscribe=False):
        target = b"\x01" + _name(subsystem) + _name(device)
        opcode = OP_CACHE_SUBSCRIBE if subscribe else OP_CACHE_GET
        body = target if subscribe else target + struct.pack(">IH", 0, 0)
        result = decode_page(self.request(opcode, body))
        if result["first"] or bool(result["subscription_id"]) != subscribe:
            raise ValueError("invalid initial CACHE page")
        resources = dict(result["resources"])
        next_ = result["next"]
        while next_ != 0xFFFF:
            page = decode_page(self.request(OP_CACHE_GET, target + struct.pack(">IH", result["snapshot_id"], next_)))
            if (page["first"] != next_ or any(page[key] != result[key] for key in
                    ("subscription_id", "snapshot_id", "generation", "revision", "captured_ms", "total", "presence"))
                    or resources.keys() & page["resources"].keys()):
                raise ValueError("inconsistent CACHE snapshot pages")
            resources.update(page["resources"])
            next_ = page["next"]
        if len(resources) != result["total"]:
            raise ValueError("incomplete CACHE snapshot")
        if subscribe:
            # Events queued before this subscription response describe its old
            # incarnation. New events cannot precede its final snapshot page.
            self.events = deque(e for e in self.events if e["subscription_id"] != result["subscription_id"])
        return dict(kind="snapshot", subsystem=subsystem, device=device,
                    subscription_id=result["subscription_id"], generation=result["generation"],
                    revision=result["revision"], captured_ms=result["captured_ms"],
                    presence=result["presence"], synchronized=True, resources=resources)

    def event(self, timeout=None):
        if self.events:
            return self.events.popleft()
        return self._event(self._frame(time.monotonic() + (self.timeout if timeout is None else timeout)))

    def unsubscribe(self, subscription_id):
        if self.request(OP_CACHE_UNSUBSCRIBE, struct.pack(">BH", 1, subscription_id)):
            raise ValueError("unexpected UNSUBSCRIBE response body")
        self.events = deque(e for e in self.events if e["subscription_id"] != subscription_id)


def apply_event(state, event):
    """Update a complete local snapshot, or mark it unusable until resubscribed."""
    if event["subscription_id"] != state["subscription_id"]:
        raise ValueError("cache event belongs to another subscription")
    if (not state["synchronized"] or event["kind"] == "resync_required"
            or event["generation"] != state["generation"] or event["revision"] != state["revision"] + 1):
        state["synchronized"] = False
        raise CacheResyncRequired(event.get("reason") or "cache revision gap")
    for key, entry in event["resources"].items():
        if key not in state["resources"] or entry["width_bits"] != state["resources"][key]["width_bits"]:
            state["synchronized"] = False
            raise CacheResyncRequired("cache resource catalog changed")
    state["resources"].update(event["resources"])
    state.update(revision=event["revision"], presence=event["presence"], captured_ms=event["time_ms"])


def json_state(state):
    """Lossless JSON: register values use hexadecimal strings, including uint64."""
    return {**state, "resources": [{**e, "value": f"0x{e['value']:0{(e['width_bits'] + 3) // 4}X}"}
                                   for e in state["resources"].values()]}


def run_cache_operation(args):
    subsystem, device = args.cache_watch or args.cache_get
    try:
        with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
            client = CacheClient(sock, args.timeout)
            state = client.snapshot(subsystem, device, subscribe=bool(args.cache_watch))
            print(json.dumps(json_state(state), ensure_ascii=False), flush=True)
            if not args.cache_watch:
                return 0
            until = time.monotonic() + args.cache_duration if args.cache_duration else float("inf")
            while time.monotonic() < until:
                try:
                    event = client.event(min(1.0, until - time.monotonic()))
                except socket.timeout:
                    continue
                try:
                    apply_event(state, event)
                except CacheResyncRequired as exc:
                    print(json.dumps(dict(kind="resync_required", subsystem=subsystem, device=device,
                                          synchronized=False, reason=str(exc))), flush=True)
                    state = client.snapshot(subsystem, device, subscribe=True)
                else:
                    state["kind"] = event["kind"]
                print(json.dumps(json_state(state), ensure_ascii=False), flush=True)
            client.unsubscribe(state["subscription_id"])
        return 0
    except KeyboardInterrupt:
        return 0  # Closing the connection releases every subscription.
    except (ConnectionError, OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
