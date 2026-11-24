#!/usr/bin/env python3
"""End-to-end tests of dcp2_client.py against the production C FS handler."""
import importlib.util
import io
import json
import os
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

    def connect(self):
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        self.client = dcp.FsClient(self.sock, 2, 0)

    def setUp(self):
        self.connect()

    def tearDown(self):
        self.sock.close()

    def assert_status(self, status, function, *args, **kwargs):
        with self.assertRaises(dcp.FsError) as raised:
            function(*args, **kwargs)
        self.assertEqual(raised.exception.status, status)

    def mutate(self, opcode, op_id, payload=b""):
        return self.client.request(opcode, struct.pack(">I", op_id) + payload)

    def begin(self, op_id, path, size, flags=0):
        reply = self.mutate(dcp.OP_FS_WRITE_BEGIN, op_id,
                            struct.pack(">QB", size, flags) + self.client._path(path))
        handle, = struct.unpack(">I", reply)
        self.assertNotEqual(handle, 0)
        return handle

    def test_mutations_survive_lost_replies(self):
        # Drop one complete, successful response to each mutation, after the C
        # server executed it. The real client must resend using the same ID.
        lost = set()

        class LossyClient(dcp.FsClient):
            def _read_frame(self, deadline):
                frame = super()._read_frame(deadline)
                if 0x06 <= frame.opcode <= 0x0B and frame.opcode not in lost and frame.body[:2] == bytes(2):
                    lost.add(frame.opcode)
                    raise socket.timeout("lost mutation reply")
                return frame

        client = LossyClient(self.sock, timeout=1, retries=1)
        folder = "flash:/повторы"
        source = self.root / "retry-source.bin"
        source.write_bytes(self.binary)
        client.mkdir(folder)
        self.assertEqual(client.upload(source, folder + "/file", block_size=3072), len(self.binary))
        client.rename(folder + "/file", folder + "/moved")
        self.assertEqual(b"".join(client.stream(folder + "/moved", dcp.FS_FILE)), self.binary)
        client.remove(folder + "/moved")
        client.remove(folder)
        self.assertEqual(lost, set(range(0x06, 0x0C)))

    def test_mutation_identity_is_per_connection_and_exact(self):
        path = self.client._path("flash:/replay")
        self.assert_status(1, self.mutate, dcp.OP_FS_MKDIR, 0, path)
        self.assertEqual(self.mutate(dcp.OP_FS_MKDIR, 41, path), b"")
        self.assertEqual(self.client.stat("flash:/replay")["type"], "directory")
        self.assertEqual(self.mutate(dcp.OP_FS_MKDIR, 41, path), b"")
        self.assert_status(1, self.mutate, dcp.OP_FS_MKDIR, 41, self.client._path("flash:/different"))
        self.assert_status(1, self.mutate, dcp.OP_FS_REMOVE, 41, path)
        self.assert_status(0x106, self.mutate, dcp.OP_FS_MKDIR, 42, path)
        # Error results are cached too, even if a local user changes the volume.
        (self.root / "flash/replay").rmdir()
        self.assert_status(0x106, self.mutate, dcp.OP_FS_MKDIR, 42, path)
        self.assert_status(6, self.mutate, dcp.OP_FS_MKDIR, 41, path)
        self.assertFalse((self.root / "flash/replay").exists())
        self.assertEqual(self.mutate(dcp.OP_FS_MKDIR, 0xffffffff, path), b"")
        self.assert_status(6, self.mutate, dcp.OP_FS_REMOVE, 1, path)
        self.sock.close()
        self.connect()
        self.assertEqual(self.mutate(dcp.OP_FS_REMOVE, 1, path), b"")

    def test_upload_offset_and_commit_visibility(self):
        path = "flash:/block-target"
        original = self.root / "flash/block-target"
        original.write_bytes(b"original")
        handle = self.begin(1, path, 6, 1)
        self.assertEqual(self.begin(1, path, 6, 1), handle)
        self.assert_status(1, self.begin, 1, path, 7, 1)
        self.assert_status(4, self.begin, 2, "flash:/other-upload", 0)
        block = struct.pack(">IQ", handle, 0) + b"abc"
        self.assertEqual(self.mutate(dcp.OP_FS_WRITE_BLOCK, 3, block), struct.pack(">Q", 3))
        self.assertEqual(self.mutate(dcp.OP_FS_WRITE_BLOCK, 3, block), struct.pack(">Q", 3))
        self.assert_status(1, self.mutate, dcp.OP_FS_WRITE_BLOCK, 3, block[:-1] + b"X")
        self.assert_status(6, self.mutate, dcp.OP_FS_WRITE_BLOCK, 4, block)
        self.assert_status(6, self.mutate, dcp.OP_FS_WRITE_BLOCK, 5, struct.pack(">IQ", handle, 4) + b"d")
        self.assertEqual(b"".join(self.client.stream(path, dcp.FS_FILE)), b"original")
        self.assertEqual(self.mutate(dcp.OP_FS_WRITE_BLOCK, 6, struct.pack(">IQ", handle, 3) + b"def"),
                         struct.pack(">Q", 6))
        final = struct.pack(">I", handle)
        self.assertEqual(self.mutate(dcp.OP_FS_WRITE_COMMIT, 7, final), b"")
        self.assertEqual(original.read_bytes(), b"abcdef")
        original.write_bytes(b"later edit")
        self.assertEqual(self.mutate(dcp.OP_FS_WRITE_COMMIT, 7, final), b"")
        self.assertEqual(original.read_bytes(), b"later edit")
        self.assert_status(0x102, self.mutate, dcp.OP_FS_WRITE_COMMIT, 8, final)

    def test_abort_and_disconnect_discard_staged_data(self):
        folder = self.root / "flash/cancel"
        folder.mkdir()
        target = folder / "existing"
        target.write_bytes(b"initial")
        path = "flash:/cancel/existing"
        before = set((self.root / "flash").rglob("*"))
        for first_id, disconnect in ((1, False), (4, True)):
            handle = self.begin(first_id, path, 100, 1)
            self.mutate(dcp.OP_FS_WRITE_BLOCK, first_id + 1,
                        struct.pack(">IQ", handle, 0) + b"some data")
            if disconnect:
                self.sock.close()
                self.connect()
                # A response on a new connection confirms teardown has run.
                self.assertEqual(self.client.stat(path)["size"], len(b"initial"))
            else:
                body = struct.pack(">I", handle)
                self.assertEqual(self.mutate(dcp.OP_FS_WRITE_ABORT, 3, body), b"")
                self.assertEqual(self.mutate(dcp.OP_FS_WRITE_ABORT, 3, body), b"")
            self.assertEqual(set((self.root / "flash").rglob("*")), before)
            self.assertEqual(target.read_bytes(), b"initial")

    def test_create_only_rechecks_destination_at_commit(self):
        path = "flash:/racing-writer"
        target = self.root / "flash/racing-writer"
        handle = self.begin(1, path, 0)
        self.assertFalse(target.exists())
        target.write_bytes(b"created after begin")
        final = struct.pack(">I", handle)
        self.assert_status(0x106, self.mutate, dcp.OP_FS_WRITE_COMMIT, 2, final)
        target.unlink()
        self.assert_status(0x106, self.mutate, dcp.OP_FS_WRITE_COMMIT, 2, final)
        self.assertFalse(target.exists())
        handle = self.begin(3, path, 0)
        self.assertEqual(self.mutate(dcp.OP_FS_WRITE_COMMIT, 4, struct.pack(">I", handle)), b"")
        self.assertEqual(target.read_bytes(), b"")

    def test_mutation_limits_and_paths(self):
        self.assertEqual(self.client.info()["capabilities"] & 0x3ff, 0x3ff)
        self.assertEqual(struct.unpack(">QHHI", self.client.request(dcp.OP_FS_WRITE_INFO)),
                         (0xffffffff, 3072, 1, 30000))
        self.assert_status(1, self.client.request, dcp.OP_FS_WRITE_INFO, b"x")
        self.assert_status(1, self.begin, 1, "flash:/bad-flag", 0, 2)
        self.assert_status(6, self.begin, 2, "flash:/too-large", 1 << 32)
        self.assert_status(1, self.mutate, dcp.OP_FS_REMOVE, 3, self.client._path("flash:/"))
        self.assert_status(1, self.mutate, dcp.OP_FS_MKDIR, 4, self.client._path("flash:/../x"))
        self.assert_status(1, self.mutate, dcp.OP_FS_RENAME, 5,
                           self.client._path("flash:/file.bin") + self.client._path("sd:/cross-volume"))
        self.assert_status(3, self.mutate, dcp.OP_FS_REMOVE, 6, self.client._path("flash:/config"))
        self.assert_status(0x101, self.begin, 7, "sd-pl:/unavailable", 0)
        handle = self.begin(8, "flash:/too-much-data", 4000)
        prefix = struct.pack(">IQ", handle, 0)
        self.assert_status(6, self.mutate, dcp.OP_FS_WRITE_BLOCK, 9, prefix + bytes(3073))
        self.assert_status(6, self.mutate, dcp.OP_FS_WRITE_COMMIT, 10, struct.pack(">I", handle))
        self.assertFalse((self.root / "flash/too-much-data").exists())

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
        self.connect()
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


    def test_monitor_cli_edits_files_and_directories(self):
        self.sock.close()
        directory = "flash:/cli-правки"
        remote = directory + "/data.bin"
        renamed = directory + "/new.bin"
        source = self.root / "cli-source.bin"
        base = [sys.executable, str(CLIENT), "127.0.0.1", "--port", str(self.port), "--fs-retries", "0"]

        def cli(*args, expected=0):
            result = subprocess.run(base + list(args), capture_output=True, timeout=10)
            self.assertEqual(result.returncode, expected, result.stderr.decode())
            return result

        limits = json.loads(cli("--fs-write-info").stdout)
        self.assertEqual((limits["max_file_size"], limits["max_block_size"]), (0xFFFFFFFF, 3072))
        cli("--fs-mkdir", directory)
        source.write_bytes(b"")
        cli("--fs-upload", str(source), remote)
        self.assertEqual(json.loads(cli("--fs-stat", remote).stdout)["size"], 0)

        source.write_bytes(self.binary)
        cli("--fs-upload", str(source), remote, expected=1)
        self.assertEqual(cli("--fs-read", remote).stdout, b"")
        cli("--fs-upload", str(source), remote, "--overwrite", "--fs-block-size", "1024")
        self.assertEqual(cli("--fs-read", remote).stdout, self.binary)
        cli("--fs-upload", str(self.root / "missing-source.bin"), remote, "--overwrite", expected=1)
        fifo = self.root / "cli-source.fifo"
        os.mkfifo(fifo)
        cli("--fs-upload", str(fifo), remote, "--overwrite", expected=1)
        cli("--fs-rename", remote, renamed)
        self.assertEqual([json.loads(line)["path"] for line in cli("--fs-list", directory).stdout.splitlines()],
                         ["new.bin"])
        cli("--fs-remove", directory, expected=1)
        self.assertEqual(cli("--fs-read", renamed).stdout, self.binary)
        cli("--fs-remove", renamed)
        cli("--fs-remove", directory)
        self.assertFalse((self.root / "flash/cli-правки").exists())

    def test_cli_reports_unknown_outcome_on_disconnect_after_commit(self):
        source = self.root / "cli-commit-source.bin"
        source.write_bytes(b"already committed")
        remote = "flash:/cli-commit.bin"
        # Relay the CLI to the real handler until the successful COMMIT reply,
        # then close its connection before the acknowledgement reaches it.
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(5)
            command = [sys.executable, str(CLIENT), "127.0.0.1", "--port", str(listener.getsockname()[1]),
                       "--timeout", "1", "--fs-upload", str(source), remote]
            with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
                downstream, _ = listener.accept()
                with downstream:
                    downstream.settimeout(5)
                    while True:
                        request = dcp.read_frame(downstream)
                        self.sock.sendall(dcp.build_frame(request.srv, request.op, request.seq, request.body))
                        response = dcp.read_frame(self.sock)
                        if request.opcode == dcp.OP_FS_WRITE_COMMIT:
                            self.assertEqual(dcp.parse_status_from_response(response), 0)
                            break
                        downstream.sendall(dcp.build_frame(response.srv, response.op, response.seq, response.body))
                _, error = process.communicate(timeout=5)
            self.assertEqual(process.returncode, 1)
            self.assertIn("outcome unknown", error.decode().lower())
            listener.setblocking(False)
            with self.assertRaises(BlockingIOError):
                listener.accept()
        self.assertEqual((self.root / "flash/cli-commit.bin").read_bytes(), source.read_bytes())


class RetryTests(unittest.TestCase):
    def test_readonly_v1_client_negotiation(self):
        class ReadOnlyClient(dcp.FsClient):
            def request(self, opcode, body=b"", **kwargs):
                if opcode != dcp.OP_FS_INFO or body:
                    raise AssertionError("unexpected request to the read-only device")
                return struct.pack(">HIHHHHIH", 1, 0x3F, 3072, 511, 16, 2, 30000, 0)

        with socket.socket() as sock:
            client = ReadOnlyClient(sock)
            for operation in (lambda: client.mkdir("flash:/dir"), client.write_info,
                              lambda: client.remove("flash:/file"), lambda: client.rename("sd:/a", "sd:/b"),
                              lambda: client.upload("source.bin", "sd:/file")):
                with self.assertRaises(dcp.FsError) as error:
                    operation()
                self.assertEqual(error.exception.status, 2)

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
