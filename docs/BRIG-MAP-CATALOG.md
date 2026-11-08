# BRIG RPG 2 — Unified Map Catalog and Map CFG

**Target:** Half-Life Steam Legacy, Windows x86.  
**Implementation:** game DLL + shared parser; no engine/runtime replacement.  
**Status:** catalog/parser integrated; runtime Steam Legacy map loading still requires an in-game check with installed BSPs.

## 1. One source of truth

The repository source files live directly beside the mod's BSP assets:

```text
_folder/brig/maps/brig_maplist.txt
_folder/brig/maps/<mapname>.cfg
```

When installed, those same files are available as `maps/brig_maplist.txt` and `maps/<mapname>.cfg`, relative to the BRIG mod directory. There is no second source-level `maps/` directory.

Each non-comment manifest line is one BSP identifier, without a path or extension.

The current manifest registers **115 BSPs** already present in the audited BRIG mod directory:

- **96 Half-Life 1 campaign maps** across the 15 story chapters, declared for Singleplayer and Co-op in map metadata. The Co-op flag expresses intended map/campaign eligibility; it does not activate the game mode or certify that the campaign is playable in Co-op.
- **7 Hazard Course maps**, marked for both Singleplayer and Co-op eligibility while retaining Singleplayer as their default mode.
- **12 Half-Life Deathmatch arenas**, marked DeathMatch and Team DeathMatch.

Every registered map has its own CFG. BRIG now has an initial dedicated `CHalfLifeCoOp` game-rules class: it is selected from the engine `coop` flag, allows monsters/flashlight, blocks player-vs-player damage, treats players as allies and uses the Singleplayer skill profile. The existing spawn selector then prefers `info_player_coop` and falls back to `info_player_start`. This is still a partial rules foundation: inventory across deaths/map transitions, scripted NPC behavior and Steam Legacy runtime validation are not complete. `game_mode_registry.h` intentionally continues to mark Co-op as not gameplay-ready, so map metadata alone cannot make Hazard Course or story maps launch in Co-op from the menu. Hazard Course retains the requested Co-op eligibility flags. Blue Shift and Opposing Force are not registered as playable campaigns in this pass.

A map is returned by the runtime catalog only when its identifier is safe and the corresponding `maps/<id>.bsp` exists and has GoldSrc BSP version 30. Missing or invalid BSPs are logged and skipped; they are not offered to map selection or the `sv_load_all_maps` diagnostic.

The manifest is curated and is the authoritative registration list. If it is missing or unreadable, the server falls back to the Legacy-compatible `IFileSystem::FindFirst("maps/*.bsp")` / `FindNext` API, filters to valid BSP v30 files, and logs that fallback. A present manifest is not silently expanded with every BSP on disk.

Map IDs are restricted to 1–31 ASCII letters, digits, underscores and hyphens. They cannot contain a path, slash, extension, colon, or traversal component. Runtime file paths are constructed only after this validation.

## 2. Supported CFG fields

All settings are read as data. No map CFG line is passed to the console, `exec`, or an arbitrary command handler.

| Key | Type | Meaning |
| --- | --- | --- |
| `brig_title` | quoted string | Map display title |
| `brig_campaign` | identifier | Stable campaign ID |
| `brig_campaign_title` | quoted string | Campaign display title |
| `brig_category` | identifier | Campaign/content category |
| `brig_chapter` | identifier | Stable chapter ID |
| `brig_chapter_title` | quoted string | Chapter display title |
| `brig_chapter_order` | integer 0–9999 | Sort order of chapter |
| `brig_map_order` | integer 0–9999 | Sort order within chapter |

A map with `brig_map_order 1` is the first map in its chapter. `brig_campaign_start 1` separately marks the opening map of the whole campaign.
| `brig_campaign_start` | 0/1 | Marks the campaign's starting map |
| `brig_singleplayer` | 0/1 | Include in the Singleplayer list |
| `brig_coop` | 0/1 | Include in the Co-op list |
| `brig_deathmatch` | 0/1 | Include in the DeathMatch list |
| `brig_team_deathmatch` | 0/1 | Include in the Team DeathMatch list |
| `brig_default_mode` | mode name | Default for a mixed-mode map list |
| `forcepmodels` | model folder list | Existing map-local player model restriction |
| `globalsoundlist` | relative path | Existing sound/sentence replacement list |

