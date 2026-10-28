#!/usr/bin/env python3
"""Manual filesystem acceptance for a running BVSTK HTTP server.

Writes only inside a newly-created private directory; reads config for a
before/after hash comparison. Does not format, reboot, or change profiles.
"""
import argparse
import hashlib
import json
from pathlib import Path
import secrets
import socket
import urllib.error
import urllib.parse
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    client = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    base = "http://" + args.host
    report = {"checks": [], "config_before": {}, "config_after": {}, "private_roots": []}

    def remember(name, **details):
        report["checks"].append({"name": name, "result": "passed", **details})
        (args.output / "http-result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        print("PASS:", name, flush=True)

    def url(endpoint, **params):
        return endpoint + "?" + urllib.parse.urlencode(params)

    def request(endpoint, method="GET", data=None, status=200, **params):
        expected = status if isinstance(status, tuple) else (status,)
        req = urllib.request.Request(base + url(endpoint, **params), data=data, method=method)
        try:
            response = client.open(req, timeout=60)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            try:
                body = response.read()
            except ConnectionResetError:
                # The existing FreeRTOS HTTP transport can close with unread
                # upload bytes after an early error. Require the received error
                # status; never accept a reset as a successful data transfer.
                if response.status not in expected or response.status < 400:
                    raise
                report.setdefault("reset_after_error_status", []).append(
                    {"status": response.status, "path": params.get("path")})
                body = b""
            assert response.status in expected, (method, endpoint, params, response.status, body)
            return body

    def action(volume, path, operation, **extra):
        return request("/api/ui/files", "PUT", b"", volume=volume, path=path,
                       action=operation, confirm="true", **extra)

    def snapshot(destination):
        hashes = {}
        pending = ["/config"]
        while pending:
            folder = pending.pop()
            offset = 0
            while True:
                data = json.loads(request("/api/ui/files", volume="flash", path=folder, offset=offset))
                assert data["ok"]
                for entry in data["entries"]:
                    path = folder.rstrip("/") + "/" + entry["name"]
                    if entry["directory"]:
                        pending.append(path)
                    else:
                        content = request("/api/ui/file", volume="flash", path=path)
                        file = args.output / destination / path.lstrip("/")
                        file.parent.mkdir(parents=True, exist_ok=True)
                        file.write_bytes(content)
                        key = path.lower()
                        assert key not in hashes
                        hashes[key] = hashlib.sha256(content).hexdigest()
                if data["next"] is None:
                    break
                offset = data["next"]
        return hashes

    volumes = json.loads(request("/api/ui/volumes"))["volumes"]
    ready = [v["name"] for v in volumes if v["ready"]]
    assert "flash" in ready, volumes
    remember("volume readiness", volumes=volumes)
    report["config_before"] = snapshot("config-before")
    remember("configuration baseline", files=len(report["config_before"]))
    private = "/.bvstk-stage5-" + secrets.token_hex(4)
    data = bytes((i * 17) & 255 for i in range(4097))
    try:
        for volume in ready:
            action(volume, private, "mkdir")
            report["private_roots"].append({"volume": volume, "path": private})
            (args.output / "http-result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
            file = private + "/данные.bin"
            body = json.loads(request("/api/ui/file", "PUT", data, volume=volume, path=file, confirm="true"))
            assert body["ok"] and body["bytes"] == len(data)
            assert request("/api/ui/file", volume=volume, path=file) == data
            request("/api/ui/file", "PUT", b"collision", status=409, volume=volume, path=file, confirm="true")
            assert request("/api/ui/file", volume=volume, path=file) == data
            request("/api/ui/file", status=413, volume=volume, path=file, max_bytes=len(data) - 1)
            empty = private + "/empty"
            assert json.loads(request("/api/ui/file", "PUT", b"", volume=volume, path=empty, confirm="true"))["bytes"] == 0
            assert request("/api/ui/file", volume=volume, path=empty) == b""
            listing = json.loads(request("/api/ui/files", volume=volume, path=private))
            assert any(e["name"] == "данные.bin" for e in listing["entries"]), listing
            request("/api/ui/file", "PUT", b"short", volume=volume, path=file, confirm="true", overwrite="true")
            assert request("/api/ui/file", volume=volume, path=file) == b"short"
            renamed = private + "/renamed.bin"
            action(volume, file, "rename", dst_volume=volume, dst=renamed)
            assert request("/api/ui/file", volume=volume, path=renamed) == b"short"
            request("/api/ui/files", "PUT", b"", status=409, volume=volume, path=renamed,
                    action="rename", dst_volume=volume, dst=empty, confirm="true")
            request("/api/ui/files", status=400, volume=volume, path="/../escape")
            request("/api/ui/files", status=400, volume=volume, path=private + "/alias.")
            request("/api/ui/files", "PUT", b"", status=400, volume=volume, path="/",
                    action="remove", confirm="true")
            partial = private + "/partial"
            endpoint = url("/api/ui/file", volume=volume, path=partial, confirm="true")
            with socket.create_connection((args.host, 80), timeout=10) as sock:
                sock.settimeout(60)
                sock.sendall(("PUT " + endpoint + " HTTP/1.0\r\nHost: " + args.host +
                              "\r\nContent-Length: 9\r\n\r\nabc").encode("ascii"))
                sock.shutdown(socket.SHUT_WR)
                chunks = []
                while True:
                    block = sock.recv(4096)
                    if not block:
                        break
                    chunks.append(block)
                status_line = b"".join(chunks).split(b"\r\n", 1)[0]
                # A truncated body can terminate the connection before an error
                # response is sent. It must never confirm a successful upload.
                assert not status_line or b" 500 " in status_line, status_line
            assert request("/api/ui/file", volume=volume, path=partial) == b"abc"
            remember("file lifecycle " + volume, sha256=hashlib.sha256(data).hexdigest(),
                     unicode=True, partial_bytes=3)

        if len(ready) > 1:
            src, dst = ready[:2]
            action(src, private + "/renamed.bin", "copy", dst_volume=dst, dst=private + "/copied")
            assert request("/api/ui/file", volume=dst, path=private + "/copied") == b"short"
            action(dst, private + "/copied", "remove")
            remember("cross-volume copy", source=src, destination=dst)
    finally:
        for item in report["private_roots"]:
            for name in ("данные.bin", "empty", "renamed.bin", "partial", "copied"):
                request("/api/ui/files", "PUT", b"", status=(200, 404),
                        volume=item["volume"], path=item["path"] + "/" + name,
                        action="remove", confirm="true")
            action(item["volume"], item["path"], "remove")
        report["cleanup"] = "complete"
        remember("private files removed")
    report["config_after"] = snapshot("config-after")
    assert report["config_after"] == report["config_before"]
    remember("configuration preserved", files=len(report["config_after"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
