# BRIG RPG 2 — Game Modes and Feature Porting

**Target:** Half-Life Steam Legacy, Windows x86.  
**Status:** foundation only; this document distinguishes implemented work from candidates and deferred systems.

## Compatibility contract

- Keep the original Steam Legacy engine/runtime. Do not ship or replace `hl.exe`, engine DLLs, renderer, or platform binaries.
- Build BRIG's own client and server game DLLs against the existing Legacy SDK boundary.
- Keep classic GoldSrc player movement, bhop behavior, camera, HUD and viewmodel behavior. Do not import Anniversary presentation or movement changes.
- Preserve map entity classnames, keyvalues, save fields and existing BSP behavior unless a separately tested compatibility fix requires otherwise.
- A game-code change is not an engine update. Never describe this work as updating the complete Legacy runtime.

## Current BRIG baseline

BRIG already has `CGameRules`, `CHalfLifeRules`, `CHalfLifeMultiplay` and `CHalfLifeTeamplay`. It also exposes `IsCoOp()`, `IsDeathmatch()` and `IsTeamplay()`.

Two important facts affect future work:

1. `CHalfLifeMultiplay::IsCoOp()` reads `gpGlobals->coop`, while `CHalfLifeMultiplay::IsDeathmatch()` currently returns `true` unconditionally. Any common mode resolver must test Co-op before Deathmatch.
2. `InstallGameRules()` currently chooses Singleplayer when `gpGlobals->deathmatch == 0`; otherwise it chooses Teamplay or the generic Multiplayer rules. There is no dedicated Co-op rules class yet. A future complete Co-op implementation must address mode selection, friendly-fire policy, spawn selection, NPC allowance and respawn together rather than relying on the Co-op predicate alone.

## Central mode feature registry

Implemented in `dlls/gamemode_features.h`:

| Mode | Multiplayer | Player respawn | Friendly player relationships | Player-vs-player damage | Barnacle input lock |
|---|---:|---:|---:|---:|---:|
| Singleplayer | No | Yes | No | No | No |
| Co-op | Yes | Yes | Yes | No | Yes |
| Deathmatch | Yes | Yes | No | Yes | No |
| Team Deathmatch | Yes | Yes | Yes | Yes | No |

The resolver deliberately checks `IsCoOp()` first. The table is the shared feature contract for mode-specific code; it does **not** claim that every row is fully implemented by the existing game rules. At present, the Barnacle input lock is the only new gameplay behavior wired to this registry. The current game-rules hierarchy still needs a dedicated, fully tested Co-op implementation before the other Co-op entries can be treated as active rules.

### Implemented Co-op-specific behavior

When the active game-rules object reports Co-op and the player has `PFLAG_ONBARNACLE`, BRIG skips the player's item pre-frame and post-frame processing. This prevents the captured player from using their held item against the Barnacle. It does not alter Singleplayer, Deathmatch or Team Deathmatch. Existing release handling clears the flag when the Barnacle is killed; player spawn also resets the physics flags.

### Implemented general Half-Life bug fix

`CBaseMonster::BarnacleVictimReleased()` now restores the idle ideal state only if the victim has not been marked killed, is not dead and still has positive health. This prevents the Barnacle release path from resetting a monster's state after it died while being lifted. It is the small, self-contained Half-Life Updated fix from `dlls/monsters.cpp`; it does not change the Barnacle entity's classname, keyvalues, model, timing or save fields.

## Porting matrix

| Feature / subsystem | Source | Intended mode | Legacy assessment | State |
|---|---|---|---|---|
| Barnacle victim resurrection fix | Half-Life Updated, `dlls/monsters.cpp` | All modes | Game-DLL-only; no engine ABI change | **Applied** |
| Prevent captured Co-op player item input | BRIG mode registry + existing `PFLAG_ONBARNACLE` | Co-op only | Game-DLL-only; release flag remains the existing mechanism | **Applied; needs runtime test** |
| Original Barnacle entity logic | BRIG `dlls/barnacle.cpp` | Singleplayer baseline; shared NPC | BRIG's file is byte-identical to Half-Life Updated at the inspected revision | **Kept; no unnecessary replacement** |
| Co-op spawn-point selection (`info_player_coop`) | Half-Life Updated / Unified SDK | Co-op | Game-DLL implementation is possible; must keep fallback behavior for old maps | Deferred |
| Co-op friendly fire and player relationships | Unified SDK Co-op rules as behavioral reference | Co-op | Game-DLL implementation is possible | Deferred; no dedicated Co-op rules class |
| Co-op respawn and inventory persistence | Unified SDK Co-op rules / persistent inventory | Co-op | Respawn can be game-DLL logic; persistent map-transition inventory needs careful save/transition design | Deferred |
| NPC classifications, allied behavior and scripted AI | Unified SDK / Sven Co-op documentation | Co-op where appropriate; otherwise all modes | Individual NPC/AI changes may be portable; blanket replacement risks changing map scripts | Audit per entity |
| Weapon, item and ammo respawn | Existing BRIG multiplayer rules; Unified SDK configurable rules | DM/TDM; selected Co-op maps only | Game-DLL logic | Existing multiplayer baseline; not generalized yet |
| Team relationships and friendly fire | BRIG `CHalfLifeTeamplay`; Unified SDK teamplay | TDM | Game-DLL logic | Existing TDM baseline; keep separate from Co-op |
| Save/restore fields and serialization | Valve/BRIG `CSave` and `CRestore`; Unified SDK save systems | Primarily SP; selected session state later | Legacy save compatibility is sensitive to field layout and save version | No bulk port |
| Network messages and client/server API | Legacy engine callbacks and BRIG user messages | Multiplayer modes | Only existing Legacy callbacks/messages are safe assumptions; new engine protocols are not drop-in | No new protocol imported |
| PM movement and collision changes | Half-Life Updated `pm_shared.cpp` | All modes | Technically game-code-side, but changes movement feel and collision response | **Intentionally not ported** |
| Anniversary HUD/menu/camera/viewmodel presentation | Anniversary runtime | All modes | Not wanted; not part of this project | **Excluded** |
| Sven Co-op server-only scripting/API | Sven Co-op Wiki | Co-op | Depends on Sven Co-op runtime/AngelScript APIs not present in BRIG | **Not portable as-is** |

