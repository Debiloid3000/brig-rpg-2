# BRIG RPG 2 — Game Modes and Feature Porting

**Target:** Half-Life Steam Legacy, Windows x86.  
**Status:** Singleplayer, DeathMatch, Team DeathMatch and the Co-op listen-server launch path are wired. Co-op uses the stock GoldSrc changelevel pipeline with a catalog guard and dedicated-server-safe player-in-volume lookup for scripted activators. Steam Legacy multi-client runtime verification is still required.

## Compatibility contract

- Keep the original Steam Legacy engine/runtime. Do not ship or replace `hl.exe`, engine DLLs, renderer, or platform binaries.
- Build BRIG's own client and server game DLLs against the existing Legacy SDK boundary.
- Keep classic GoldSrc player movement, bhop behavior, camera, HUD and viewmodel behavior. Do not import Anniversary presentation or movement changes.
- Preserve map entity classnames, keyvalues, save fields and existing BSP behavior unless a separately tested compatibility fix requires otherwise.
- A game-code change is not an engine update. Never describe this work as updating the complete Legacy runtime.

## Current BRIG baseline

BRIG already has `CGameRules`, `CHalfLifeRules`, `CHalfLifeMultiplay` and `CHalfLifeTeamplay`. It also exposes `IsCoOp()`, `IsDeathmatch()` and `IsTeamplay()`.

The mode registry exposes the modes whose existing Legacy rules and launch path are wired into Create Game:

1. Singleplayer uses CHalfLifeRules.
2. Deathmatch uses the original CHalfLifeMultiplay rules.
3. Team Deathmatch uses the original CHalfLifeTeamplay rules selected through mp_teamplay.
4. Co-op uses the dedicated CHalfLifeCoOp rules class and can be selected/launched from Create Game on maps whose installed catalog metadata enables Co-op. This is an implemented gameplay path, not a claim that all NPC/scripted maps have passed runtime testing.

The generic CHalfLifeMultiplay::IsCoOp() reads gpGlobals->coop, while its generic IsDeathmatch() returns true unconditionally. The dedicated CHalfLifeCoOp class reports Co-op and explicitly reports that it is not Deathmatch; the common resolver checks Co-op first. InstallGameRules() checks gpGlobals->coop before the Singleplayer/Deathmatch branch.

Singleplayer, Co-op, Deathmatch and Team Deathmatch are selectable in Create Game. The UI lists only maps whose installed BSP and map metadata pass the shared catalog/readiness validation. Blue Shift and Opposing Force remain unavailable in every mode.

## Central mode feature registry

Implemented in `dlls/gamemode_features.h`:

| Mode | Multiplayer | Player respawn | Friendly player relationships | Player-vs-player damage | Barnacle input lock |
|---|---:|---:|---:|---:|---:|
| Singleplayer | No | Yes | No | No | No |
| Co-op | Yes | Yes | Yes | No | Yes |
| Deathmatch | Yes | Yes | No | Yes | No |
| Team Deathmatch | Yes | Yes | Yes | Yes | No |

The resolver deliberately checks IsCoOp() first. All four supported modes pass the shared readiness gate. Deathmatch reuses CHalfLifeMultiplay; Team Deathmatch reuses CHalfLifeTeamplay and the existing mp_teamplay server cvar. Co-op uses CHalfLifeCoOp, does not report itself as Deathmatch, and is launched with deathmatch 0 / coop 1 / mp_teamplay 0. Every mode's map list is restricted by installed BSP and catalog metadata.

### Implemented Co-op-specific behavior

- `InstallGameRules()` selects `CHalfLifeCoOp` whenever the engine's `coop` global is enabled, before the Singleplayer and Deathmatch branches.
- `CHalfLifeCoOp` is multiplayer for connection/HUD/respawn purposes, but is not Deathmatch. It uses the normal Singleplayer skill values, always allows campaign monsters, follows `mp_flashlight` for the flashlight, and awards zero frag points for player kills.
- Damage from another player is rejected by default; `mp_friendlyfire` can enable it. Player-to-player relationship queries return `GR_TEAMMATE`.
- Co-op does not run inherited DeathMatch time/frag-limit intermissions or the DeathMatch mapcycle. It forces `mp_timelimit 0`, `mp_fraglimit 0` and `mp_allowmonsters 1` at launch. Story progression is controlled by map-authored `trigger_changelevel` entities.
- Co-op spawn selection cycles through `info_player_coop` points first and checks that a point is enabled and not occupied by another player. It then tries unoccupied `info_player_start` points; if every usable point is occupied, it falls back to the first usable point without killing another player. A named level-transition start spot keeps priority over ordinary spawn points.
- When a Co-op player is captured by a Barnacle and has `PFLAG_ONBARNACLE`, BRIG skips item pre-frame and post-frame processing so the captured player cannot use their held item. Existing release handling clears the flag when the Barnacle is killed; player spawn also resets the physics flags.
- Co-op death does not create a temporary weapon box or strip the player's inventory. Weapon entity links and ammo remain intact, and the active weapon is deployed again on respawn.
- Co-op spawn preserves ammo after the first spawn. The first spawn still initializes an empty inventory before map equipment/default items are granted.
- A restored Co-op player is marked as having initialized inventory after the existing Legacy PLAYER save fields have been read; no existing save-field layout was changed.

