#!/usr/bin/env python3
"""Wire compatibility, bounds, error handling and no write retries."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

spec = importlib.util.spec_from_file_location(
    "dcp2_client", Path(__file__).resolve().parents[2] / "scripts/dcp2/dcp2_client.py")
dcp = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = dcp
spec.loader.exec_module(dcp)

class Socket:
    def __init__(self, response=b""):
        self.response = response
        self.sent = []
    def sendall(self, data):
        self.sent.append(data)
    def recv(self, count):
        part, self.response = self.response[:min(count, 3)], self.response[min(count, 3):]
        return part

class MdioTests(unittest.TestCase):
    def test_read_wire_format(self):
        sock = Socket(dcp.build_frame(3, 0x80, 1, b"\0\0\xc0\xf1"))
        self.assertEqual(dcp.mdio_read(sock, 1, 1, 3), 0xC0F1)
        self.assertEqual(sock.sent, [dcp.build_frame(3, 0, 1, b"\x01\x03")])
    def test_write_wire_format(self):
        sock = Socket(dcp.build_frame(3, 0x81, 65535, b"\0\0"))
        dcp.mdio_write(sock, 65535, 31, 31, 0xA55A)
        self.assertEqual(sock.sent, [dcp.build_frame(3, 1, 65535, b"\x1f\x1f\xa5\x5a")])
    def test_denied_not_retried(self):
        sock = Socket(dcp.build_frame(3, 0x81, 1, struct.pack(">H", 3)))
        with self.assertRaisesRegex(RuntimeError, "DENIED"):
            dcp.mdio_write(sock, 1, 1, 0, 0x8000)
        self.assertEqual(len(sock.sent), 1)
    def test_disconnect_not_retried(self):
        sock = Socket()
        with self.assertRaises(ConnectionError):
            dcp.mdio_write(sock, 1, 1, 0, 0x8000)
        self.assertEqual(len(sock.sent), 1)
    def test_bounds_before_send(self):
        sock = Socket()
        for seq, phy, reg, value in [(0,1,0,0),(1,32,0,0),(1,0,-1,0),(1,0,0,65536),(65536,1,0,0)]:
            with self.assertRaises(ValueError):
                dcp.mdio_write(sock, seq, phy, reg, value)
        self.assertEqual(sock.sent, [])
    def test_invalid_response(self):
        for op, body in [(0,b"\0\0\xff"),(1,b"\0\0\0")]:
            sock = Socket(dcp.build_frame(3, op | 0x80, 1, body))
            with self.assertRaises(ValueError):
                if op: dcp.mdio_write(sock, 1, 1, 0, 0)
                else: dcp.mdio_read(sock, 1, 1, 0)
    def test_cli_names(self):
        p = dcp.build_parser()
        self.assertEqual(p.parse_args(["host", "--mdio-read", "1", "3"]).mdio_read, ["1", "3"])
        self.assertEqual(p.parse_args(["host", "--mdio-write", "1", "4", "0x1e1"]).mdio_write, ["1", "4", "0x1e1"])
        self.assertIn("mdio", dcp.STREAM_SERVICES)
        self.assertNotIn("smi", dcp.STREAM_SERVICES)

if __name__ == "__main__":
    unittest.main()
