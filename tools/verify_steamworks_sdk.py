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
