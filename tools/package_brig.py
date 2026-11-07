"""Build a checked Windows installation ZIP with exactly one brig/ root."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import tempfile
import zipfile
from pathlib import Path

from brig_assets import digest, pe_check
from validate_brig import MAP_METADATA_FILES, ROOT, validate


def copy_map_metadata(stage: Path) -> None:
    """Copy only the explicitly registered map metadata files into a mod stage."""
    for name in MAP_METADATA_FILES:
        source = ROOT / "_folder/brig" / name
        if not source.is_file():
            raise ValueError(f"required map metadata source is missing: {name}")
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)


def package(source: Path, client: Path, server: Path, output: Path, version: str, commit: str) -> Path:
    if not re.fullmatch(r"v[A-Za-z0-9][A-Za-z0-9._-]{0,79}", version):
        raise ValueError("release version must start with v and contain only letters, numbers, '.', '_' or '-'")
    pe_check(client, ("Initialize", "HUD_Init", "HUD_VidInit"))
    pe_check(server, ("GiveFnptrsToDll", "GetEntityAPI2"))
    validate(source, ROOT / "docs/brig-assets.json")
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"brig-{version}-windows-x86.zip"
    with tempfile.TemporaryDirectory(prefix="brig-package-") as temp:
        stage = Path(temp) / "brig"
        manifest = json.loads((ROOT / "docs/brig-assets.json").read_text(encoding="utf-8"))
        for name in manifest["files"]:
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source / name, target)

        # The central registry/CFG files are source-owned metadata, not borrowed assets.
        copy_map_metadata(stage)

        for binary, dest in ((client, "cl_dlls/client.dll"), (server, "dlls/hl.dll")):
            target = stage / dest
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(binary, target)
        build = {"version": version, "commit": commit, "platform": "windows-x86", "engine": "steam_legacy",
                 "dll_sha256": {"cl_dlls/client.dll": digest(client), "dlls/hl.dll": digest(server)}}
        (stage / "BUILD.json").write_text(json.dumps(build, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(validate(stage, ROOT / "docs/brig-assets.json",
                                  require_dlls=True, require_map_metadata=True), indent=2))
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as zipped:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    name = "brig/" + path.relative_to(stage).as_posix()
                    info = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    with path.open("rb") as src, zipped.open(info, "w", force_zip64=True) as dst:
                        shutil.copyfileobj(src, dst, 1024 * 1024)
        if archive.stat().st_size >= 2 * 1024 ** 3:
            raise ValueError("archive exceeds the GitHub Release per-asset 2 GiB limit")
        with zipfile.ZipFile(archive) as zipped:
            broken = zipped.testzip()
            if broken:
                raise ValueError(f"ZIP CRC check failed: {broken}")
            if any(not name.startswith("brig/") or ".." in Path(name).parts for name in zipped.namelist()):
                raise ValueError("invalid archive root")
    (output / (archive.name + ".sha256")).write_text(digest(archive) + "  " + archive.name + "\n", encoding="ascii")
    print(archive)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", default=os.environ.get("GITHUB_SHA", "local"))
    args = parser.parse_args()
    package(ROOT / "_folder/brig", args.client, args.server, args.output, args.version, args.commit)


if __name__ == "__main__":
    main()
