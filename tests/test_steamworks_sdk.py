import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from verify_steamworks_sdk import REQUIRED_FILES, verify_sdk


def make_pe32_x86() -> bytes:
    data = bytearray(0x80)
    data[0:2] = b"MZ"
    data[0x3C:0x40] = (0x40).to_bytes(4, "little")
    data[0x40:0x44] = b"PE\0\0"
    data[0x44:0x46] = (0x014C).to_bytes(2, "little")
    data[0x58:0x5A] = (0x010B).to_bytes(2, "little")
    return bytes(data)


def make_elf32_i386() -> bytes:
    data = bytearray(20)
    data[0:4] = b"\x7fELF"
    data[4] = 1  # ELFCLASS32
    data[5] = 1  # little endian
    data[18:20] = (3).to_bytes(2, "little")  # EM_386
    return bytes(data)


def make_import_member(machine: int) -> bytes:
    # IMPORT_OBJECT_HEADER: Sig1, Sig2, Version, Machine, timestamp,
    # data size, ordinal/hint, and type information.
    fields = (0, 0xFFFF, 0, machine, 0, 0, 0, 0)
    sizes = (2, 2, 2, 2, 4, 4, 2, 2)
    return b"".join(value.to_bytes(size, "little") for value, size in zip(fields, sizes))


def make_coff_archive(machine: int) -> bytes:
    members = [
        ("/", b"\x00\x00\x00\x00"),  # linker index (not a COFF object)
        ("steam_api.dll/", make_import_member(machine)),
    ]
    archive = bytearray(b"!<arch>\n")
    for name, payload in members:
        header = (
            name.encode("ascii").ljust(16, b" ")
            + b"0".ljust(12, b" ")
            + b"0".ljust(6, b" ")
            + b"0".ljust(6, b" ")
            + b"100644".ljust(8, b" ")
            + str(len(payload)).encode("ascii").ljust(10, b" ")
            + b"`\n"
        )
        archive.extend(header)
        archive.extend(payload)
        if len(payload) & 1:
            archive.extend(b"\n")
    return bytes(archive)


class SteamworksSdkSetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.sdk = Path(self.temp.name) / "sdk"

    def tearDown(self):
        self.temp.cleanup()

    def _write_valid_layout(self):
        for relative in REQUIRED_FILES:
            target = self.sdk / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if relative == Path("redistributable_bin/steam_api.dll"):
                target.write_bytes(make_pe32_x86())
            elif relative == Path("redistributable_bin/steam_api.lib"):
                target.write_bytes(make_coff_archive(0x014C))
            elif relative == Path("redistributable_bin/linux32/libsteam_api.so"):
                target.write_bytes(make_elf32_i386())
            else:
                target.write_text("// header fixture\n", encoding="utf-8")

    def test_missing_sdk_explains_official_download_path(self):
        with self.assertRaisesRegex(ValueError, "partner.steamgames.com"):
            verify_sdk(self.sdk)

    def test_valid_32_bit_sdk_layout_passes(self):
        self._write_valid_layout()
        result = verify_sdk(self.sdk)
        self.assertEqual(result["headers"], 2)
        self.assertEqual(result["windows_x86_files"], 2)
        self.assertEqual(result["linux_x86_files"], 1)
        self.assertEqual(len(result["validated_files"]), len(REQUIRED_FILES))

    def test_missing_matchmaking_header_is_reported(self):
        self._write_valid_layout()
        (self.sdk / "public/steam/isteammatchmaking.h").unlink()
        with self.assertRaisesRegex(ValueError, "isteammatchmaking.h"):
            verify_sdk(self.sdk)

    def test_64_bit_windows_sdk_dll_is_rejected(self):
        self._write_valid_layout()
        data = bytearray(make_pe32_x86())
        data[0x44:0x46] = (0x8664).to_bytes(2, "little")
        data[0x58:0x5A] = (0x020B).to_bytes(2, "little")
        (self.sdk / "redistributable_bin/steam_api.dll").write_bytes(data)
        with self.assertRaisesRegex(ValueError, "32-bit Windows x86"):
            verify_sdk(self.sdk)

    def test_64_bit_windows_import_library_is_rejected(self):
        self._write_valid_layout()
        (self.sdk / "redistributable_bin/steam_api.lib").write_bytes(
            make_coff_archive(0x8664)
        )
        with self.assertRaisesRegex(ValueError, "x86 COFF archive/import library"):
            verify_sdk(self.sdk)

    def test_malformed_windows_import_library_is_rejected(self):
        self._write_valid_layout()
        (self.sdk / "redistributable_bin/steam_api.lib").write_bytes(b"!<arch>\ntruncated")
        with self.assertRaisesRegex(ValueError, "x86 COFF archive/import library"):
            verify_sdk(self.sdk)

    def test_non_i386_linux_sdk_is_rejected(self):
        self._write_valid_layout()
        data = bytearray(make_elf32_i386())
        data[18:20] = (62).to_bytes(2, "little")  # EM_X86_64
        (self.sdk / "redistributable_bin/linux32/libsteam_api.so").write_bytes(data)
        with self.assertRaisesRegex(ValueError, "32-bit Linux i386"):
            verify_sdk(self.sdk)


if __name__ == "__main__":
    unittest.main()
