#!/usr/bin/python3
"""Generate a Neutrino-compatible root.shadow file.

Neutrino's libcrypt uses the GOST R 34.11-2012 (Streebog-512) digest for
the ``$G$`` password format.  This file intentionally has no third-party
Python dependencies, so it can be used on a fresh host together with the
repository's Neutrino build scripts.
"""

from __future__ import annotations

import argparse
import getpass
import os
import secrets
import sys
import tempfile
from pathlib import Path


MASK512 = (1 << 512) - 1
CRYPT_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyz./ABCDEFGHIJKLMNOPQRSTUVWXYZ"
CRYPT_ALPHABET_BYTES = CRYPT_ALPHABET.encode("ascii")
DEFAULT_SALT_LENGTH = 6
DEFAULT_SHADOW_LINE = "root:*:90:18565:0:0:0:0:0\n"


# GOST R 34.11-2012 linear transformation matrix.
A = (
    0x641C314B2B8EE083,
    0xC83862965601DD1B,
    0x8D70C431AC02A736,
    0x07E095624504536C,
    0x0EDD37C48A08A6D8,
    0x1CA76E95091051AD,
    0x3853DC371220A247,
    0x70A6A56E2440598E,
    0xA48B474F9EF5DC18,
    0x550B8E9E21F7A530,
    0xAA16012142F35760,
    0x492C024284FBAEC0,
    0x9258048415EB419D,
    0x39B008152ACB8227,
    0x727D102A548B194E,
    0xE4FA2054A80B329C,
    0xF97D86D98A327728,
    0xEFFA11AF0964EE50,
    0xC3E9224312C8C1A0,
    0x9BCF4486248D9F5D,
    0x2B838811480723BA,
    0x561B0D22900E4669,
    0xAC361A443D1C8CD2,
    0x456C34887A3805B9,
    0x5B068C651810A89E,
    0xB60C05CA30204D21,
    0x71180A8960409A42,
    0xE230140FC0802984,
    0xD960281E9D1D5215,
    0xAFC0503C273AA42A,
    0x439DA0784E745554,
    0x86275DF09CE8AAA8,
    0x0321658CBA93C138,
    0x0642CA05693B9F70,
    0x0C84890AD27623E0,
    0x18150F14B9EC46DD,
    0x302A1E286FC58CA7,
    0x60543C50DE970553,
    0xC0A878A0A1330AA6,
    0x9D4DF05D5F661451,
    0xACCC9CA9328A8950,
    0x4585254F64090FA0,
    0x8A174A9EC8121E5D,
    0x092E94218D243CBA,
    0x125C354207487869,
    0x24B86A840E90F0D2,
    0x486DD4151C3DFDB9,
    0x90DAB52A387AE76F,
    0x46B60F011A83988E,
    0x8C711E02341B2D01,
    0x05E23C0468365A02,
    0x0AD97808D06CB404,
    0x14AFF010BDD87508,
    0x2843FD2067ADEA10,
    0x5086E740CE47C920,
    0xA011D380818E8F40,
    0x83478B07B2468764,
    0x1B8E0B0E798C13C8,
    0x3601161CF205268D,
    0x6C022C38F90A4C07,
    0xD8045870EF14980E,
    0xAD08B0E0C3282D1C,
    0x47107DDD9B505A38,
    0x8E20FAA72BA0B470,
)