## Movement and presentation exclusions

Half-Life Updated changes the movement collision reflection path in `pm_shared/pm_shared.cpp` (it restricts the special reflection case to the first impact plane). That is a physics/movement behavior change, so it is intentionally not imported into BRIG under this task. The current BRIG movement code remains the baseline. Do not modify bunnyhopping, air movement, camera bob/roll/tilt, or viewmodel offsets as part of future game-mode work.

Also excluded: Anniversary main menu, HUD scale/layout, renderer-dependent presentation, camera sway, viewmodel sway, and Anniversary-only engine features.

## Sven Co-op feature triage

The supplied Sven Co-op source link is the official wiki rather than a game-code repository. Therefore, Sven Co-op behavior is treated as documentation/specification, not as permission to invent its internal implementation.

| Mechanic | Category | BRIG decision |
|---|---|---|
| Barnacle victim release and multi-victim edge cases | Co-op-specific NPC behavior | Candidate for a dedicated test pass; Sven Co-op's published changelog records bugs involving respawn-trigger release, multiple Barnacles and prey references |
| NPC allies / shared enemy relationships | Co-op-specific | Candidate; must be mediated through Co-op rules, not global NPC changes |
| Player spawn/respawn and relocation | Co-op-specific | Candidate; map spawnpoint compatibility and death-state reset required |
| Player-to-player damage policy | Co-op-specific | Candidate; friendly-fire policy belongs in Co-op gamerules |
| Scripted map entities and environmental interactions | General Half-Life / map-dependent | Preserve existing entity semantics; extend only for a documented map use case |
| Weapon/item respawn and scoring | Deathmatch / Team Deathmatch | Keep separate from Co-op and Singleplayer |
| Sven Co-op AngelScript hooks, server commands and plugin APIs | Requires another runtime | Not portable without implementing a separate scripting/runtime layer; do not fake those APIs |
| Sven-specific network, player movement and engine hooks | Engine/runtime-dependent | Excluded unless a Legacy game-DLL-only equivalent is proven |

## Future integration contract: menu, map and session settings

This is an interface contract for later prompts, not a claim that the menu or map configuration system already exists.

1. **Session selection** owns the requested mode and server/session options.
2. **Map configuration** may provide a default mode and may lock a map to a compatible mode. A map must not silently force an unsupported mode.
3. **Game rules** are the only authority that gameplay code queries. Entities must ask the rules/feature registry instead of reading menu state or scattering raw `coop`, `deathmatch` and `teamplay` checks.
4. **Menu/UI** reads and writes session configuration; it must not directly mutate NPC/player state.
5. **Validation** rejects unsupported combinations before the map starts and records the resolved mode for diagnostics.
6. The mode contract must not change BSP entity definitions or require map recompilation.

Recommended future resolution order: force Singleplayer when the session is truly singleplayer; otherwise resolve an explicit server/session mode, then a map default, then the Legacy-compatible fallback. A map lock may reject an incompatible explicit request. The exact config format and cvar names should be chosen in the later menu/configuration task, not guessed here.

## Source and license notes

- BRIG's current license file is the Valve Half-Life SDK license. The inspected Half-Life Updated and Unified SDK repositories carry the same Valve SDK license text.
- This change imports one small, attributable bug-fix condition from Half-Life Updated; it does not copy source files wholesale.
- No Sven Co-op implementation source was imported. Official wiki documentation is used only as a behavioral reference.
- Any future source port must preserve the relevant license and notices, and must be reviewed before distribution.

## Build and validation

The repository CI is the validation path for this change:

- Win32 MSVC Release build of the client and server game DLLs.
- Linux-x86 compile checks and Legacy client/server API bounds tests.
- Existing Python tests, asset policy validation and packaging checks.

This branch has not been tested inside a running Steam Legacy game in this change. In particular, verify: Singleplayer Barnacle capture remains unchanged; Co-op capture blocks the caught player's item input; killing a Barnacle releases a living player; death followed by respawn clears the capture flag; an NPC killed while captured does not return to the idle state; existing BSPs without `info_player_coop` continue to load. Do not mark the full Co-op mode complete until these tests and dedicated Co-op gamerules/spawn behavior are implemented.
