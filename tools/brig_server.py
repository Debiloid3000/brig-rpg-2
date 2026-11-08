"""Launch a BRIG RPG 2 dedicated server through a local Steam Legacy HLDS install."""
from __future__ import annotations

import argparse
import json
import os
import re
import signal
import subprocess
from pathlib import Path
from typing import Sequence

MAP_ID_RE = re.compile(r"[A-Za-z0-9_-]{1,31}\Z")
SUPPORTED_MODES = ("coop", "deathmatch", "team_deathmatch")


def _safe_console_value(label: str, value: str, max_length: int, allow_empty: bool) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{label} must be text")
    if (not allow_empty and not value) or len(value) > max_length:
        raise ValueError(f"{label} must be between 1 and {max_length} characters")
    for char in value:
        code = ord(char)
        if code < 32 or code == 127 or char in {'"', "\\", ";"}:
            raise ValueError(f"{label} contains a console delimiter or control character")
    return value


def _read_mode_flag(config_path: Path, key: str) -> bool:
    found: str | None = None
    for line_number, raw in enumerate(config_path.read_text(encoding="utf-8-sig").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("//"):
            continue
        parts = line.split(None, 1)
        if parts[0].lower() != key:
            continue
        if found is not None:
            raise ValueError(f"{config_path}: duplicate {key} on line {line_number}")
        if len(parts) != 2:
            raise ValueError(f"{config_path}: {key} needs a 0/1 value on line {line_number}")
        value = parts[1].strip()
        if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
            value = value[1:-1]
        if value not in {"0", "1"}:
            raise ValueError(f"{config_path}: {key} must be 0 or 1 on line {line_number}")
        found = value
    return found == "1"


def build_command(
    hlds_exe: str | Path,
    mod_dir: str | Path,
    map_name: str = "c0a0",
    mode: str = "coop",
    max_players: int = 8,
    port: int = 27015,
    hostname: str = "BRIG RPG 2",
    password: str = "",
    lan: bool = True,
    time_limit_minutes: int = 20,
    frag_limit: int = 0,
    friendly_fire: bool = False,
    fall_damage: bool = False,
    weapon_stay: bool = False,
    flashlight: bool = True,
    allow_monsters: bool = False,
    allow_spectators: bool = False,
) -> tuple[list[str], Path]:
    """Validate a BRIG dedicated-server install and return (argv, working_directory)."""
    if mode not in SUPPORTED_MODES:
        raise ValueError(f"unsupported mode: {mode}")
    if not MAP_ID_RE.fullmatch(map_name):
        raise ValueError("map must be a plain BSP identifier (letters, digits, _ or -)")
    if not 2 <= max_players <= 32:
        raise ValueError("max_players must be between 2 and 32")
    if not 1 <= port <= 65535:
        raise ValueError("port must be between 1 and 65535")
    if not 0 <= time_limit_minutes <= 120:
        raise ValueError("time_limit_minutes must be between 0 and 120")
    if not 0 <= frag_limit <= 1000:
        raise ValueError("frag_limit must be between 0 and 1000")

    hostname = _safe_console_value("hostname", hostname, 48, False)
    password = _safe_console_value("password", password, 32, True)

    executable = Path(hlds_exe).expanduser().resolve(strict=True)
    if not executable.is_file() or executable.name.lower() != "hlds.exe":
        raise ValueError("hlds_exe must point to an existing hlds.exe")

    mod = Path(mod_dir).expanduser().resolve(strict=True)
    if not mod.is_dir():
        raise ValueError("mod_dir must be a directory")
    if mod.parent != executable.parent:
        raise ValueError("the BRIG mod folder must be beside hlds.exe inside the HLDS root")
    if not (mod / "liblist.gam").is_file():
        raise ValueError("mod_dir is missing liblist.gam")
    if not (mod / "dlls" / "hl.dll").is_file():
        raise ValueError("mod_dir is missing dlls/hl.dll; build and stage the server DLL first")

    normalized_map = map_name.lower()
    map_file = mod / "maps" / f"{normalized_map}.bsp"
    if not map_file.is_file():
        raise ValueError(f"map BSP is missing: {map_file}")
    with map_file.open("rb") as stream:
        header = stream.read(4)
    if len(header) != 4 or int.from_bytes(header, "little") != 30:
        raise ValueError(f"{map_file} is not a GoldSrc BSP version 30 map")

    metadata = mod / "maps" / f"{normalized_map}.cfg"
    if not metadata.is_file():
        raise ValueError(f"map metadata is missing: {metadata}")
    flag_key = {
        "coop": "brig_coop",
        "deathmatch": "brig_deathmatch",
        "team_deathmatch": "brig_team_deathmatch",
    }[mode]
    if not _read_mode_flag(metadata, flag_key):
        raise ValueError(f"map {normalized_map} is not enabled for {mode} in its CFG")

    deathmatch = "0" if mode == "coop" else "1"
    coop = "1" if mode == "coop" else "0"
    teamplay = "1" if mode == "team_deathmatch" else "0"
    lan_value = "1" if lan else "0"
    session_cvars = [
        ("+mp_timelimit", str(time_limit_minutes)),
        ("+mp_fraglimit", str(frag_limit)),
        ("+mp_friendlyfire", "1" if friendly_fire else "0"),
        ("+mp_falldamage", "1" if fall_damage else "0"),
        ("+mp_weaponstay", "1" if weapon_stay else "0"),
        ("+mp_flashlight", "1" if flashlight else "0"),
        ("+mp_allowmonsters", "1" if (allow_monsters or mode == "coop") else "0"),
        ("+allow_spectators", "1" if allow_spectators else "0"),
    ]

    # Mode and maxplayers must be established before +map so InstallGameRules()
    # sees the correct gpGlobals values when the BSP starts. Hostname/password/
    # LAN are applied again after +map so server.cfg defaults cannot win later.
    command = [
        str(executable),
        "-console",
        "-game", mod.name,
        "-port", str(port),
        "+maxplayers", str(max_players),
        "+deathmatch", deathmatch,
        "+coop", coop,
        "+mp_teamplay", teamplay,
        "+sv_lan", lan_value,
        "+hostname", hostname,
        "+sv_password", password,
    ]
    for key, value in session_cvars:
        command.extend((key, value))
    command.extend(("+map", normalized_map))
    command.extend(("+hostname", hostname, "+sv_lan", lan_value, "+sv_password", password))
    for key, value in session_cvars:
        command.extend((key, value))
    return command, executable.parent


def run_server(command: Sequence[str], working_directory: Path) -> int:
    creationflags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0) if os.name == "nt" else 0
    process = subprocess.Popen(list(command), cwd=str(working_directory), creationflags=creationflags)
    print(f"Started BRIG dedicated server (PID {process.pid}). Press Ctrl+C to stop it.")
    try:
        return process.wait()
    except KeyboardInterrupt:
        try:
            if os.name == "nt" and hasattr(signal, "CTRL_BREAK_EVENT"):
                process.send_signal(signal.CTRL_BREAK_EVENT)
            else:
                process.send_signal(signal.SIGINT)
            return process.wait(timeout=10)
        except (OSError, subprocess.TimeoutExpired):
            process.terminate()
            return process.wait(timeout=5)


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hlds", required=True, help="full path to hlds.exe")
    parser.add_argument("--mod-dir", required=True, help="BRIG mod directory beside hlds.exe")
    parser.add_argument("--map", default="c0a0", dest="map_name", help="installed BRIG BSP identifier")
    parser.add_argument("--mode", choices=SUPPORTED_MODES, default="coop")
    parser.add_argument("--max-players", type=int, default=8)
    parser.add_argument("--port", type=int, default=27015)
    parser.add_argument("--hostname", default="BRIG RPG 2")
    parser.add_argument("--password", default="")
    parser.add_argument("--time-limit", type=int, default=20, help="minutes; 0 disables the time limit")
    parser.add_argument("--frag-limit", type=int, default=0, help="frags; 0 disables the frag limit")
    parser.add_argument("--friendly-fire", action="store_true")
    parser.add_argument("--fall-damage", action="store_true")
    parser.add_argument("--weapon-stay", action="store_true")
    parser.add_argument("--no-flashlight", action="store_true")
    parser.add_argument("--allow-monsters", action="store_true", help="used by DeathMatch/TDM; Co-op always permits campaign monsters")
    parser.add_argument("--allow-spectators", action="store_true")
    parser.add_argument("--internet", action="store_true", help="disable LAN-only mode; LAN is the safe default")
    parser.add_argument("--dry-run", action="store_true", help="validate the install and print the exact command")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = make_parser()
    args = parser.parse_args(argv)
    try:
        command, working_directory = build_command(
            hlds_exe=args.hlds,
            mod_dir=args.mod_dir,
            map_name=args.map_name,
            mode=args.mode,
            max_players=args.max_players,
            port=args.port,
            hostname=args.hostname,
            password=args.password,
            lan=not args.internet,
            time_limit_minutes=args.time_limit,
            frag_limit=args.frag_limit,
            friendly_fire=args.friendly_fire,
            fall_damage=args.fall_damage,
            weapon_stay=args.weapon_stay,
            flashlight=not args.no_flashlight,
            allow_monsters=args.allow_monsters,
            allow_spectators=args.allow_spectators,
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.dry_run:
        print(json.dumps({"cwd": str(working_directory), "argv": command}, indent=2))
        return 0
    return run_server(command, working_directory)


if __name__ == "__main__":
    raise SystemExit(main())