# GOST R 34.11-2012 substitution permutation.
P = bytes(
    (
        252, 238, 221, 17, 207, 110, 49, 22, 251, 196, 250, 218, 35, 197, 4, 77,
        233, 119, 240, 219, 147, 46, 153, 186, 23, 54, 241, 187, 20, 205, 95, 193,
        249, 24, 101, 90, 226, 92, 239, 33, 129, 28, 60, 66, 139, 1, 142, 79,
        5, 132, 2, 174, 227, 106, 143, 160, 6, 11, 237, 152, 127, 212, 211, 31,
        235, 52, 44, 81, 234, 200, 72, 171, 242, 42, 104, 162, 253, 58, 206, 204,
        181, 112, 14, 86, 8, 12, 118, 18, 191, 114, 19, 71, 156, 183, 93, 135,
        21, 161, 150, 41, 16, 123, 154, 199, 243, 145, 120, 111, 157, 158, 178, 177,
        50, 117, 25, 61, 255, 53, 138, 126, 109, 84, 198, 128, 195, 189, 13, 87,
        223, 245, 36, 169, 62, 168, 67, 201, 215, 121, 214, 246, 124, 34, 185, 3,
        224, 15, 236, 222, 122, 148, 176, 188, 220, 232, 40, 80, 78, 51, 10, 74,
        167, 151, 96, 115, 30, 0, 98, 68, 26, 184, 56, 130, 100, 159, 38, 65,
        173, 69, 70, 146, 39, 94, 85, 47, 140, 163, 165, 125, 105, 213, 149, 59,
        7, 88, 179, 64, 134, 172, 29, 247, 48, 55, 107, 228, 136, 217, 231, 137,
        225, 27, 131, 73, 76, 63, 248, 254, 141, 83, 170, 144, 202, 216, 133, 97,
        32, 113, 103, 164, 45, 43, 9, 91, 203, 155, 37, 208, 190, 229, 108, 82,
        89, 166, 116, 210, 230, 244, 180, 192, 209, 102, 175, 194, 57, 75, 99, 182,
    )
)


# Iteration constants, stored as little-endian 64-bit words.
C_WORDS = (
    (
        0xDD806559F2A64507, 0x05767436CC744D23, 0xA2422A08A460D315, 0x4B7CE09192676901,
        0x714EB88D7585C4FC, 0x2F6A76432E45D016, 0xEBCB2F81C0657C1F, 0xB1085BDA1ECADAE9,
    ),
    (
        0xE679047021B19BB7, 0x55DDA21BD7CBCD56, 0x5CB561C2DB0AA7CA, 0x9AB5176B12D69958,
        0x61D55E0F16B50131, 0xF3FEEA720A232B98, 0x4FE39D460F70B5D7, 0x6FA3B58AA99D2F1A,
    ),
    (
        0x991E96F50ABA0AB2, 0xC2B6F443867ADB31, 0xC1C93A376062DB09, 0xD3E20FE490359EB1,
        0xF2EA7514B1297B7B, 0x06F15E5F529C1F8B, 0x0A39FC286A3D8435, 0xF574DCAC2BCE2FC7,
    ),
    (
        0x220CBEBC84E3D12E, 0x3453EAA193E837F1, 0xD8B71333935203BE, 0xA9D72C82ED03D675,
        0x9D721CAD685E353F, 0x488E857E335C3C7D, 0xF948E1A05D71E4DD, 0xEF1FDFB3E81566D2,
    ),
    (
        0x601758FD7C6CFE57, 0x7A56A27EA9EA63F5, 0xDFFF00B723271A16, 0xBFCD1747253AF5A3,
        0x359E35D7800FFFBD, 0x7F151C1F1686104A, 0x9A3F410C6CA92363, 0x4BEA6BACAD474799,
    ),
    (
        0xFA68407A46647D6E, 0xBF71C57236904F35, 0x0AF21F66C2BEC6B6, 0xCFFAA6B71C9AB7B4,
        0x187F9AB49AF08EC6, 0x2D66C4F95142A46C, 0x6FA4C33B7A3039C0, 0xAE4FAEAE1D3AD3D9,
    ),
    (
        0x8886564D3A14D493, 0x3517454CA23C4AF3, 0x06476983284A0504, 0x0992ABC52D822C37,
        0xD3473E33197A93C9, 0x399EC6C7E6BF87C9, 0x51AC86FEBF240954, 0xF4C70E16EEAAC5EC,
    ),
    (
        0xA47F0DD4BF02E71E, 0x36ACC2355951A8D9, 0x69D18D2BD1A5C42F, 0xF4892BCB929B0690,
        0x89B4443B4DDBC49A, 0x4EB7F8719C36DE1E, 0x03E7AA020C6E4141, 0x9B1F5B424D93C9A7,
    ),
    (
        0x7261445183235ADB, 0x0E38DC92CB1F2A60, 0x7B2B8A9AA6079C54, 0x800A440BDBB2CEB1,
        0x3CD955B7E00D0984, 0x3A7D3A1B25894224, 0x944C9AD8EC165FDE, 0x378F5A541631229B,
    ),
    (
        0x74B4C7FB98459CED, 0x3698FAD1153BB6C3, 0x7A1E6C303B7652F4, 0x9FE76702AF69334B,
        0x1FFFE18A1B336103, 0x8941E71CFF8A78DB, 0x382AE548B2E4F3F3, 0xABBEDEA680056F52,
    ),
    (
        0x6BCAA4CD81F32D1B, 0xDEA2594AC06FD85D, 0xEFBACD1D7D476E98, 0x8A1D71EFEA48B9CA,
        0x2001802114846679, 0xD8FA6BBBEBAB0761, 0x3002C6CD635AFE94, 0x7BCD9ED0EFC889FB,
    ),
    (
        0x48BC924AF11BD720, 0xFAF417D5D9B21B99, 0xE71DA4AA88E12852, 0x5D80EF9D1891CC86,
        0xF82012D430219F9B, 0xCDA43C32BCDF1D77, 0xD21380B00449B17A, 0x378EE767F11631BA,
    ),
)


