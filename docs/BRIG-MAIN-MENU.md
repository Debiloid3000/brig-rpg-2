## Main menu and navigation

The engine-owned main menu is described by `_folder/brig/resource/gamemenu.res`. The client HUD file `cl_dll/menu.cpp` is only the server-driven `ShowMenu` HUD element and is not the main menu.

- The GameUI resource keeps standard Resume, Disconnect, Save/Load, Options and Quit commands.
- Save and Load retain the engine's `notmulti` filter; Save additionally remains available only while in-game.
- The Play and Multiplayer rows invoke `engine brig_menu_play` and `engine brig_menu_multiplayer`. Those commands are registered by the client DLL and open VGUI1 pages inside the existing `TeamFortressViewport`.
- VGUI1 owns the BRIG page stack, mouse interaction, arrow-key selection, Enter activation and Escape/Back navigation. It does not replace or patch `hl.exe`.
- Play contains Singleplayer, Create Game and Game. Multiplayer contains Server Browser, Lobby, Create Lobby and Connect to Server. Each navigation route is implemented; pages for unfinished gameplay/network subsystems report their status instead of pretending to launch them.
- Singleplayer uses `maps/brig_maplist.txt` and per-map CFGs through `game_shared/map_catalog.h`; `game_shared/campaign_catalog.h` derives categories, ordered campaigns/chapters and Training courses. The VGUI flow supports campaign selection, chapter selection, the all-campaign-maps view and paged map selection.
- `game_shared/game_mode_registry.h` is the shared readiness registry. Co-op map eligibility does not mark Co-op gameplay complete.
- `game_shared/session_config.h` exposes one client-side `SessionConfiguration` object. The Singleplayer Game Settings page updates difficulty (GoldSrc skill 1–3) and the cheats preference on this object; future Create Game and Server Settings controls must reuse it.

The Singleplayer campaign browser and map-launch path are implemented in code. Before a map command is sent, the client re-reads the catalog and verifies the BSP/header, map identifier, Singleplayer metadata and mode/expansion readiness. Difficulty and the cheats preference are queued before the map command; actual server-side application is not claimed until checked in Steam Legacy. Create Game editing, editable multiplayer server settings, server browser and lobby/network implementation remain unfinished. Steam Legacy mouse, keyboard, menu layering, launch, difficulty, cheats, menu return and save/restore still require manual runtime verification.



The three engine-owned GameUI resource overrides are checksum-locked in `tools/validate_brig.py`: `resource/gamemenu.res`, `resource/gameui_english.txt` and `resource/gameui_russian.txt`. CI accepts only the exact approved SHA-256 values for these files; other changes still fail the asset audit. The original preservation manifest is left intact.
