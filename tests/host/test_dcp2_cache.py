#!/usr/bin/env python3
"""Production C CACHE handler and Python client over TCP; no board required."""
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import unittest

SERVER = sys.argv.pop(1)
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/dcp2"))
import dcp2_client as dcp
import dcp2_cache as cache


class CacheTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = subprocess.Popen([SERVER], stdout=subprocess.PIPE, text=True)
        line = cls.server.stdout.readline()
        if not line:
            raise RuntimeError("C CACHE self-tests failed")
        cls.port = int(line)

    @classmethod
    def tearDownClass(cls):
        cls.server.terminate()
        cls.server.wait(timeout=5)
        cls.server.stdout.close()

    def setUp(self):
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=2)
        self.client = cache.CacheClient(self.sock, 2)

    def tearDown(self):
        self.sock.close()

    def test_frozen_pagination_and_uint64(self):
        state = self.client.snapshot("future_bus", "sensor")
        self.assertEqual(len(state["resources"]), 256)
        # The fixture modifies resource_200 between pages. GET retains its old value.
        self.assertEqual(state["resources"]["resource_200"]["value"], 0xFEDCBA98765432C8)
        self.assertFalse(state["resources"]["resource_001"]["valid"])
        self.assertEqual(state["resources"]["resource_002"]["source"], "settings")
        self.assertEqual(cache.json_state(state)["resources"][0]["value"], "0xFEDCBA9876543200")

    def test_snapshot_to_ordered_updates(self):
        state = self.client.snapshot("future_bus", "sensor", subscribe=True)
        self.assertEqual(state["revision"], 0)
        for revision in range(1, 4):
            event = self.client.event()
            cache.apply_event(state, event)
            self.assertEqual(state["revision"], revision)
            self.assertEqual(state["resources"]["resource_200"]["value"], 1000 + revision)
        # A regular GET while subscribed keeps interleaved events for the caller.
        self.client.snapshot("future_bus", "sensor")
        cache.apply_event(state, self.client.event())
        self.assertEqual(state["revision"], 4)
        self.client.unsubscribe(state["subscription_id"])
        with self.assertRaises(cache.CacheError) as raised:
            self.client.unsubscribe(state["subscription_id"])
        self.assertEqual(raised.exception.status, 0x200)

    def test_resubscription_replaces_old_queued_events(self):
        previous = self.client.snapshot("future_bus", "sensor", subscribe=True)
        current = self.client.snapshot("future_bus", "sensor", subscribe=True)
        self.assertEqual(previous["subscription_id"], current["subscription_id"])
        event = self.client.event()
        self.assertEqual(event["revision"], current["revision"] + 1)
        cache.apply_event(current, event)

    def test_missing_target_and_bad_names(self):
        with self.assertRaises(cache.CacheError) as raised:
            self.client.snapshot("spi", "not_registered")
        self.assertEqual(raised.exception.status, 0x200)
        with self.assertRaises(ValueError):
            self.client.snapshot("bad/name", "sensor")

    def test_gap_invalidates_projection(self):
        state = self.client.snapshot("future_bus", "sensor", subscribe=True)
        event = self.client.event()
        event["revision"] += 1
        with self.assertRaises(cache.CacheResyncRequired):
            cache.apply_event(state, event)
        self.assertFalse(state["synchronized"])

    def test_fragment_retained_across_idle_timeout(self):
        reader, writer = socket.socketpair()
        try:
            client = cache.CacheClient(reader, 0.01)
            body = cache.EVENT.pack(1, 2, 1, 1, 1, 123, 1, 0, 0)
            packet = dcp.build_frame(8, 0x61, 0, body)
            writer.sendall(packet[:5])
            with self.assertRaises(socket.timeout):
                client.event()
            writer.sendall(packet[5:])
            self.assertEqual(client.event()["presence"], "ready")
        finally:
            reader.close()
            writer.close()

    def test_malformed_entries(self):
        header = cache.EVENT.pack(1, 1, 1, 1, 1, 1, 1, 0, 1)
        with self.assertRaises(ValueError):
            cache.decode_event(header + b"\x01x" + cache.ENTRY.pack(8, 1, 2, 256, 0))
        with self.assertRaises(ValueError):
            cache.decode_event(header + b"\x01x" + cache.ENTRY.pack(8, 0, 2, 1, 0))
        with self.assertRaises(ValueError):
            cache.decode_event(header[:-1])


    def test_cli_get(self):
        self.sock.close()
        result = subprocess.run([sys.executable, str(ROOT / "scripts/dcp2/dcp2_client.py"),
            "127.0.0.1", "--port", str(self.port), "--cache-get", "future_bus", "sensor"],
            capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        state = json.loads(result.stdout)
        self.assertEqual(len(state["resources"]), 256)
        self.assertEqual(state["resources"][0]["value"], "0xFEDCBA9876543200")


if __name__ == "__main__":
    unittest.main()