def _make_linear_table() -> tuple[tuple[int, ...], ...]:
    table = []
    for row in range(8):
        row_constants = A[row * 8 : row * 8 + 8]
        table.append(
            tuple(
                0 if not substituted else _linear_value(substituted, row_constants)
                for substituted in (P[value] for value in range(256))
            )
        )
    return tuple(table)


def _linear_value(value: int, row_constants: tuple[int, ...]) -> int:
    result = 0
    for bit, constant in enumerate(row_constants):
        if value & (1 << bit):
            result ^= constant
    return result


LINEAR_TABLE = _make_linear_table()
ROUND_CONSTANTS = tuple(
    b"".join(word.to_bytes(8, "little") for word in row) for row in C_WORDS
)


def _xor_blocks(*blocks: bytes) -> bytes:
    result = bytearray(blocks[0])
    for block in blocks[1:]:
        for index, value in enumerate(block):
            result[index] ^= value
    return bytes(result)


def _lps(block: bytes) -> bytes:
    words = [0] * 8
    for output_word in range(8):
        value = 0
        for input_word in range(8):
            value ^= LINEAR_TABLE[input_word][block[output_word + 8 * input_word]]
        words[output_word] = value
    return b"".join(word.to_bytes(8, "little") for word in words)


def _g(h: bytes, n: bytes, message: bytes) -> bytes:
    key = _lps(_xor_blocks(h, n))
    state = message
    for constant in ROUND_CONSTANTS:
        state = _lps(_xor_blocks(state, key))
        key = _lps(_xor_blocks(key, constant))
    return _xor_blocks(h, state, key, message)


def streebog512(message: bytes) -> bytes:
    """Return the little-endian Streebog-512 digest used by Neutrino."""

    h = bytes(64)
    n = 0
    sigma = 0
    offset = 0

    while len(message) - offset >= 64:
        block = message[offset : offset + 64]
        h = _g(h, n.to_bytes(64, "little"), block)
        n = (n + 512) & MASK512
        sigma = (sigma + int.from_bytes(block, "little")) & MASK512
        offset += 64

    tail = message[offset:]
    padded = tail + b"\x01" + bytes(63 - len(tail))
    h = _g(h, n.to_bytes(64, "little"), padded)
    n = (n + len(tail) * 8) & MASK512
    sigma = (sigma + int.from_bytes(padded, "little")) & MASK512
    zero = bytes(64)
    h = _g(h, zero, n.to_bytes(64, "little"))
    h = _g(h, zero, sigma.to_bytes(64, "little"))
    return h


