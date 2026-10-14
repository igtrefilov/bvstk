#!/usr/bin/env python3
"""Production C policy parser and validated Python client, including fragmented TCP."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import unittest
from unittest import mock

SERVER = sys.argv.pop(1)
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("dcp2_client", ROOT / "scripts/dcp2/dcp2_client.py")
dcp = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = dcp
spec.loader.exec_module(dcp)

class Socket:
    def __init__(self, response=b""):
        self.response, self.sent = response, []
    def sendall(self, data):
        self.sent.append(data)
    def recv(self, size):
        count = min(size, 1)
        result, self.response = self.response[:count], self.response[count:]
        return result

class ClientTests(unittest.TestCase):
    def test_wire_and_interleaved_event(self):
        for bus, address, rule in [(2, 54, b"\x01\x63"), (3, 1, b"\x03\xab\xcd\xff\xff")]:
            for opcode in (2, 4, 5, 6):
                sock = Socket(dcp.build_frame(6, 0x50, 0) + dcp.build_frame(bus, opcode | 0x80, 65535, b"\0\0"))
                with contextlib.redirect_stderr(io.StringIO()):
                    if opcode == 2: dcp.policy_set(sock, bus, 65535, address, "blacklist")
                    elif opcode == 6: dcp.policy_clear(sock, bus, 65535, address, "blacklist")
                    else:
                        fn = dcp.policy_rule_add if opcode == 4 else dcp.policy_rule_delete
                        fn(sock, bus, 65535, address, 1, 1 if bus == 2 else 3,
                           99 if bus == 2 else 0xABCD, None if bus == 2 else 0xFFFF)
                self.assertEqual(sock.sent, [dcp.build_frame(bus, opcode, 65535,
                    bytes([address, 1]) + (rule if opcode in (4, 5) else b""))])
    def test_counts_and_masks(self):
        data = struct.pack(">BHH", 1, 256, 256) + b"\x01\x02" * 512
        policy = dcp.decode_policy(2, data)
        self.assertEqual(policy["mode"], "blacklist")
        self.assertEqual(len(policy["whitelist"]), 256)
        self.assertEqual(len(policy["blacklist"]), 256)
        data = struct.pack(">BHHBHHBHH", 0, 1, 1, 3, 0xABCD, 0xFF00, 15, 0xFFFF, 0)
        self.assertEqual(dcp.decode_policy(3, data)["blacklist"], [{"reg": 15, "val": 65535, "mask": 0}])
    def test_malformed_decoding(self):
        for service, data in [(2,b""),(2,b"\x02\0\0\0\0"),(2,b"\0\0\1\0\0"),
                              (2,b"\0\0\0\0\0\x01"),(2,struct.pack(">BHH",0,257,0)+b"\0\0"*257),
                              (3,struct.pack(">BHHBHH",0,1,0,32,0,0)),
                              (3,struct.pack(">BHH",0,65,0)+b"\0"*325), (1,b"\0"*5)]:
            with self.assertRaises(ValueError): dcp.decode_policy(service, data)
    def test_validation_before_send(self):
        sock = Socket()
        cases = [(2,0,54,0,1,1,None), (2,65536,54,0,1,1,None), (2,1,128,0,1,1,None),
                 (3,1,32,0,1,1,0), (2,1,54,2,1,1,None), (3,1,1,-1,1,1,0),
                 (2,1,54,0,256,1,None), (3,1,1,0,32,1,0), (2,1,54,0,1,256,None),
                 (3,1,1,0,1,65536,0), (3,1,1,0,1,1,None), (3,1,1,0,1,1,-1),
                 (3,1,1,0,1,1,65536), (2,1,54,0,1,1,0), (9,1,1,0,1,1,None)]
        for fields in cases:
            with self.assertRaises(ValueError): dcp.policy_rule_add(sock, *fields)
        for value in ("allow", -1, 2):
            with self.assertRaises(ValueError): dcp.i2c_policy_set(sock, 1, 54, value)
            with self.assertRaises(ValueError): dcp.mdio_policy_clear(sock, 1, 1, value)
        self.assertEqual(sock.sent, [])
    def test_errors_and_disconnect_are_not_retried(self):
        for opcode, fn in [(2, lambda s: dcp.mdio_policy_set(s,1,1,0)),
                           (4, lambda s: dcp.mdio_policy_rule_add(s,1,1,0,3,1,65535)),
                           (5, lambda s: dcp.mdio_policy_rule_delete(s,1,1,0,3,1,65535)),
                           (6, lambda s: dcp.mdio_policy_clear(s,1,1,0))]:
            for response, error in [(b"",ConnectionError),
                                     (dcp.build_frame(3, opcode|0x80,1,b"\0\6"),RuntimeError),
                                     (dcp.build_frame(3, opcode|0x80,1,b"\0\0\0"),ValueError)]:
                sock=Socket(response)
                with self.assertRaises(error): fn(sock)
                self.assertEqual(len(sock.sent), 1)
    def test_response_identity_and_status(self):
        for service, opcode, seq, body in [(2, 0x82, 1, b"\0\0"),
                                          (3, 0x83, 1, b"\0\0"),
                                          (3, 0x82, 2, b"\0\0"),
                                          (3, 0x02, 1, b"\0\0"),
                                          (3, 0x82, 1, b"\0")]:
            sock = Socket(dcp.build_frame(service, opcode, seq, body))
            with self.assertRaises(ValueError): dcp.mdio_policy_set(sock, 1, 1, 0)
            self.assertEqual(len(sock.sent), 1)
        for status, name in [(1, "ERR_MALFORMED"), (2, "ERR_UNSUPPORTED"), (3, "ERR_DENIED"),
                             (4, "ERR_BUSY"), (5, "ERR_TIMEOUT"), (6, "ERR_RANGE"),
                             (7, "ERR_INTERNAL"), (0x1234, "0x1234")]:
            sock = Socket(dcp.build_frame(3, 0x82, 1, struct.pack(">H", status)))
            with self.assertRaisesRegex(RuntimeError, "MDIO POLICY_SET failed: " + name):
                dcp.mdio_policy_set(sock, 1, 1, 0)
            self.assertEqual(len(sock.sent), 1)
    def test_cli_validation_before_connection(self):
        cases = [("i2c", "get", ["128"]), ("mdio", "get", ["32"]),
                 ("i2c", "set", ["54", "unknown"]), ("mdio", "clear", ["1", "2"]),
                 ("i2c", "add", ["54", "0", "256", "1"]),
                 ("i2c", "delete", ["54", "0", "1", "256"]),
                 ("mdio", "add", ["1", "0", "32", "1", "0"]),
                 ("mdio", "delete", ["1", "0", "3", "65536", "0"]),
                 ("mdio", "add", ["1", "0", "3", "1", "65536"]),
                 ("mdio", "add", ["1", "0", "3", "1", "-1"])]
        for bus, action, fields in cases:
            args = dcp.build_parser().parse_args(["host", f"--{bus}-policy-{action}"] + fields)
            with mock.patch.object(dcp.socket, "create_connection") as connect:
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(dcp.run_policy_operation(args), 1)
                connect.assert_not_called()
    def test_cli_options_and_numeric_arguments(self):
        parser = dcp.build_parser()
        for bus, address in [("i2c","54"),("mdio","1")]:
            for action, rest in [("get",[]),("set",["whitelist"]),("clear",["1"]),
                                 ("add",["blacklist","3","0xab"]+(["0xff"] if bus=="mdio" else [])),
                                 ("delete",["0","3","1"]+(["0"] if bus=="mdio" else []))]:
                fields=[address]+rest
                args=parser.parse_args(["host",f"--{bus}-policy-{action}"]+fields)
                self.assertEqual(getattr(args,f"{bus}_policy_{action}"),fields)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            parser.parse_args(["host","--mdio-policy-add","1","0","1","2"])

class ProductionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = subprocess.Popen([SERVER], stdout=subprocess.PIPE, text=True)
        line = cls.server.stdout.readline()
        if not line: raise RuntimeError("C policy parser self-tests failed")
        cls.port = int(line)
    @classmethod
    def tearDownClass(cls):
        cls.server.terminate(); cls.server.wait(timeout=5); cls.server.stdout.close()
    def setUp(self):
        self.sock=socket.create_connection(("127.0.0.1",self.port),timeout=2)
        self.output=contextlib.redirect_stderr(io.StringIO()); self.output.__enter__()
    def tearDown(self):
        self.output.__exit__(None,None,None); self.sock.close()
    def raw(self,bus,opcode,body):
        packet=dcp.build_frame(bus,opcode,1,body)
        for byte in packet: self.sock.sendall(bytes([byte]))
        return dcp.wait_for_response(self.sock,bus,opcode,1)
    def test_i2c_full_policy_lifecycle(self):
        self.assertEqual(dcp.i2c_policy_get(self.sock,1,54),{"mode":"whitelist","whitelist":[],"blacklist":[]})
        for rule_list in (0,1):
            dcp.i2c_policy_rule_add(self.sock,2,54,rule_list,1,99)
            dcp.i2c_policy_rule_add(self.sock,3,54,rule_list,1,99)
        dcp.i2c_policy_set(self.sock,4,54,"blacklist")
        state=dcp.i2c_policy_get(self.sock,5,54)
        self.assertEqual(state,{"mode":"blacklist","whitelist":[{"reg":1,"val":99}],"blacklist":[{"reg":1,"val":99}]})
        dcp.i2c_policy_rule_delete(self.sock,6,54,0,1,99)
        self.assertEqual(dcp.i2c_policy_get(self.sock,7,54)["blacklist"],state["blacklist"])
        with self.assertRaisesRegex(RuntimeError,"ERR_RANGE"):
            dcp.i2c_policy_rule_delete(self.sock,8,54,0,1,99)
        dcp.i2c_policy_clear(self.sock,9,54,1)
        self.assertEqual(dcp.i2c_policy_get(self.sock,10,54),{"mode":"blacklist","whitelist":[],"blacklist":[]})
    def test_mdio_masked_identity_and_lists(self):
        dcp.mdio_policy_rule_add(self.sock,1,1,0,3,0xABCD,0xFF00)
        dcp.mdio_policy_rule_add(self.sock,2,1,0,3,0xABEF,0xFF00)
        dcp.mdio_policy_rule_add(self.sock,3,1,1,3,0xABCD,0xFFFF)
        state=dcp.mdio_policy_get(self.sock,4,1)
        self.assertEqual(state["whitelist"],[{"reg":3,"val":0xAB00,"mask":0xFF00}])
        self.assertEqual(state["blacklist"],[{"reg":3,"val":0xABCD,"mask":0xFFFF}])
        dcp.mdio_policy_rule_delete(self.sock,5,1,0,3,0xAB12,0xFF00)
        dcp.mdio_policy_clear(self.sock,6,1,0)
        dcp.mdio_policy_set(self.sock,7,1,1)
        self.assertEqual(dcp.mdio_policy_get(self.sock,8,1),{"mode":"blacklist","whitelist":[],"blacklist":state["blacklist"]})
    def test_errors_from_production_parser(self):
        for bus,address,rule in [(2,54,b"\x01\x01"),(3,1,b"\x03\0\1\xff\xff")]:
            for opcode in range(2,7):
                body=bytes([address])+(b"" if opcode==3 else b"\0"+(rule if opcode in (4,5) else b""))
                for wrong in (body[:-1],body+b"\0"):
                    response=self.raw(bus,opcode,wrong)
                    self.assertEqual(response.body,b"\0\1")
                if opcode!=3:
                    self.assertEqual(self.raw(bus,opcode,bytes([address,2])+body[2:]).body,b"\0\1")
            self.assertEqual(self.raw(bus,63,b"").body,b"\0\2")
            self.assertEqual(self.raw(bus,3,b"\xff").body,b"\0\6")
            self.assertEqual(self.raw(bus,3,b"\x07").body,b"\0\6")
            self.assertEqual(self.raw(bus,5,bytes([address,0])+rule).body,b"\0\6")
        self.assertEqual(self.raw(2,4,b"\x36\0\x20\1").body,b"\0\6")
        self.assertEqual(self.raw(2,4,b"\x36\0\1\x80").body,b"\0\6")
        self.assertEqual(self.raw(3,4,b"\1\0\x10\0\1\xff\xff").body,b"\0\6")
    def test_capacity_error(self):
        for i in range(64): dcp.mdio_policy_rule_add(self.sock,i+1,1,0,3,i,65535)
        with self.assertRaisesRegex(RuntimeError,"ERR_RANGE"):
            dcp.mdio_policy_rule_add(self.sock,65,1,0,3,64,65535)
        self.assertEqual(len(dcp.mdio_policy_get(self.sock,66,1)["whitelist"]),64)
        dcp.mdio_policy_rule_add(self.sock,67,1,0,3,0,65535)
        self.assertEqual(len(dcp.mdio_policy_get(self.sock,68,1)["whitelist"]),64)
    def test_cli_against_production_parser(self):
        # The fixture handles one connection at a time and resets state on connect.
        self.sock.close()
        for bus, address, reg, value, mask in [("i2c", "0x36", "1", "0x63", []),
                                               ("mdio", "1", "3", "0xabcd", ["0xff00"])]:
            for action, rest in [("get", []), ("set", ["blacklist"]),
                                 ("add", ["whitelist", reg, value] + mask),
                                 ("clear", ["blacklist"]),
                                 ("delete", ["whitelist", reg, value] + mask)]:
                with self.subTest(bus=bus, action=action):
                    result = subprocess.run([sys.executable, str(ROOT / "scripts/dcp2/dcp2_client.py"),
                                             "127.0.0.1", "--port", str(self.port),
                                             f"--{bus}-policy-{action}", address] + rest,
                                            capture_output=True, text=True, timeout=5)
                    self.assertIn("[event-before-response]", result.stderr)
                    if action == "delete":
                        self.assertEqual(result.returncode, 1)
                        self.assertEqual(result.stdout, "")
                        self.assertIn(f"{bus.upper()} POLICY_DELETE failed: ERR_RANGE", result.stderr)
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        if action == "get":
                            self.assertEqual(json.loads(result.stdout),
                                             {"mode": "whitelist", "whitelist": [], "blacklist": []})
                        else:
                            self.assertEqual(result.stdout, f"{bus.upper()} POLICY_{action.upper()} -> OK\n")

if __name__ == "__main__": unittest.main()
