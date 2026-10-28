"""Independent standard-library TAR fixtures for the shared C CLI tests."""
import io
from pathlib import Path
import sys
import tarfile

destination = Path(sys.argv[1])
destination.mkdir(exist_ok=True)
for name, format_, member in (
    ("ustar", tarfile.USTAR_FORMAT, "./nested/plain.txt"),
    ("pax", tarfile.PAX_FORMAT, "nested/" + "длинное имя " * 8 + ".txt"),
    ("escape", tarfile.USTAR_FORMAT, "../outside"),
    ("absolute", tarfile.USTAR_FORMAT, "/outside"),
    ("link", tarfile.USTAR_FORMAT, "link"),
):
    with tarfile.open(destination / (name + ".tar"), "w", format=format_) as archive:
        if name == "ustar":
            directory = tarfile.TarInfo("./")
            directory.type = tarfile.DIRTYPE
            archive.addfile(directory)
        entry = tarfile.TarInfo(member)
        if name == "link":
            entry.type = tarfile.SYMTYPE
            entry.linkname = "../../outside"
            archive.addfile(entry)
        else:
            entry.size = 8
            archive.addfile(entry, io.BytesIO(b"external"))
bad_checksum = bytearray((destination / "ustar.tar").read_bytes())
bad_checksum[0] ^= 1
(destination / "checksum.tar").write_bytes(bad_checksum)
(destination / "truncated.tar").write_bytes((destination / "pax.tar").read_bytes()[:600])
