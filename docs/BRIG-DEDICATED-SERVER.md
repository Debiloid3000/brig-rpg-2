# BRIG RPG 2 — Dedicated Server Launcher

The legacy MFC ServerCtrl utility in this repository is an old engine-control sample. It contains a hard-coded D:\quiver path and is not the BRIG dedicated-server launcher.

## Required folder layout

Install or prepare a Steam Legacy HLDS root with the BRIG mod folder next to hlds.exe:

```text
HLDS-root/
  hlds.exe
  brig/
    liblist.gam
    dlls/hl.dll
    maps/
      brig_maplist.txt
      c0a0.bsp
      c0a0.cfg
```

The BRIG server DLL must be built and staged into brig/dlls/hl.dll. A map must have a valid GoldSrc BSP v30 and a matching CFG that enables the selected mode.

## Start a Co-op server

From the repository root:

```powershell
py tools/brig_server.py --hlds "D:\HLDS\\hlds.exe" --mod-dir "D:\HLDS\\brig" --mode coop --map c0a0 --max-players 8 --port 27015
```

LAN-only is the default. Add --internet only when the server should accept non-LAN traffic. Add --password for a password-protected server. Use --time-limit and --frag-limit for match limits, and --friendly-fire, --fall-damage, --weapon-stay, --no-flashlight and --allow-spectators for the corresponding server rules. Use --allow-monsters for DeathMatch/TDM; Co-op always permits campaign monsters. Use --dry-run to validate paths and print the exact arguments without starting HLDS.

The launcher validates the executable/mod layout, liblist.gam, the server DLL, BSP v30, and the selected mode flag in the map CFG. It exposes the same time/frag limits, friendly fire, fall damage, weapon stay, flashlight, monster and spectator settings used by Create Game. The cvars are applied before and after the map starts so server.cfg defaults do not silently replace the selected session settings. Co-op always permits campaign monsters; the allow-monsters switch applies to DeathMatch/TDM. Hostname and password are passed as separate process arguments and console delimiters are rejected. The server runs as a separate HLDS process in its own console; Ctrl+C requests shutdown, with a process termination fallback if HLDS does not exit.

The launcher is an initial dedicated-process start/foreground-stop path. It does not yet provide a VGUI process manager, remote console, restart scheduler, Steam Lobby, or automated public-server listing. Those remain distinct from Create Game's listen-server flow and the existing LAN Server Browser.

## Supported mode mapping

| Mode | deathmatch | coop | mp_teamplay | Required map CFG flag |
|---|---:|---:|---:|---|
| Co-op | 0 | 1 | 0 | brig_coop 1 |
| Deathmatch | 1 | 0 | 0 | brig_deathmatch 1 |
| Team Deathmatch | 1 | 0 | 1 | brig_team_deathmatch 1 |

The launcher's command-line mode values are set before +map so InstallGameRules() receives the correct engine globals during map startup. Hostname, LAN visibility and password are re-applied after map startup because server configuration files may supply defaults.

The launcher intentionally does not add Blue Shift or Opposing Force maps. Their content remains unavailable in the shared map-mode registry.