Recognized mode names are `singleplayer`, `coop`, `deathmatch` and `team_deathmatch`. Short aliases `sp`, `dm` and `tdm` are also accepted by the parser. The default must be enabled by the map's flags. If it is not, the parser logs a warning and selects the first enabled mode in the order Singleplayer, Co-op, DeathMatch, Team DeathMatch.

A flag set to `0` excludes a map from that mode's list. This is the supported per-list exclusion mechanism; menus must not maintain extra hidden exclusion lists.

Recommended `brig_category` values are `half_life_1`, `blue_shift`, `opposing_force` and `custom`. Other safe identifiers can be used for future content.

## 3. Example map CFG

The source copies of the installed examples are `_folder/brig/maps/c0a0.cfg` and `_folder/brig/maps/c1a0.cfg` (installed runtime paths are `maps/c0a0.cfg` and `maps/c1a0.cfg`). The full custom-map example is `docs/examples/map-profile.cfg.example`:

```cfg
brig_title "Example Custom Campaign Map"
brig_campaign "my_campaign"
brig_campaign_title "My Custom Campaign"
brig_category "custom"
brig_chapter "arrival"
brig_chapter_title "Arrival"
brig_chapter_order 1
brig_map_order 1
brig_campaign_start 1

brig_singleplayer 1
brig_coop 1
brig_deathmatch 0
brig_team_deathmatch 0
brig_default_mode "coop"

forcepmodels "scientist,barney"
globalsoundlist "example/soundrep.txt"
```

The model folders and matching `models/player/<folder>/<folder>.mdl` files must exist. The runtime still checks model assets before accepting or precaching a model. The replacement list must exist under the mod's `sound/` namespace to have an effect.

## 4. Parser safety and limits

The shared parser is `game_shared/map_catalog.h`. It is header-only and deliberately does not depend on engine APIs, so the future client menu can parse the same manifest and map CFG format.

Limits and validation:

- Manifest: maximum 64 KiB and 512 unique map identifiers.
- Per-map CFG: maximum 16 KiB, 256 bytes per line.
- Display strings: maximum 95 bytes; control characters are rejected.
- Identifiers: ASCII letters, digits, underscores and hyphens only.
- Booleans: exactly `0` or `1`.
- Sort orders: non-negative decimal integers from 0 to 9999.
- `forcepmodels`: up to 32 safe model folder identifiers, each at most 31 characters.
- `globalsoundlist`: relative `.txt` or legacy `.gsr` path; absolute paths, drive prefixes, backslashes and arbitrary `..` traversal are rejected. For compatibility, the exact historical Sven form `../../maps/<relative-list>.txt` or `.gsr` is accepted and normalized to `maps/<relative-list>` only when the remainder contains no traversal.
- Unknown keys, arbitrary CVARs, console commands, malformed lines, duplicate keys and invalid values are ignored and logged. Valid entries elsewhere in the same file remain usable.
- Quoted values cannot contain an escaped quote or a newline. This intentionally small grammar keeps the parser deterministic and safe.

The catalog uses bounded reads through the already initialized Legacy-compatible `IFileSystem`. BSP files are opened and only their four-byte version header is read; the catalog does not load entire BSPs into memory just to check their presence.

## 5. Defaults and missing files

If a BSP is registered but its CFG is missing, its metadata defaults to:

- Display title = map identifier.
- Category = `uncategorized`.
- Singleplayer = enabled.
- Co-op, DeathMatch and Team DeathMatch = disabled.
- Default mode = Singleplayer.

This conservative fallback does not claim that a campaign or an expansion is supported. A map with no enabled mode remains registered in the catalog data, but it should not appear in any mode's selection list. A declared `brig_coop 1` is only a map-level eligibility declaration; the menu/session launch path must also verify that the selected mode is actually implemented and supported.

If `maps/brig_maplist.txt` is missing, BSP discovery is used only as a fallback. If both the list and BSP are missing, the map is not available.

## 6. Legacy profile compatibility and precedence

The existing `forcepmodels` and `globalsoundlist` worldspawn keys and server CVARs remain supported.

At map initialization:

