#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import os
import socket
import sys
import time
from dataclasses import dataclass
from http.client import HTTPConnection
from pathlib import Path
from typing import Iterable
from urllib.parse import quote


def _human_bytes(n: int) -> str:
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    v = float(n)
    for u in units:
        if v < 1024.0 or u == units[-1]:
            if u == "B":
                return f"{int(v)} {u}"
            return f"{v:.1f} {u}"
        v /= 1024.0
    return f"{v:.1f} TiB"


def _bar(done: int, total: int, width: int = 28) -> str:
    if total <= 0:
        return "[" + ("#" * width) + "]"
    frac = max(0.0, min(1.0, done / total))
    filled = int(round(frac * width))
    return "[" + ("#" * filled) + ("-" * (width - filled)) + "]"


@dataclass(frozen=True)
class LocalFile:
    abs_path: Path
    rel_posix: str
    size: int


def _iter_files(src_dir: Path, exclude_abs: set[Path]) -> list[LocalFile]:
    out: list[LocalFile] = []
    for root, _dirs, files in os.walk(src_dir):
        root_path = Path(root)
        for name in files:
            p = root_path / name
            try:
                rp = p.resolve()
            except FileNotFoundError:
                continue
            if rp in exclude_abs:
                continue
            if name in {".DS_Store", "Thumbs.db"}:
                continue
            st = p.stat()
            if not p.is_file():
                continue
            rel = p.relative_to(src_dir).as_posix()
            out.append(LocalFile(abs_path=p, rel_posix=rel, size=int(st.st_size)))
    out.sort(key=lambda f: f.rel_posix)
    return out


def _iter_parent_dirs(rel_posix: str) -> Iterable[str]:
    # "a/b/c.txt" -> "a", "a/b"
    parts = [p for p in rel_posix.split("/")[:-1] if p]
    cur: list[str] = []
    for part in parts:
        cur.append(part)
        yield "/".join(cur)


class _ConsoleClient:
    def __init__(self, host: str, port: int, timeout_s: float = 5.0) -> None:
        self._sock = socket.create_connection((host, port), timeout=timeout_s)
        self._sock.settimeout(timeout_s)
        self._buf = bytearray()

    def close(self) -> None:
        try:
            self._sock.close()
        except Exception:
            pass

    def _read_some(self) -> bytes:
        try:
            return self._sock.recv(4096)
        except socket.timeout:
            return b""

    def _consume_telnet_iac(self, data: bytes) -> bytes:
        # Drop IAC negotiation sequences so our line parsing is stable.
        out = bytearray()
        i = 0
        while i < len(data):
            b = data[i]
            if b != 0xFF:
                out.append(b)
                i += 1
                continue
            # IAC
            if i + 1 >= len(data):
                break
            cmd = data[i + 1]
            if cmd == 0xFA:  # SB ... IAC SE
                i += 2
                while i + 1 < len(data):
                    if data[i] == 0xFF and data[i + 1] == 0xF0:
                        i += 2
                        break
                    i += 1
                continue
            # Most other commands are 3 bytes: IAC <cmd> <opt>
            i += 3
        return bytes(out)

    def _read_lines_until_status(self, deadline: float) -> tuple[bool, str]:
        while time.time() < deadline:
            chunk = self._read_some()
            if chunk:
                self._buf.extend(self._consume_telnet_iac(chunk))
            while True:
                idx = self._buf.find(b"\n")
                if idx < 0:
                    break
                line = self._buf[: idx + 1]
                del self._buf[: idx + 1]
                s = line.decode("utf-8", errors="ignore").strip()
                if s == "OK":
                    return True, "OK"
                if s.startswith("ERR"):
                    return False, s
        return False, "timeout waiting for console reply"

    def cmd(self, command: str, timeout_s: float = 10.0) -> tuple[bool, str]:
        self._sock.sendall(command.encode("utf-8") + b"\r\n")
        return self._read_lines_until_status(time.time() + timeout_s)

    def drain(self, duration_s: float = 0.8) -> None:
        deadline = time.time() + duration_s
        while time.time() < deadline:
            chunk = self._read_some()
            if not chunk:
                break
            self._buf.extend(self._consume_telnet_iac(chunk))


