#!/usr/bin/env python3
"""End-to-end tests of dcp2_client.py against the production C FS handler."""
import importlib.util
import io
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import unittest

SERVER = sys.argv.pop(1)
REPO = Path(__file__).resolve().parents[2]
CLIENT = REPO / "scripts/dcp2/dcp2_client.py"
SPEC = importlib.util.spec_from_file_location("dcp2_client", CLIENT)
dcp = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = dcp
SPEC.loader.exec_module(dcp)


class FilesystemTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="bvstk-fs-test-")
        cls.root = Path(cls.temporary.name)
        (cls.root / "flash/config/nested/empty").mkdir(parents=True)
        (cls.root / "sd").mkdir()
        cls.binary = bytes(range(256)) * 149 + b"\x00\xffend"
        (cls.root / "flash/file.bin").write_bytes(cls.binary)
        (cls.root / "flash/config/network.json").write_bytes(b'{"ip":"192.168.0.10"}\n')
        (cls.root / "flash/config/nested/zero").touch()
        (cls.root / "flash/config/nested/data.bin").write_bytes(cls.binary)
        (cls.root / "flash/config/данные.json").write_text('{"name":"тест"}\n', encoding="utf-8")
        cls.long_name = "nested/" + "long" * 45 + ".json"
        (cls.root / "flash/config" / cls.long_name).write_bytes(b"long path contents")
        (cls.root / "sd/readme.txt").write_bytes(b"second volume\n")
        deep = cls.root / "flash/deep"
        for _ in range(17):
            deep /= "d"
        deep.mkdir(parents=True)
        cls.server = subprocess.Popen([SERVER, str(cls.root)], stdout=subprocess.PIPE, text=True)
        line = cls.server.stdout.readline()
        if not line:
            raise RuntimeError(f"C self-tests/server failed: exit={cls.server.poll()}")
        cls.port = int(line)

    @classmethod
    def tearDownClass(cls):
        cls.server.terminate()
        cls.server.wait(timeout=5)
        cls.server.stdout.close()
        cls.temporary.cleanup()

    def setUp(self):
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        self.client = dcp.FsClient(self.sock, 2, 0)

    def tearDown(self):
        self.sock.close()

    def assert_status(self, status, function, *args, **kwargs):
        with self.assertRaises(dcp.FsError) as raised:
            function(*args, **kwargs)
        self.assertEqual(raised.exception.status, status)

    def test_info_and_stat(self):
        info = self.client.info()
        self.assertEqual((info["fs_version"], info["max_block_size"], info["max_handles"]), (1, 3072, 2))
        self.assertEqual([v["root"] for v in info["volumes"]], ["flash:/", "sd:/", "sd-pl:/"])
        self.assertEqual(self.client.stat("flash:/file.bin")["size"], len(self.binary))
        self.assertEqual(self.client.stat("flash:/")["type"], "directory")

    def test_multiple_volumes(self):
        self.assertEqual(b"".join(self.client.stream("sd:/readme.txt", dcp.FS_FILE)), b"second volume\n")
        self.assert_status(0x0101, self.client.stat, "sd-pl:/")
        self.assert_status(0x0100, self.client.stat, "unknown:/")

    def test_nonrecursive_tree(self):
        entries = list(self.client.list_entries("flash:/config/"))
        self.assertEqual({e["path"] for e in entries}, {"network.json", "nested", "данные.json"})

    def test_recursive_tree_crosses_blocks(self):
        entries = list(self.client.list_entries("flash:/config", True, 7, True))
        by_path = {e["path"]: e for e in entries}
        self.assertEqual(len(entries), 7)
        self.assertEqual(by_path["nested/empty"]["type"], "directory")
        self.assertEqual(by_path["nested/zero"]["size"], 0)
        self.assertEqual(by_path["nested/data.bin"]["size"], len(self.binary))
        self.assertIn(self.long_name, by_path)
        order = [e["path"] for e in entries]
        self.assertLess(order.index("nested"), order.index("nested/empty"))

    def test_file_and_exact_block_boundary(self):
        for block in (17, 3072):
            self.assertEqual(b"".join(self.client.stream("flash:/file.bin", dcp.FS_FILE,
                             block_size=block, verify_repeats=True)), self.binary)
        self.assertEqual(b"".join(self.client.stream("sd:/readme.txt", dcp.FS_FILE,
                         block_size=14, verify_repeats=True)), b"second volume\n")

    def test_empty_file_and_directory(self):
        self.assertEqual(list(self.client.stream("flash:/config/nested/zero", dcp.FS_FILE,
                         block_size=1, verify_repeats=True)), [b""])
        self.assertEqual(list(self.client.list_entries("flash:/config/nested/empty", True)), [])

    def test_tar_contents_long_names_unicode_empty_directories(self):
        data = b"".join(self.client.stream("flash:/config/", dcp.FS_TAR, block_size=513, verify_repeats=True))
        self.assertEqual(len(data) % 512, 0)
        self.assertEqual(data[-1024:], bytes(1024))
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            members = {item.name: item for item in archive.getmembers()}
            self.assertEqual(len(members), 7)
            self.assertTrue(members["nested/empty"].isdir())
            for item in archive.getmembers():
                if item.isfile():
                    self.assertEqual(archive.extractfile(item).read(), (self.root / "flash/config" / item.name).read_bytes())

    def test_empty_tar_and_volume_root(self):
        data = b"".join(self.client.stream("flash:/config/nested/empty", dcp.FS_TAR, block_size=512))
        self.assertEqual(data, bytes(1024))
        data = b"".join(self.client.stream("sd:/", dcp.FS_TAR))
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            self.assertEqual(archive.getnames(), ["readme.txt"])

    def test_open_retry_and_handle_limits(self):
        first = self.client.open("flash:/file.bin", dcp.FS_FILE, open_id=50)
        self.assertEqual(self.client.open("flash:/file.bin", dcp.FS_FILE, open_id=50), first)
        self.assert_status(1, self.client.open, "sd:/readme.txt", dcp.FS_FILE, open_id=50)
        second = self.client.open("flash:/config", dcp.FS_TREE, open_id=51)
        self.assert_status(4, self.client.open, "flash:/file.bin", dcp.FS_FILE, open_id=52)
        self.client.close(first[0])
        self.client.close(first[0])
        self.client.close(second[0])
        self.assert_status(0x0102, self.client.read_block, first[0], 0, first[1])

    def test_order_and_eof_retry(self):
        handle, block, _ = self.client.open("sd:/readme.txt", dcp.FS_FILE)
        self.assert_status(6, self.client.read_block, handle, 1, block)
        first = self.client.read_block(handle, 0, block)
        self.assertTrue(first[1])
        self.assertEqual(self.client.read_block(handle, 0, block), first)
        self.assert_status(6, self.client.read_block, handle, 1, block)
        self.client.close(handle)

    def test_paths_types_limits_and_reserved_fields(self):
        for path in ("flash:/../file.bin", "flash:/a//b", "flash:/a/./b", "flash:/a\\b", "/flash/file.bin"):
            self.assert_status(1, self.client.stat, path)
        self.assert_status(1, self.client.request, dcp.OP_FS_STAT, b"\x00\x02\xc0\xaf")
        self.assert_status(1, self.client.request, dcp.OP_FS_INFO, b"unexpected")
        self.assert_status(1, self.client.request, dcp.OP_FS_CLOSE, b"\0")
        self.assert_status(6, self.client.stat, "flash:/" + "x" * 512)
        self.assert_status(0x0100, self.client.stat, "flash:/missing")
        self.assert_status(0x0103, self.client.open, "flash:/file.bin", dcp.FS_TREE)
        self.assert_status(0x0103, self.client.open, "flash:/config", dcp.FS_FILE)
        self.assert_status(6, self.client.open, "flash:/file.bin", dcp.FS_FILE, block_size=3073)
        self.assert_status(1, self.client.open, "flash:/file.bin", dcp.FS_FILE, recursive=True)
        self.assert_status(2, self.client.open, "flash:/config", 4)
        self.assert_status(2, self.client.request, 63)

    def test_depth_limit_does_not_silently_truncate(self):
        self.assert_status(6, list, self.client.list_entries("flash:/deep", True))

    def test_modified_file_is_terminal_and_cache_is_stable(self):
        target = self.root / "flash/changed.bin"
        target.write_bytes(self.binary)
        handle, block, _ = self.client.open("flash:/changed.bin", dcp.FS_FILE, block_size=32)
        first = self.client.read_block(handle, 0, block)
        target.write_bytes(b"short")
        self.assertEqual(self.client.read_block(handle, 0, block), first)
        for index in range(1, 2000):
            try:
                self.client.read_block(handle, index, block)
            except dcp.FsError as error:
                self.assertEqual(error.status, 0x0105)
                self.assert_status(0x0105, self.client.read_block, handle, index, block)
                break
        else:
            self.fail("changed file was reported as a successful stream")
        self.client.close(handle)

    def test_disconnect_releases_file_and_directory_handles(self):
        self.client.open("flash:/file.bin", dcp.FS_FILE)
        handle, block, _ = self.client.open("flash:/config", dcp.FS_TREE, True, 5)
        self.client.read_block(handle, 0, block)
        self.sock.close()
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        self.client = dcp.FsClient(self.sock, 2, 0)
        self.assertEqual(b"".join(self.client.stream("sd:/readme.txt", dcp.FS_FILE)), b"second volume\n")

    def test_monitor_cli_info_list_read_tar_and_failed_output(self):
        self.sock.close()  # The firmware and the fixture serve one connection at a time.
        base = [sys.executable, str(CLIENT), "127.0.0.1", "--port", str(self.port), "--fs-retries", "0"]
        output = self.root / "download.bin"
        result = subprocess.run(base + ["--fs-info"], capture_output=True, check=True)
        self.assertEqual(json.loads(result.stdout)["fs_version"], 1)
        result = subprocess.run(base + ["--fs-list", "flash:/config/", "--recursive", "--fs-block-size", "11"],
                                capture_output=True, check=True)
        self.assertEqual(len([json.loads(line) for line in result.stdout.splitlines()]), 7)
        subprocess.run(base + ["--fs-read", "flash:/file.bin", "--output", str(output), "--fs-verify-repeats"],
                       capture_output=True, check=True)
        self.assertEqual(output.read_bytes(), self.binary)
        result = subprocess.run(base + ["--fs-read", "flash:/missing", "--output", str(output)], capture_output=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(output.read_bytes(), self.binary)
        self.assertEqual(list(self.root.glob("*.part")), [])
        result = subprocess.run(base + ["--fs-tar", "sd:/"], capture_output=True, check=True)
        with tarfile.open(fileobj=io.BytesIO(result.stdout)) as archive:
            self.assertEqual(archive.extractfile("readme.txt").read(), b"second volume\n")


class RetryTests(unittest.TestCase):
    def test_partial_late_response_is_preserved_across_retry(self):
        client_sock, server_sock = socket.socketpair()
        errors = []

        def server():
            try:
                first = dcp.read_frame(server_sock)
                frame = dcp.build_frame(dcp.SRV_FS, first.opcode | 0x80, first.seq,
                                        b"\0\0" + struct.pack(">BBQ", 1, 0, 123))
                server_sock.sendall(frame[:11])  # Split inside the DCP header/payload boundary.
                second = dcp.read_frame(server_sock)
                if first.seq == second.seq or first.body != second.body:
                    raise AssertionError("retry identity is wrong")
                server_sock.sendall(frame[11:])
                server_sock.sendall(dcp.build_frame(dcp.SRV_NOTIFY, 0x50, 0, b"event"))
                server_sock.sendall(dcp.build_frame(dcp.SRV_FS, second.opcode | 0x80, second.seq,
                                                     b"\0\0" + struct.pack(">BBQ", 1, 0, 123)))
            except Exception as error:
                errors.append(error)
            finally:
                server_sock.close()

        worker = threading.Thread(target=server, daemon=True)
        worker.start()
        try:
            self.assertEqual(dcp.FsClient(client_sock, timeout=0.1, retries=1).stat("flash:/test")["size"], 123)
        finally:
            client_sock.close()
            worker.join(timeout=2)
        self.assertFalse(worker.is_alive())
        self.assertEqual(errors, [])

    def test_truncated_stream_does_not_succeed(self):
        client_sock, server_sock = socket.socketpair()
        server_sock.close()
        try:
            with self.assertRaises((ConnectionError, OSError)):
                dcp.FsClient(client_sock, timeout=0.1, retries=0).info()
        finally:
            client_sock.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