1. Existing worldspawn keyvalues establish the legacy values.
2. The typed `maps/<mapname>.cfg` is parsed.
3. Only explicitly present `forcepmodels` and `globalsoundlist` keys override their worldspawn values. A key omitted from CFG leaves the worldspawn setting untouched.
4. If no sound list is set, the existing default `sound/<mapname>/soundrep.txt` path is used.
5. The sound replacement list is loaded and replacement files are precached before map entities start using them.

The old map-profile loader executed CFG files with `SERVER_COMMAND("exec ...")`. That was removed. Map CFG is now data-only. Existing sound-list parsing, sentence replacement behavior, player-model folder checks and precaching remain in place.

`MapProfiles_Clear()` is still called from `ServerDeactivate()`. It clears the two map-local CVARs and the active sound replacement table so one map's restrictions or replacements do not bleed into the next map.

## 7. Campaign/expansion compatibility

The metadata schema can describe Half-Life 1, Blue Shift, Opposing Force and user-created BRIG campaigns. That is metadata support, not proof that the gameplay content is implemented.

Blue Shift and Opposing Force maps are not registered as playable campaigns in this pass. Do not mark them playable merely because a BSP exists. Before adding either expansion to the same manifest, verify:

- All required BSPs, models, sprites, sounds, textures and other resources are installed.
- The required entity classnames and keyvalue behavior exist in BRIG's game DLL.
- Campaign-specific weapons, NPCs, scripts and game rules are present and work on Legacy.
- The selected mode is actually implemented and tested for those maps.
- Old maps still load without BSP edits or mandatory new entity keyvalues.

A future campaign integration prompt can add its BSP identifiers and CFGs to the same manifest after the content/code audit.

## 8. Menu, lobby and rotation integration

All consumers must use one map catalog:

- Singleplayer campaign selection.
- Create Game.
- Co-op.
- DeathMatch.
- Team DeathMatch.
- Lobby.
- Map rotation.
- Any future custom-campaign browser.

The server bridge currently exposes:

- `MapCatalog_Refresh()`
- `MapCatalog_Print()`
- `MapCatalog_GetAvailableMapCount()`
- `MapCatalog_GetAvailableMapName(index)`
- `MapCatalog_IsAllowedForMultiplayer(mapName, teamDeathmatch)`

Server console commands `brig_mapcatalog_reload` and `brig_mapcatalog_list` refresh and print the same catalog. The legacy `sv_load_all_maps` validation command now consumes the catalog rather than constructing a second independent BSP list.

The future client-side menu should read `maps/brig_maplist.txt` through the client DLL's initialized `IFileSystem` and use `game_shared/map_catalog.h` to parse identifiers and map metadata. UI code owns the selected session mode; it does not directly change game rules or entity state. Before starting a map, validate that the selected mode is enabled by that map's metadata and is actually available in the game's mode implementation.

Server map rotation is already catalog-filtered. The existing `mapcycle.txt` stays as an operator-controlled ordering/player-count filter, but each candidate is checked with `MapCatalog_IsAllowedForMultiplayer`. Entries absent from the catalog or disabled for the active DeathMatch/TDM mode are skipped. A one-entry mapcycle is supported; if all operator entries are filtered, the first enabled map from the central catalog is used. If the mode has no enabled maps, the server keeps the current map and logs a warning.

The selected mode should be carried by session/Create Game configuration, not written back into map CFG. Map CFG describes the map; session state describes how this launch plays it. The same resolved catalog record should be passed to the campaign browser, lobby, and rotation.

## 9. Legacy boundary and validation

No engine binaries, `hl.exe`, Anniversary runtime callbacks, renderer features or network protocols are changed. BSP files are read-only inputs. The current implementation only uses game-DLL filesystem access already present in the project.

Automated tests are in `tests/map_catalog_tests.cpp`. They cover safe identifiers, manifest deduplication, metadata parsing, mode validation, unsafe CFG commands/paths, sound/model-list validation, BSP version checks and missing-BSP filtering. Linux-x86 CI compiles and runs these tests with both g++ and clang++; the Win32 MSVC build also compiles the shared parser as part of the server DLL.

A successful DLL build does not substitute for in-game verification. Steam Legacy runtime checks should cover a present and missing BSP, missing map CFG, malformed CFG, worldspawn compatibility, map transitions/reset, custom model selection, sound replacement, and the same map appearing in the correct mode lists.