Co-op death/respawn keeps the player's weapon entities and ammunition attached to the player instead of packing them into a temporary weapon box or clearing them. The previous active weapon is deployed again after respawn, and the normal map/default loadout is granted only when the player has no retained weapons. The existing Legacy PLAYER save table already serializes weapon pointers and ammo; a successful Co-op restore marks the runtime inventory as initialized without changing the save-field layout. Runtime verification is still required for death/respawn, map transitions, disconnect/reconnect, and scripted AI.

### Implemented general Half-Life bug fix

`CBaseMonster::BarnacleVictimReleased()` now restores the idle ideal state only if the victim has not been marked killed, is not dead and still has positive health. This prevents the Barnacle release path from resetting a monster's state after it died while being lifted. It is the small, self-contained Half-Life Updated fix from `dlls/monsters.cpp`; it does not change the Barnacle entity's classname, keyvalues, model, timing or save fields.

## Porting matrix

| Feature / subsystem | Source | Intended mode | Legacy assessment | State |
|---|---|---|---|---|
| Barnacle victim resurrection fix | Half-Life Updated, `dlls/monsters.cpp` | All modes | Game-DLL-only; no engine ABI change | **Applied** |
| Prevent captured Co-op player item input | BRIG mode registry + existing `PFLAG_ONBARNACLE` | Co-op only | Game-DLL-only; release flag remains the existing mechanism | **Applied; needs runtime test** |
| Original Barnacle entity logic | BRIG `dlls/barnacle.cpp` | Singleplayer baseline; shared NPC | BRIG's file is byte-identical to Half-Life Updated at the inspected revision | **Kept; no unnecessary replacement** |
| Co-op spawn-point selection (`info_player_coop`) | BRIG Co-op spawn selector over existing Legacy entities | Co-op | Checks triggered and occupied points; prefers `info_player_coop`, then `info_player_start`; uses a usable occupied fallback without killing another player; named transition start spot wins | **Applied; needs runtime test** |
| Co-op friendly fire and player relationships | BRIG `CHalfLifeCoOp` rules | Co-op | Game-DLL rule overrides; no engine API change | **Applied; needs runtime test** |
| Co-op respawn and inventory persistence | BRIG Co-op player rules + existing Legacy player save/restore fields | Co-op | Immediate respawn is inherited from multiplayer rules; no new save fields are added | **Death/respawn implemented; map-transition/runtime validation pending** |
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
| Player spawn/respawn and relocation | Co-op-specific | Implemented with Legacy spawn selection; verify across real multi-client transitions |
| Player-to-player damage policy | Co-op-specific | Implemented in Co-op gamerules; controlled by `mp_friendlyfire` |
| Scripted map entities and environmental interactions | General Half-Life / map-dependent | Preserve existing entity semantics; extend only for a documented map use case |
| Weapon/item respawn and scoring | Deathmatch / Team Deathmatch | Keep separate from Co-op and Singleplayer |
| Sven Co-op AngelScript hooks, server commands and plugin APIs | Requires another runtime | Not portable without implementing a separate scripting/runtime layer; do not fake those APIs |
| Sven-specific network, player movement and engine hooks | Engine/runtime-dependent | Excluded unless a Legacy game-DLL-only equivalent is proven |

## Co-op campaign map transitions

Co-op deliberately uses the original GoldSrc trigger_changelevel / CHANGE_LEVEL mechanism rather than a BRIG-specific map sequencer:

- Singleplayer is unchanged: the trigger checks the Legacy local player, resolves its landmark and calls CHANGE_LEVEL as before.
- In Co-op, the player who touches the trigger is preferred for the transition-volume check. If a map entity activates the trigger (or the player activator is outside the volume), the server searches connected clients and uses the first player inside the matching `trigger_transition` volume. This works without a listen-server local player.
- Before changing maps, Co-op checks the destination against the server's shared map catalog. The target must have an installed GoldSrc BSP and a CFG declaring brig_coop 1.
- A valid trigger calls the same CHANGE_LEVEL(map, landmark) engine entry point. The server owns the transition; clients do not issue independent map commands. The engine's standard landmark/player transfer path remains responsible for carrying connected players into the new server level.
- A first valid activation initiates the whole server transition; this is not an all-players-ready barrier. Mappers remain responsible for the map and landmark keys and any trigger_transition volumes.
- Co-op does not let inherited DeathMatch time/frag limits take the team into a mapcycle. Campaign progression is map-trigger-driven.

The code path is implemented, but it must still be tested in Steam Legacy with two or more clients and with a dedicated server. In particular, verify the loading overlay on every client, player placement at the next landmark/start points, inventory/ammo preservation, and NPC/scripted sequence continuity. Do not mark those runtime cases passed based on CI alone.

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

This branch has not been tested inside a running Steam Legacy game in this change. In particular, verify: Singleplayer Barnacle capture remains unchanged; Co-op rules are selected when `coop` is enabled even if `deathmatch` is zero; player-vs-player damage is blocked; monsters and flashlights remain enabled; players are reported as allies; `info_player_coop` is preferred and maps without it fall back to `info_player_start`; captured players cannot use held items; respawn clears the Barnacle flag; inventory and map transitions do not lose or duplicate items; NPCs and scripted map events behave correctly. Do not mark the full Co-op mode complete until these tests pass inside Steam Legacy and the remaining inventory/AI work is finished.


## Map catalog dependency

The future menu and session prompts must use the single registry and map-local metadata contract in [BRIG-MAP-CATALOG.md](BRIG-MAP-CATALOG.md). The map's declared available modes are content metadata; they do not create or complete the corresponding GameRules implementation by themselves.