def gost_crypt(password: bytes, salt: str) -> str:
    """Build Neutrino's ``$G$<salt>$<digest>`` password value."""

    digest = streebog512(password + salt.encode("ascii"))
    encoded = bytearray()
    for index in range(0, 63, 3):
        encoded.append(CRYPT_ALPHABET_BYTES[digest[index] & 0x3F])
        encoded.append(CRYPT_ALPHABET_BYTES[digest[index + 1] & 0x3F])
        encoded.append(
            CRYPT_ALPHABET_BYTES[
                ((digest[index] >> 2) & 0x30)
                | ((digest[index + 1] >> 4) & 0x0C)
                | (digest[index + 2] >> 6)
            ]
        )
        encoded.append(CRYPT_ALPHABET_BYTES[digest[index + 2] & 0x3F])
    return f"$G${salt}${encoded.decode('ascii')}"


def _read_password(password_stdin: bool) -> bytes:
    if password_stdin:
        password = sys.stdin.buffer.readline()
        if not password:
            raise ValueError("password-stdin did not provide a password")
        password = password.rstrip(b"\r\n")
    else:
        first = getpass.getpass("Root password: ")
        second = getpass.getpass("Retype root password: ")
        if first != second:
            raise ValueError("passwords do not match")
        password = first.encode("utf-8")

    if not password:
        raise ValueError("empty passwords are not allowed")
    if any(marker in password for marker in (b"\x00", b":", b"\n", b"\r")):
        raise ValueError("password contains a character that cannot be stored in root.shadow")
    return password


def _validate_salt(salt: str) -> None:
    # libcrypt copies at most eight salt bytes, while its $G$ output buffer
    # leaves room for at most seven bytes plus the 84-byte digest.
    if not 3 <= len(salt) <= 7:
        raise ValueError("salt must contain between 3 and 7 ASCII characters")
    if any(character not in CRYPT_ALPHABET for character in salt):
        raise ValueError("salt may contain only [0-9a-z./A-Z]")


def _write_shadow(path: Path, line: str, force: bool) -> None:
    path = path.expanduser()
    path.parent.mkdir(parents=True, exist_ok=True)

    if path.exists() and not force:
        try:
            previous = path.read_text(encoding="ascii")
        except (OSError, UnicodeError) as error:
            raise ValueError(f"cannot inspect existing output {path}: {error}") from error
        if previous != DEFAULT_SHADOW_LINE:
            raise ValueError(f"{path} already contains a password; use --force to replace it")

    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=str(path.parent)
    )
    try:
        with os.fdopen(file_descriptor, "w", encoding="ascii", newline="\n") as output:
            os.fchmod(output.fileno(), 0o600)
            output.write(line)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_name, path)
        os.chmod(path, 0o600)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except OSError:
            pass
        raise


def main() -> int:
    repository_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Generate a Neutrino $G$ root.shadow entry for the AX7020 image."
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=repository_root / "build/neutrino/root.shadow",
        help="output path (default: build/neutrino/root.shadow)",
    )
    parser.add_argument(
        "--salt",
        help="3-7 character salt; a random 6-character salt is used by default",
    )
    parser.add_argument(
        "--password-stdin",
        action="store_true",
        help="read one password line from stdin instead of using a hidden prompt",
    )
    parser.add_argument(
        "--print-hash",
        action="store_true",
        help="also print the generated hash (do not put it into build logs)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace an existing root.shadow that already contains a password",
    )
    args = parser.parse_args()

    try:
        salt = args.salt or "".join(secrets.choice(CRYPT_ALPHABET) for _ in range(DEFAULT_SALT_LENGTH))
        _validate_salt(salt)
        password = _read_password(args.password_stdin)
        password_hash = gost_crypt(password, salt)
        shadow_line = f"root:{password_hash}:90:18565:0:0:0:0:0\n"
        _write_shadow(args.output, shadow_line, args.force)
    except (EOFError, KeyboardInterrupt):
        print("Password generation cancelled", file=sys.stderr)
        return 130
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if args.print_hash:
        print(password_hash)
        print(f"root.shadow written to {args.output}", file=sys.stderr)
    else:
        print(f"root.shadow written to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
