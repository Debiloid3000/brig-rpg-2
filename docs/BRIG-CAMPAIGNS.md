# BRIG RPG 2 — Campaign Catalog

**Target:** Half-Life Steam Legacy, Windows x86.  
**Status:** the catalog parser, campaign/chapter grouping, interactive selector, Training section, campaign-wide map view, map selection and validated Singleplayer launch are implemented. In-game launch/save/restore behavior still requires Steam Legacy runtime verification; Blue Shift and Opposing Force remain locked.

## Single source of truth

The campaign selector must not own another BSP list. Its data comes from:

- Source manifest: `_folder/brig/maps/brig_maplist.txt`
- Map metadata: `_folder/brig/maps/<mapname>.cfg`
- Shared parser and map eligibility: `game_shared/map_catalog.h`
- Campaign/chapter grouping: `game_shared/campaign_catalog.h`

At runtime, the files are read from the installed mod's `maps/` directory. The campaign catalog groups records; it does not register maps independently and does not prove that a game mode or expansion is playable.

## Categories

The category filter supports these metadata identifiers:

| Menu category | `brig_category` |
| --- | --- |
| All | Any registered story-campaign category |
| Half-Life 1 | `half_life_1` |
| Blue Shift | `blue_shift` |
| Opposing Force | `opposing_force` |
| Custom BRIG campaigns | `custom` |

The selector cycles through the known categories. A category with no registered, launchable content shows an unavailable/empty state; Blue Shift and Opposing Force remain explicitly locked, and no guessed BSP names are introduced.

## Chapters and start maps

Each campaign is grouped by `brig_campaign`. Chapters are grouped by `brig_chapter` and sorted by `brig_chapter_order`. Maps inside a chapter are sorted by `brig_map_order`, then by map identifier as a deterministic tie-breaker.

The first map in each sorted chapter is its chapter entry point. `brig_campaign_start 1` has a different meaning: it marks the entry point of the entire campaign and must not be used to decide the start map of every chapter.

The regular chapter view uses chapter titles from `brig_chapter_title`; it does not substitute BSP identifiers for chapter names. The campaign-wide map view uses the same catalog records and preserves their catalog order.

## Training is separate

Maps whose category is `training` (or whose campaign ID is the legacy `hazard_course`) are excluded from the story-campaign groups. They are exposed through `BuildTrainingCourses` / `BuildTrainingMaps` and are presented under a separate Training section.

The installed metadata currently registers Hazard Course as training content beginning at `t0a0`. Its continuation maps are part of the same training course, not extra numbered story chapters.

## Current installed-content audit

The current `brig_maplist.txt` registers Half-Life 1 campaign maps, Hazard Course maps, and a set of Half-Life DeathMatch maps. It does not register a Blue Shift or Opposing Force BSP campaign in the BRIG maps directory.

Some Blue Shift / Opposing Force model and sound assets exist elsewhere in the asset tree. Those assets alone are not evidence that the expansion campaigns are supported.

Blue Shift and Opposing Force must remain unavailable / incomplete until the following are audited and implemented against the current Legacy game DLL:

- Campaign BSPs and required textures, models, sprites and sounds.
- Expansion-specific entities and keyvalue behavior.
- Expansion weapons and NPC classes.
- Server-side gameplay and save/restore behavior.
- Client presentation and compatibility with the Steam Legacy engine/API.
- A full map-by-map load and gameplay test.

The campaign category filter is only a catalog filter. It must never override `BrigGameModeRegistry::IsGameplayReady` or present an unimplemented expansion/mode as playable.

## Current UI and remaining work

The Singleplayer VGUI flow is interactive: category → campaign → chapter/map. It supports a campaign-wide map view, paginated map selection, a separate Training section and chapter-entry selection. The launch path reloads the central catalog and refuses unsafe or unavailable map IDs, missing/non-GoldSrc BSP files, maps without Singleplayer enabled, unsupported game modes and locked expansions. It queues the selected GoldSrc difficulty (`skill 1`, `skill 2` or `skill 3`) and the cheats preference before issuing the map command.

The remaining Singleplayer work is validation inside the target Steam Legacy runtime:

1. Verify the queued session settings are applied by the server in the expected order.
2. Test save/load, map transitions, menu return and campaign progression in-game.
3. Keep Blue Shift and Opposing Force unavailable until their BSPs, entities, resources, weapons, NPCs and save/restore behavior are implemented and validated.

A successful DLL build and automated catalog tests do not replace those runtime checks. For schema changes, update the shared parser, its tests, this document and `docs/BRIG-INTEGRATION-CONTRACT.md` together.
