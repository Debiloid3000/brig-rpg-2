# BRIG RPG 2 — Integration Contract

## First-person view controls

The first-person presentation controls belong to the **client DLL**. They are local user preferences and must not be synchronized through server settings, map configuration, Co-op/DeathMatch rules, or shared gameplay state.

### Ownership and behavior

| Setting | Owner / effect |
| --- | --- |
| `default_fov` | Client HUD FOV default; the server's `SetFOV` message can temporarily override it for zoom, and `SetFOV 0` restores the current default. |
| `viewmodel_fov` | Client first-person entity only. GoldSrc's shared scene projection is not replaced; the value is approximated by scaling the viewmodel's geometry in the client Studio renderer. |
| `viewmodel_offset_x` | Moves the first-person entity along its camera-relative right axis. |
| `viewmodel_offset_y` | Moves the first-person entity along its camera-relative forward axis. |
| `viewmodel_offset_z` | Moves the first-person entity along its camera-relative up axis. |
| `cl_righthand` | Archived local preference: `1` is right-hand/default; `0` mirrors the first-person model hierarchy for left-hand presentation. |

The "viewmodel" here means any Studio model rendered as the local first-person presentation entity, not just weapon models: it can include arms, held items, tools, props, and other first-person models. The settings are selected by the engine's `GetViewModel()` entity identity, not by a weapon class or model filename.

The offset code modifies the engine's viewmodel entity origin, not `ref_params_s::vieworg`, the player's predicted origin, collision, or server coordinates. The renderer applies the FOV-like scale and handedness only when the current Studio entity is `gEngfuncs.GetViewModel()`. Player entities and ordinary world studio entities must remain unaffected.

Hand reflection is performed on root-bone transforms so child bones inherit it with their animation. It does not replace model assets, edit model files, or change the local player's character model. Both hardware and software Studio transform paths should be checked in-game; the exact visual result remains dependent on the viewmodel asset's rig and authored orientation.

### Commands

- `viewmodel_presetpos 1|2|3`: applies four archived viewmodel CVAR values.
- `switchhands`: toggles `cl_righthand`.
- `switchhandsleft`: sets `cl_righthand 0`.
- `switchhandsright`: sets `cl_righthand 1`.

A preset is only a starting layout. Users can subsequently edit each CVAR independently; the individual CVARs, rather than a hidden preset state, are authoritative and archived.

### Required validation

| Area | Validation |
| --- | --- |
| Registration | Confirm all CVARs and commands appear in the in-game console after client HUD initialization. |
| Persistence | Change each archived CVAR, restart the game, and confirm the value remains. |
| Presets | Test arguments `1`, `2`, `3`, missing arguments, extra arguments, and malformed text. Confirm all four values change for valid presets only. |
| Offsets | Confirm X/Y/Z move only the first-person entity and never move the camera, collision, or remote player models. Test limits and all standard first-person model types. |
| Viewmodel FOV | Test values 54 and 68 and intermediate values. Confirm world FOV and scoped/zoom FOV remain unchanged. Inspect animations and clipping. |
| Handedness | Test both hand states with every available first-person model, animated sequences, weapon/item changes, spectator in-eye view, and both renderer modes where supported. Confirm remote player models are unchanged. |
| Default FOV | Test normal play, zoom in/out, changing `default_fov` during play, map changes, demo playback/recording, spectator, and intermission. Confirm sensitivity restores when the default FOV is active. |
| Build | Build `Release | Win32` with the repository's Visual Studio solution and run the repository tests. |

The Windows x86 CI workflow is the automated build authority. A successful DLL build does not replace a Steam Legacy runtime check; runtime-only cases must be reported separately when that environment is unavailable.


## Main menu and session navigation

The engine-owned main menu remains in `resource/gamemenu.res`. BRIG's navigation pages are VGUI1 panels hosted by `TeamFortressViewport`; `cl_dll/menu.cpp` remains the server-driven HUD `ShowMenu` component. No engine executable or engine DLL is replaced.

The GameMenu Play and Multiplayer items invoke client-DLL commands `brig_menu_play` and `brig_menu_multiplayer`. Standard Resume, Disconnect, Save/Load, Options and Quit callbacks remain intact. Save/Load continue to use the engine's `notmulti` filter.

The menu consumes shared data rather than maintaining parallel lists:
- `campaign_catalog.h` derives campaign groups from Map Catalog records.
- `game_mode_registry.h` shares the same `GameMode` enum used by the game-rules feature registry and reports launch readiness.
- `session_config.h` owns one client-side `SessionConfiguration` object; menu pages read it, and future settings controls must update it rather than create another copy.
- Server Browser, Lobby, Create Lobby and Connect have real navigation routes but report their unimplemented network status until those subsystems are built.

