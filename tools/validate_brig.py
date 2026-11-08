"""Fail CI/release packaging on missing, changed or incompatible brig assets."""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
import wave
from pathlib import Path, PurePosixPath

from brig_assets import cstring, digest, model_info, pe_check, sprite_check, wave_check

ROOT = Path(__file__).resolve().parents[1]

# These are intentional, narrowly-scoped BRIG GameUI overrides. The large
# preserved-resource audit manifest remains the source of all other hashes;
# these three resources are accepted only at this exact reviewed revision.
APPROVED_RESOURCE_OVERRIDES = {
    "resource/gamemenu.res": "83e64c0fbab103db520e857899152a0acd75c25c0ad2ea0804b3f34c21efbc3b",
    "resource/gameui_english.txt": "e4b337ae811b49ea6aaf8f31690c00aeab69f724540d985bd450974910c6738f",
    "resource/gameui_russian.txt": "d331658574853d758970162c0d19a5228a4f2d02c14a5273b250ac93c621357a",
}

# The map manifest is the source of truth for the generated metadata
# allowlist. Each registered BSP identifier must have exactly one CFG.
def _map_metadata_paths() -> tuple[str, ...]:
    manifest = ROOT / "_folder/brig/maps/brig_maplist.txt"
    paths = ["maps/brig_maplist.txt"]
    if not manifest.is_file():
        return tuple(paths)

    seen = set()
    for line_number, raw_line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        parts = line.split(None, 1)
        if len(parts) > 1 and not (
            parts[1].startswith("#") or parts[1].startswith("//")
        ):
            raise ValueError(f"{manifest}: unexpected data on line {line_number}")
        map_id = parts[0]
        if not re.fullmatch(r"[A-Za-z0-9_-]{1,31}", map_id):
            raise ValueError(f"{manifest}: unsafe map identifier on line {line_number}")
        normalized = map_id.lower()
        if normalized in seen:
            raise ValueError(f"{manifest}: duplicate map identifier on line {line_number}")
        if len(seen) >= 512:
            raise ValueError(f"{manifest}: map catalog exceeds the 512-entry limit")
        seen.add(normalized)
        paths.append(f"maps/{normalized}.cfg")
    return tuple(paths)


MAP_METADATA_FILES = _map_metadata_paths()


def validate_generated_map_metadata(
    mod: Path,
    actual: dict[str, Path],
    errors: list[str],
    require: bool = False,
) -> None:
    present = any(name in actual for name in MAP_METADATA_FILES)
    for name in MAP_METADATA_FILES:
        source = ROOT / "_folder/brig" / name
        target = actual.get(name)
        if target is None:
            if require or present:
                errors.append(f"missing generated map metadata: {name}")
            continue
        if not source.is_file():
            errors.append(f"source map metadata is missing: {source}")
        elif digest(target) != digest(source):
            errors.append(f"generated map metadata differs from source: {name}")


