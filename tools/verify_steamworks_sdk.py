"""Validate a locally acquired Steamworks SDK for BRIG's 32-bit Steam Legacy targets."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SDK_ROOT = ROOT / "_devfolder" / "steamworks" / "sdk"

REQUIRED_FILES = (
    Path("public/steam/steam_api.h"),
    Path("public/steam/isteammatchmaking.h"),
    Path("redistributable_bin/steam_api.dll"),
    Path("redistributable_bin/steam_api.lib"),
    Path("redistributable_bin/linux32/libsteam_api.so"),
)


def _is_win32_x86_pe(path: Path) -> bool:
    with path.open("rb") as stream:
        dos_header = stream.read(64)
        if len(dos_header) < 64 or dos_header[:2] != b"MZ":
            return False
        pe_offset = int.from_bytes(dos_header[0x3C:0x40], "little")
        if pe_offset < 64:
            return False
        stream.seek(pe_offset)
        pe_header = stream.read(26)

    if len(pe_header) < 26 or pe_header[:4] != b"PE\0\0":
        return False
    machine = int.from_bytes(pe_header[4:6], "little")
    optional_magic = int.from_bytes(pe_header[24:26], "little")
    return machine == 0x014C and optional_magic == 0x010B


def _is_linux_x86_elf(path: Path) -> bool:
    with path.open("rb") as stream:
        header = stream.read(20)
    if len(header) < 20 or header[:4] != b"\x7fELF":
        return False
    elf_class = header[4]
    data_encoding = header[5]
    machine = int.from_bytes(header[18:20], "little" if data_encoding == 1 else "big")
    return elf_class == 1 and data_encoding == 1 and machine == 3


def verify_sdk(sdk_root: str | Path = DEFAULT_SDK_ROOT) -> dict[str, object]:
    root = Path(sdk_root).expanduser()
    if not root.is_dir():
        raise ValueError(
            f"Steamworks SDK folder not found: {root}. "
            "Download it from https://partner.steamgames.com/doc/sdk and extract its sdk directory here."
        )

    missing = [str(relative) for relative in REQUIRED_FILES if not (root / relative).is_file()]
    empty = [
        str(relative)
        for relative in REQUIRED_FILES
        if (root / relative).is_file() and (root / relative).stat().st_size == 0
    ]
    if missing or empty:
        problems = []
        if missing:
            problems.append("missing files: " + ", ".join(missing))
        if empty:
            problems.append("empty files: " + ", ".join(empty))
        raise ValueError("Steamworks SDK is incomplete (" + "; ".join(problems) + ")")

    windows_dll = root / "redistributable_bin/steam_api.dll"
    windows_lib = root / "redistributable_bin/steam_api.lib"
    linux_so = root / "redistributable_bin/linux32/libsteam_api.so"
    if not _is_win32_x86_pe(windows_dll):
        raise ValueError("redistributable_bin/steam_api.dll is not a valid 32-bit Windows x86 PE binary")
    if windows_lib.read_bytes()[:8] != b"!<arch>\n":
        raise ValueError("redistributable_bin/steam_api.lib is not a valid COFF archive/import library")
    if not _is_linux_x86_elf(linux_so):
        raise ValueError("redistributable_bin/linux32/libsteam_api.so is not a valid 32-bit Linux i386 ELF binary")

    return {
        "sdk_root": str(root.resolve()),
        "headers": 2,
        "windows_x86_files": 2,
        "linux_x86_files": 1,
        "validated_files": [str(path) for path in REQUIRED_FILES],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, default=DEFAULT_SDK_ROOT)
    args = parser.parse_args(argv)
    try:
        result = verify_sdk(args.sdk_root)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