The Singleplayer VGUI path now navigates category → campaign → chapter/map, exposes a campaign-wide map list and keeps Training courses separate. Before launch, the client reloads the same central catalog and accepts only a safe map ID that resolves to an existing GoldSrc BSP v30, has Singleplayer enabled, belongs to a gameplay-ready mode, and is not tagged as an unsupported Blue Shift/Opposing Force expansion. The launch helper queues the selected GoldSrc difficulty (`skill 1`, `skill 2`, or `skill 3`) and the cheats preference before the map command. This command sequence and server-side result remain subject to manual Steam Legacy runtime validation. Create Game editing, editable multiplayer server settings and network actions remain later stages.

## Unified map catalog

The repository source registry is `_folder/brig/maps/brig_maplist.txt`; per-map metadata lives beside the BSPs in `_folder/brig/maps/<mapname>.cfg`. At runtime they resolve to `maps/brig_maplist.txt` and `maps/<mapname>.cfg` inside the installed mod. Singleplayer, Create Game, Co-op, DeathMatch, Team DeathMatch, lobby, and map rotation must all consume this same catalog. Do not maintain a second BSP list in any UI or game-mode subsystem.

The server-side catalog implementation is shared through `game_shared/map_catalog.h` and exposed through the game DLL bridge:
- `MapCatalog_Refresh()`
- `MapCatalog_GetAvailableMapCount()`
- `MapCatalog_GetAvailableMapName(index)`
- `MapCatalog_IsAllowedForMultiplayer(mapName, teamDeathmatch)`
- `MapCatalog_Print()`

The debug/administration commands `brig_mapcatalog_reload` and `brig_mapcatalog_list` refresh and inspect that same list. The existing `sv_load_all_maps` diagnostic now uses the registered catalog and preserves manifest order. The engine filesystem's `FindFirst/FindNext` BSP enumeration is only a compatibility fallback when `maps/brig_maplist.txt` is missing or unreadable.

The future client menu should read the same manifest through its initialized Legacy-compatible `IFileSystem`, then use the shared parser from `game_shared/map_catalog.h` for IDs and per-map CFG metadata. It must not invent a separate campaign list or assume that an installed BSP implies support for a campaign's entities/resources.

### Map/session ownership

- **Map catalog** owns map registration and map metadata, not session choices.
- **Map CFG** owns map-specific presentation compatibility and the map's declared mode availability/default.
- **Session/Create Game/Lobby** owns the user's selected mode and session options.
- **GameRules** remains the authority for actual gameplay behavior; map metadata is validated against the selected mode before starting the map.
- **Menu/UI** reads catalog metadata and writes session configuration; it does not directly mutate entities or gameplay state.

### Map rotation

The existing `mapcycle.txt` remains the server operator's ordering and player-count filter, not a second map database. Before a selected entry is accepted, the game checks it against the central catalog and the active DeathMatch or Team DeathMatch availability flag. Unsupported/stale entries are skipped; if player-count filters exclude all otherwise valid entries, the first catalog-approved cycle entry is used. If the cycle has no valid entry, a catalog-approved map for the active mode is selected; if none exists, the server stays on the current map and logs a warning.


The engine GameUI overrides are narrowly SHA-256 allowlisted in `tools/validate_brig.py`. Only the exact reviewed versions of `resource/gamemenu.res`, `resource/gameui_english.txt` and `resource/gameui_russian.txt` are accepted; the integrity checks for all other audited resources remain unchanged.

## Campaign catalog and training separation

`game_shared/campaign_catalog.h` derives campaigns and chapters exclusively from Map Catalog records. Story categories are selected through the catalog metadata (`half_life_1`, `blue_shift`, `opposing_force`, and `custom`). Chapter order comes from `brig_chapter_order`; map order inside a chapter comes from `brig_map_order`. The first sorted map of each chapter is its chapter entry point.

`brig_campaign_start` identifies the first map of the whole campaign. It is not a per-chapter flag. Training metadata (`brig_category "training"`, including the legacy `hazard_course` ID) is excluded from story campaigns and surfaced separately through the training-course helpers.

The category filter is metadata filtering only. It must not make an unsupported expansion or gameplay mode appear playable. Every launch path must additionally check the content audit and `BrigGameModeRegistry::IsGameplayReady`.

See [BRIG-CAMPAIGNS.md](BRIG-CAMPAIGNS.md) for the installed-content audit and explicit implementation boundaries.