def validate(mod: Path, manifest: Path, require_dlls: bool = False,
             require_map_metadata: bool = False) -> dict:
    audit = json.loads(manifest.read_text(encoding="utf-8"))
    errors = []
    # File synchronization tools may keep dot-prefixed partial copies beside
    # large assets. They are never engine resources or release payload files.
    actual = {p.relative_to(mod).as_posix(): p for p in mod.rglob("*")
              if p.is_file() and not any(part.startswith(".") for part in p.relative_to(mod).parts)}
    expected = audit["files"]
    folded = {}
    counts = {"models": 0, "sprites": 0, "waves": 0, "maps": 0}
    for name, path in actual.items():
        if path.is_symlink() or name.lower() in folded:
            errors.append(f"symlink or case-insensitive duplicate: {name}")
        folded[name.lower()] = name
    generated = {"cl_dlls/client.dll", "dlls/hl.dll", "BUILD.json", *MAP_METADATA_FILES}
    validate_generated_map_metadata(mod, actual, errors, require=require_map_metadata)
    for name in sorted(actual.keys() - expected.keys() - generated):
        errors.append(f"untracked payload: {name}")
    for name, record in expected.items():
        path = actual.get(name)
        if path is None:
            errors.append(f"missing resource: {name}")
            continue
        actual_sha256 = digest(path)
        if actual_sha256 != record["sha256"] and actual_sha256 != APPROVED_RESOURCE_OVERRIDES.get(name):
            errors.append(f"checksum changed: {name}")
        source = PurePosixPath(record["source"])
        if "need restock" in source.parts and len(source.parts) >= 4 and source.parts[2] in {"cstrike", "cstrike_hd"}:
            tail = source.parts[3:]
            if tail[0].lower() not in {"models", "sound"} or (tail[0].lower() == "models" and len(tail) > 1 and tail[1].lower() == "player"):
                errors.append(f"forbidden CS source: {source}")
        if record["kind"] == "archive":
            if not (name.startswith("_Extra/") or "/_Extra/" in name):
                errors.append(f"archive in active path: {name}")
            continue
        ext = path.suffix.lower()
        try:
            if ext in {".mdl", ".spr", ".wav"} and len(name.encode("ascii")) >= 64:
                raise ValueError("path exceeds MAX_QPATH=64")
            if ext == ".mdl":
                info = model_info(path.read_bytes())
                counts["models"] += 1
                if info["kind"] == "model":
                    for _, ref in info["groups"]:
                        if ref.lower() not in folded:
                            raise ValueError(f"missing sequence group {ref}")
                    if info["bodies"] and info["textures"] == 0:
                        companion = str(PurePosixPath(name).with_suffix("")) + "t.mdl"
                        if companion.lower() not in folded:
                            raise ValueError(f"missing texture companion {companion}")
            elif ext == ".spr":
                sprite_check(path.read_bytes())
                counts["sprites"] += 1
            elif ext == ".wav":
                wave_check(path)
                counts["waves"] += 1
            elif ext == ".ogg":
                raise ValueError("OGG is not playable by steam_legacy")
            elif ext == ".bsp":
                data = path.read_bytes()
                if len(data) < 124 or struct.unpack_from("<i", data)[0] != 30:
                    raise ValueError("expected GoldSource BSP v30")
                for i in range(15):
                    start, size = struct.unpack_from("<ii", data, 4 + i * 8)
                    if start < 0 or size < 0 or start + size > len(data):
                        raise ValueError("BSP lump outside file")
                counts["maps"] += 1
        except (ValueError, struct.error, wave.Error, EOFError, UnicodeError) as error:
            errors.append(f"{name}: {error}")
    present = {x["sha256"] for x in expected.values()}
    for source, sha in audit["preserved_sources"].items():
        if sha not in present:
            errors.append(f"original addon/Extras file was lost: {source}")
    for name in ("liblist.gam", "valve.rc", "default.cfg", "skill.cfg", "delta.lst", "titles.txt", "maps/c0a0.bsp", "maps/t0a0.bsp", "maps/crossfire.bsp", "sprites/hud.txt", "models/player.mdl"):
        if name not in actual:
            errors.append(f"required startup file missing: {name}")
    info = (mod / "liblist.gam").read_text(encoding="utf-8")
    for key, val in (("game", "Brig RPG 2"), ("gamedll", "dlls\\hl.dll"), ("startmap", "c0a0"), ("trainmap", "t0a0")):
        if not re.search(rf'^\s*{key}\s+"{re.escape(val)}"', info, re.M):
            errors.append(f"incorrect liblist.gam {key}")
    if (mod / "delta.lst").read_bytes() != (ROOT / "network/delta.lst").read_bytes():
        errors.append("delta.lst does not match the source network schema")
    if any(name in actual for name in ("config.cfg", "autoexec.cfg", "steam_autocloud.vdf")):
        errors.append("personal configuration included")
    # HUD definitions reference SPR basenames; verify both 320/640 variants.
    for text in (mod / "sprites").glob("*.txt"):
        for line in text.read_text(errors="replace").splitlines():
            row = line.split()
            if len(row) >= 7 and row[1] in {"320", "640"} and "sprites/" + row[2].lower() + ".spr" not in folded:
                errors.append(f"{text.name}: missing sprite {row[2]}")
    if require_dlls:
        for name, exports in (("cl_dlls/client.dll", ("Initialize", "HUD_Init", "HUD_VidInit")), ("dlls/hl.dll", ("GiveFnptrsToDll", "GetEntityAPI2"))):
            try:
                pe_check(mod / name, exports)
            except (ValueError, OSError, struct.error) as error:
                errors.append(str(error))
    if errors:
        raise ValueError("\n".join(errors[:80]) + (f"\n... {len(errors)} errors total" if len(errors) > 80 else ""))
    return {"files": len(actual), **counts, "preserved_originals": len(audit["preserved_sources"]), "dlls_required": require_dlls}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mod", type=Path, default=ROOT / "_folder/brig")
    parser.add_argument("--manifest", type=Path, default=ROOT / "docs/brig-assets.json")
    parser.add_argument("--require-dlls", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(validate(args.mod, args.manifest, args.require_dlls), indent=2))
    except (ValueError, OSError) as error:
        sys.exit(str(error))


if __name__ == "__main__":
    main()
