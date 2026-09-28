"""Validation shared by the AX7020 boot-image builder and QSPI programmer."""
import ast
import hashlib
import json
import operator
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = Path(__file__).resolve().parent
ERASE_SIZE = 64 * 1024  # program_flash / W25Q256 block erase granularity


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(stream.read()).hexdigest()


def layout():
    """Read the actual board defaults, accepting integer arithmetic only."""
    header = (ROOT / "src/hardware/boards/ax7020/bvstk_qspi_layout.h").read_text()
    definitions = dict(re.findall(r"^#define\s+(QSPI_\w+)\s+([^\n]+)", header, re.M))
    operations = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul}

    def evaluate(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name) and node.id in definitions:
            return macro(node.id)
        if isinstance(node, ast.BinOp) and type(node.op) in operations:
            return operations[type(node.op)](evaluate(node.left), evaluate(node.right))
        raise ValueError("Unsupported QSPI layout expression")

    def macro(name):
        expression = re.sub(r"(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b", r"\1", definitions[name])
        return evaluate(ast.parse(expression, mode="eval").body)

    base, size = macro("QSPI_FS_BASE_BYTES"), macro("QSPI_FLASH_SIZE_BYTES")
    if not 0 < base < size or base % ERASE_SIZE or size != 32 * 1024 * 1024:
        raise ValueError("Unsupported AX7020 QSPI layout")
    return {"offset": 0, "fs_base": base, "flash_size": size, "erase_size": ERASE_SIZE}


def validate_image(path, geometry=None):
    geometry = geometry or layout()
    data = Path(path).read_bytes()
    if len(data) < 0xA0:
        raise ValueError("Truncated BOOT.bin")
    if len(data) > geometry["fs_base"]:
        raise ValueError("BOOT.bin overlaps flash:/")
    erase_end = (len(data) + ERASE_SIZE - 1) // ERASE_SIZE * ERASE_SIZE
    if erase_end > geometry["fs_base"]:
        raise ValueError("QSPI erase range overlaps flash:/")
    words = struct.unpack_from("<19I", data)
    if words[8:10] != (0xAA995566, 0x584C4E58):
        raise ValueError("Not a Zynq-7000 boot header at offset zero")
    if sum(words[8:19]) & 0xFFFFFFFF != 0xFFFFFFFF:
        raise ValueError("Invalid Zynq boot-header checksum")
    if words[10] != 0 or words[13] == 0 or words[12] + words[13] > len(data):
        raise ValueError("Unsupported or truncated FSBL partition")
    iht, pht = struct.unpack_from("<2I", data, 0x98)
    if iht < 0xA0 or iht + 64 > len(data) or pht < iht + 64 or pht + 192 > len(data):
        raise ValueError("Invalid boot partition-table offsets")
    if struct.unpack_from("<I", data, iht + 4)[0] != 3:
        raise ValueError("Expected exactly FSBL, bitstream and application")
    partitions = []
    for index, attributes in enumerate((0x10, 0x20, 0x10)):
        header = struct.unpack_from("<16I", data, pht + index * 64)
        start, size = header[5] * 4, header[2] * 4
        if sum(header) & 0xFFFFFFFF != 0xFFFFFFFF:
            raise ValueError(f"Invalid partition {index} checksum")
        if header[6] != attributes or header[0] != header[1] or header[1] != header[2]:
            raise ValueError(f"Unsupported partition {index} attributes/lengths")
        if size == 0 or start < pht + 192 or start + size > len(data):
            raise ValueError(f"Truncated partition {index}")
        if partitions and start < partitions[-1]["offset"] + partitions[-1]["size"]:
            raise ValueError("Overlapping boot partitions")
        partitions.append({"offset": start, "size": size, "load": header[3], "entry": header[4]})
    if partitions[0]["offset"] != words[12] or partitions[0]["size"] != words[13]:
        raise ValueError("FSBL boot and partition headers disagree")
    if partitions[2]["load"] < 0x100000 or not partitions[2]["load"] <= partitions[2]["entry"] < partitions[2]["load"] + partitions[2]["size"]:
        raise ValueError("Application entry is outside its DDR load range")
    return {"size": len(data), "erase_end": erase_end, "fsbl_offset": words[12],
            "fsbl_size": words[13], "partitions": partitions, "sha256": hashlib.sha256(data).hexdigest()}


def load_bundle(directory):
    directory = Path(directory).resolve()
    manifest = json.loads((directory / "manifest.json").read_text())
    if manifest.get("format") != 1 or manifest.get("target") != "freertos":
        raise ValueError("Unsupported boot bundle")
    if manifest.get("qspi") != layout():
        raise ValueError("Boot bundle does not match the current QSPI layout; rebuild it")
    required = {"BOOT.bin", "fsbl.elf", "qspi_read.elf", "ps7_init.tcl", "design.bit", "app_bvstk.elf", "design.xsa", "boot.bif"}
    if not required <= manifest.get("files", {}).keys():
        raise ValueError("Incomplete boot-bundle manifest")
    for name, expected in manifest["files"].items():
        if Path(name).name != name or sha256(directory / name) != expected:
            raise ValueError(f"Boot bundle checksum mismatch: {name}")
    actual = validate_image(directory / "BOOT.bin", manifest["qspi"])
    if actual != manifest["image"]:
        raise ValueError("BOOT.bin metadata mismatch")
    return manifest


def tool_env():
    env = os.environ.copy()
    xsct = shutil.which("xsct")
    if xsct:
        toolchain = Path(xsct).resolve().parents[1] / "gnu/aarch32/lin/gcc-arm-none-eabi/bin"
        if toolchain.is_dir():
            env["PATH"] = str(toolchain) + os.pathsep + env["PATH"]
    return env


def run(command, *, cwd=None, env=None, log=None, timeout=None):
    print("+ " + " ".join(map(str, command)), flush=True)
    if log is None:
        subprocess.run(list(map(str, command)), cwd=cwd, env=env, check=True, timeout=timeout)
    else:
        with Path(log).open("w") as stream:
            result = subprocess.run(list(map(str, command)), cwd=cwd, env=env,
                                    stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
        if result.returncode:
            print(Path(log).read_text(errors="replace")[-12000:], flush=True)
            raise subprocess.CalledProcessError(result.returncode, command)


def require_tools(names, env):
    for name in names:
        if not shutil.which(name, path=env["PATH"]):
            raise ValueError(f"{name} not found; source the Vitis settings64.sh first")
