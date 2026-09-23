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

    def test_real_stream_record(self):
        data = b"BVT1" + struct.pack(">QHHBBBBIII", 42, 0x102, 0, 4, 1, 0, 16, 1, 2, 7)
        body = struct.pack(">QBIH", 123000, 2, 0, len(data)) + data
        frame = dcp.read_frame(Socket(dcp.build_frame(3, 0x50, 0, body)))
        decoded = dcp.decode_stream_event(frame, True, True)
        for text in ["record=42", "kind=MASTER_READ", "source=WEB", "address=0x01", "reg=0x02", "value=0x0007"]:
            self.assertIn(text, decoded)
        body = struct.pack(">BIH", 3, 9, len(data)) + data
        frame = dcp.read_frame(Socket(dcp.build_frame(3, 0x50, 0, body)))
        decoded = dcp.decode_stream_event(frame, False, False)
        self.assertIn("lost_delta=9", decoded)
        self.assertIn("overflow=1", decoded)

    def test_stream_does_not_present_failed_read_as_data(self):
        data = b"BVT1" + struct.pack(">QHHBBBBIII", 1, 0x102, 5, 3, 0, 0, 8, 54, 19, 0)
        frame = dcp.read_frame(Socket(dcp.build_frame(2, 0x50, 0, struct.pack(">BIH", 2, 0, 32) + data)))
        self.assertNotIn("value=", dcp.decode_stream_event(frame, False, True))
        frame = dcp.read_frame(Socket(dcp.build_frame(2, 0x50, 0, struct.pack(">BIH", 2, 0, 4) + b"oops")))
        self.assertIn("malformed", dcp.decode_stream_event(frame, False, True))
        frame = dcp.read_frame(Socket(dcp.build_frame(2, 0x50, 0, struct.pack(">BIH", 4, 0, 4) + b"test")))
        self.assertIn("synthetic=1", dcp.decode_stream_event(frame, False, True))

if __name__ == "__main__":
    unittest.main()