class _Progress:
    def __init__(self) -> None:
        self._isatty = sys.stderr.isatty()
        self._initialized = False
        self._hide_cursor = "\x1b[?25l"
        self._show_cursor = "\x1b[?25h"

    def init(self) -> None:
        if not self._isatty or self._initialized:
            return
        sys.stderr.write(self._hide_cursor)
        sys.stderr.write("\n\n")  # reserve 2 lines
        sys.stderr.flush()
        self._initialized = True

    def close(self) -> None:
        if not self._isatty or not self._initialized:
            return
        sys.stderr.write(self._show_cursor)
        sys.stderr.flush()
        self._initialized = False

    def render(
        self,
        *,
        overall_done: int,
        overall_total: int,
        files_done: int,
        files_total: int,
        file_name: str,
        file_done: int,
        file_total: int,
    ) -> None:
        if not self._isatty:
            return
        self.init()

        overall_pct = 100.0 if overall_total <= 0 else (overall_done * 100.0 / overall_total)
        file_pct = 100.0 if file_total <= 0 else (file_done * 100.0 / file_total)

        overall_line = (
            f"Overall {_bar(overall_done, overall_total)} "
            f"{overall_pct:6.2f}%  "
            f"{_human_bytes(overall_done)}/{_human_bytes(overall_total)}  "
            f"files {files_done}/{files_total}"
        )
        file_line = (
            f"File   {_bar(file_done, file_total)} "
            f"{file_pct:6.2f}%  "
            f"{_human_bytes(file_done)}/{_human_bytes(file_total)}  "
            f"{file_name}"
        )

        # Move cursor up 2 lines, clear them, redraw.
        sys.stderr.write("\x1b[2A")
        sys.stderr.write("\r\x1b[2K" + overall_line[:200] + "\n")
        sys.stderr.write("\r\x1b[2K" + file_line[:200] + "\n")
        sys.stderr.flush()


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            b = f.read(128 * 1024)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def _http_get_small(*, host: str, port: int, url_path: str, max_bytes: int = 512 * 1024) -> tuple[int, bytes]:
    conn = HTTPConnection(host, port, timeout=10)
    try:
        conn.request("GET", url_path, headers={"Connection": "close"})
        resp = conn.getresponse()
        data = resp.read(max_bytes + 1)
        if len(data) > max_bytes:
            raise RuntimeError(f"GET {url_path}: response too large (> {max_bytes} bytes)")
        return resp.status, data
    finally:
        try:
            conn.close()
        except Exception:
            pass


def _http_put_file(
    *,
    host: str,
    port: int,
    url_path: str,
    display_name: str,
    local_path: Path,
    file_size: int,
    progress: _Progress,
    overall_before: int,
    overall_total: int,
    files_done: int,
    files_total: int,
) -> None:
    conn = HTTPConnection(host, port, timeout=15)
    try:
        conn.putrequest("PUT", url_path)
        conn.putheader("Content-Length", str(file_size))
        conn.putheader("Content-Type", "application/octet-stream")
        conn.endheaders()

        sent = 0
        with local_path.open("rb") as f:
            while True:
                chunk = f.read(64 * 1024)
                if not chunk:
                    break
                conn.send(chunk)
                sent += len(chunk)
                progress.render(
                    overall_done=overall_before + sent,
                    overall_total=overall_total,
                    files_done=files_done,
                    files_total=files_total,
                    file_name=display_name,
                    file_done=sent,
                    file_total=file_size,
                )

        resp = conn.getresponse()
        body = resp.read(256)  # keep it short
        if resp.status != 200:
            txt = body.decode("utf-8", errors="ignore").strip()
            raise RuntimeError(f"HTTP {resp.status} {resp.reason}: {txt or '<no body>'} ({url_path})")
    finally:
        try:
            conn.close()
        except Exception:
            pass


def _parse_manifest(data: bytes) -> dict:
    try:
        obj = json.loads(data.decode("utf-8"))
    except Exception as e:
        raise RuntimeError(f"Invalid manifest JSON: {e}") from e
    if not isinstance(obj, dict):
        raise RuntimeError("Invalid manifest JSON: expected object")
    return obj


