# BRIG RPG 2 — Campaign Catalog

**Target:** Half-Life Steam Legacy, Windows x86.  
**Status:** catalog model and read-only menu listing are implemented; chapter launch, the all-maps toggle, and expansion gameplay are not yet implemented.

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

A category appears only when matching map records exist in the installed catalog. Empty Blue Shift or Opposing Force categories must not be populated with guessed BSP names.

## Chapters and start maps

Each campaign is grouped by `brig_campaign`. Chapters are grouped by `brig_chapter` and sorted by `brig_chapter_order`. Maps inside a chapter are sorted by `brig_map_order`, then by map identifier as a deterministic tie-breaker.

The first map in each sorted chapter is its chapter entry point. `brig_campaign_start 1` has a different meaning: it marks the entry point of the entire campaign and must not be used to decide the start map of every chapter.

The regular chapter view should show chapter titles from `brig_chapter_title`; it must not substitute BSP identifiers for chapter names. The optional all-maps view should use the same catalog records and preserve their catalog order.

## Training is separate

Maps whose category is `training` (or whose campaign ID is the legacy `hazard_course`) are excluded from the story-campaign groups. They are exposed through `BuildTrainingCourses` / `BuildTrainingMaps` and must be presented under a separate Training section.

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

The Singleplayer menu currently renders campaign/chapter data and a separate Training listing as read-only text. It does not yet provide interactive category tabs, chapter selection, an all-maps toggle, difficulty selection, a cheats control, or a launch action. No button should claim to launch a map until the client-to-server session handoff and launch path are implemented and tested.

The following are explicitly not complete:

1. Interactive category tabs and chapter/map selection.
2. Selecting and launching a chapter's start map or an individual map in all-maps mode.
3. A difficulty setting wired to the actual game rules.
4. A pre-map `sv_cheats` policy for Singleplayer.
5. Save/restore and menu-return runtime verification.
6. Blue Shift and Opposing Force gameplay support.

For changes to the schema, update the shared parser, its tests, this document, and `docs/BRIG-INTEGRATION-CONTRACT.md` together.