def _build_manifest(files: list[LocalFile]) -> dict:
    m: dict[str, object] = {"version": 1, "generated_at_unix": int(time.time()), "files": {}}
    out_files: dict[str, object] = {}
    for f in files:
        out_files[f.rel_posix] = {"sha256": _sha256_file(f.abs_path), "size": f.size}
    m["files"] = out_files
    return m


def _manifest_bytes(manifest_obj: dict) -> bytes:
    return (json.dumps(manifest_obj, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Upload ~/Zynq/bvstk/web/assets/ contents to device flash:/www/ (mkdir via TCP console, PUT via HTTP)."
    )
    ap.add_argument("host", help="Device IP/host")
    ap.add_argument("--http-port", type=int, default=80, help="HTTP server port (default: 80)")
    ap.add_argument("--console-port", type=int, default=8888, help="TCP console port (default: 8888)")
    ap.add_argument(
        "--src",
        default=None,
        help="Source directory (default: ./assets next to this script)",
    )
    ap.add_argument(
        "--dst",
        default="flash:/www",
        help="Destination base path on device for mkdir (default: flash:/www)",
    )
    ap.add_argument(
        "--http-prefix",
        default="/flash/www",
        help="HTTP PUT prefix (default: /flash/www)",
    )
    ap.add_argument(
        "--manifest",
        default="manifest.json",
        help="Manifest file name stored under flash:/www (default: manifest.json)",
    )
    ap.add_argument(
        "--force",
        action="store_true",
        help="Upload all files (ignore remote manifest and overwrite)",
    )
    ap.add_argument("--no-mkdir", action="store_true", help="Do not create directories via TCP console")
    ap.add_argument("--dry-run", action="store_true", help="Print actions without uploading")
    args = ap.parse_args()

    script_abs = Path(__file__).resolve()
    src_dir = (
        Path(args.src).expanduser().resolve()
        if args.src
        else (script_abs.parent / "assets").resolve()
    )

    if not src_dir.exists() or not src_dir.is_dir():
        print(f"ERROR: source directory not found: {src_dir}", file=sys.stderr)
        print("Hint: create ~/Zynq/bvstk/web/assets/ and put web files there.", file=sys.stderr)
        return 2

    files = _iter_files(src_dir, exclude_abs={script_abs})
    if not files:
        print(f"No files found under {src_dir}", file=sys.stderr)
        return 1

    prefix = args.http_prefix.rstrip("/")
    manifest_name = args.manifest.strip().lstrip("/")
    if not manifest_name:
        print("ERROR: --manifest must not be empty", file=sys.stderr)
        return 2
    manifest_url_path = prefix + "/" + quote(manifest_name, safe="/")

    remote_manifest: dict | None = None
    remote_manifest_raw: bytes | None = None
    if not args.force and not args.dry_run:
        try:
            st, data = _http_get_small(host=args.host, port=args.http_port, url_path=manifest_url_path)
            if st == 200 and data:
                remote_manifest = _parse_manifest(data)
                remote_manifest_raw = data
            elif st == 404:
                remote_manifest = None
            else:
                # Non-404 errors should be visible early.
                if st != 200:
                    raise RuntimeError(f"GET {manifest_url_path}: HTTP {st}")
        except Exception as e:
            print(f"WARN: cannot load remote manifest ({manifest_url_path}): {e}", file=sys.stderr)
            remote_manifest = None

    remote_files: dict[str, dict] = {}
    if isinstance(remote_manifest, dict):
        rf = remote_manifest.get("files")
        if isinstance(rf, dict):
            for k, v in rf.items():
                if isinstance(k, str) and isinstance(v, dict):
                    remote_files[k] = v

    # Decide which files need upload (hash-based).
    to_upload: list[LocalFile] = []
    skipped = 0
    if args.force:
        to_upload = list(files)
    else:
        for f in files:
            r = remote_files.get(f.rel_posix)
            if not r:
                to_upload.append(f)
                continue
            rsha = r.get("sha256")
            rsz = r.get("size")
            if isinstance(rsha, str) and isinstance(rsz, int) and rsz == f.size:
                lsha = _sha256_file(f.abs_path)
                if lsha == rsha:
                    skipped += 1
                    continue
            to_upload.append(f)

    local_manifest = _build_manifest(files)
    local_manifest_bytes = _manifest_bytes(local_manifest)

    total_bytes = sum(f.size for f in to_upload)
    progress = _Progress()

    # Prepare directories (required for nested files: HTTP PUT does not mkdir parents).
    if not args.no_mkdir:
        dirs: set[str] = set()
        for f in to_upload:
            for d in _iter_parent_dirs(f.rel_posix):
                dirs.add(d)
        dir_list = sorted(dirs, key=lambda s: (s.count("/"), s))

        if args.dry_run:
            print(f"Would mkdir {args.dst}", file=sys.stderr)
            for d in dir_list:
                print(f"Would mkdir {args.dst}/{d}", file=sys.stderr)
        else:
            cc = _ConsoleClient(args.host, args.console_port, timeout_s=5.0)
            try:
                cc.drain()
                ok, msg = cc.cmd(f"mkdir {args.dst}")
                if not ok:
                    raise RuntimeError(f"mkdir {args.dst}: {msg}")
                for d in dir_list:
                    ok, msg = cc.cmd(f"mkdir {args.dst}/{d}")
                    if not ok:
                        raise RuntimeError(f"mkdir {args.dst}/{d}: {msg}")
            finally:
                cc.close()

    # Upload files.
    done_bytes = 0
    files_total = len(to_upload)
    try:
        for i, lf in enumerate(to_upload, start=1):
            rel = lf.rel_posix
            # Build /flash/www/<rel> with URL-encoding.
            url_path = prefix + "/" + quote(rel, safe="/")

            if args.dry_run:
                if args.force:
                    print(f"Would PUT {lf.abs_path} -> http://{args.host}:{args.http_port}{url_path}", file=sys.stderr)
                else:
                    print(f"Would PUT (changed) {lf.abs_path} -> http://{args.host}:{args.http_port}{url_path}", file=sys.stderr)
                done_bytes += lf.size
                continue

            progress.render(
                overall_done=done_bytes,
                overall_total=total_bytes,
                files_done=i - 1,
                files_total=files_total,
                file_name=rel,
                file_done=0,
                file_total=lf.size,
            )

            _http_put_file(
                host=args.host,
                port=args.http_port,
                url_path=url_path,
                display_name=rel,
                local_path=lf.abs_path,
                file_size=lf.size,
                progress=progress,
                overall_before=done_bytes,
                overall_total=total_bytes,
                files_done=i,
                files_total=files_total,
            )
            done_bytes += lf.size

        # Upload manifest if needed.
        if args.dry_run:
            print(f"Would PUT manifest -> http://{args.host}:{args.http_port}{manifest_url_path}", file=sys.stderr)
        else:
            should_put_manifest = True
            if remote_manifest_raw is not None and remote_manifest_raw == local_manifest_bytes:
                should_put_manifest = False
            if should_put_manifest:
                conn = HTTPConnection(args.host, args.http_port, timeout=10)
                try:
                    conn.putrequest("PUT", manifest_url_path)
                    conn.putheader("Content-Length", str(len(local_manifest_bytes)))
                    conn.putheader("Content-Type", "application/json; charset=utf-8")
                    conn.endheaders()
                    conn.send(local_manifest_bytes)
                    resp = conn.getresponse()
                    _ = resp.read(256)
                    if resp.status != 200:
                        raise RuntimeError(f"PUT manifest: HTTP {resp.status} {resp.reason}")
                finally:
                    try:
                        conn.close()
                    except Exception:
                        pass

        progress.render(
            overall_done=total_bytes,
            overall_total=total_bytes,
            files_done=files_total,
            files_total=files_total,
            file_name="done",
            file_done=0,
            file_total=0,
        )
        if progress._isatty:
            sys.stderr.write("\n")
            sys.stderr.flush()
    finally:
        progress.close()

    if args.force:
        print(f"OK: uploaded {files_total} files, {_human_bytes(total_bytes)} -> flash:/www", file=sys.stderr)
    else:
        print(
            f"OK: uploaded {files_total} changed files, skipped {skipped}, {_human_bytes(total_bytes)} -> flash:/www",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
