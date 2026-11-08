## Main menu and navigation

The engine-owned main menu is described by `_folder/brig/resource/gamemenu.res`. The client HUD file `cl_dll/menu.cpp` is only the server-driven `ShowMenu` HUD element and is not the main menu.

- The GameUI resource keeps standard Resume, Disconnect, Save/Load, Options and Quit commands.
- Save and Load retain the engine's `notmulti` filter; Save additionally remains available only while in-game.
- The Play and Multiplayer rows invoke `engine brig_menu_play` and `engine brig_menu_multiplayer`. Those commands are registered by the client DLL and open VGUI1 pages inside the existing `TeamFortressViewport`.
- VGUI1 owns the BRIG page stack, mouse interaction, arrow-key selection, Enter activation and Escape/Back navigation. It does not replace or patch `hl.exe`.
- Play contains Singleplayer, Create Game and Game. Multiplayer contains Server Browser, Lobby, Create Lobby and Connect to Server. Each navigation route is implemented; pages for unfinished gameplay/network subsystems report their status instead of pretending to launch them.
- Singleplayer uses `maps/brig_maplist.txt` and per-map CFGs through `game_shared/map_catalog.h`; `game_shared/campaign_catalog.h` derives categories, ordered campaigns/chapters and Training courses. The VGUI flow supports campaign selection, chapter selection, the all-campaign-maps view and paged map selection.
- game_shared/game_mode_registry.h is the shared readiness registry. Singleplayer, Deathmatch and Team Deathmatch are marked ready for menu launch; Co-op map eligibility does not mark Co-op gameplay complete.
- game_shared/session_config.h exposes one client-side SessionConfiguration object. Singleplayer Game Settings and the Create Game page both read and update this same object.
- Create Game cycles Singleplayer, Deathmatch and Team Deathmatch, selects only installed maps whose BSP and map metadata pass validation, and exposes max-player and LAN controls for listen-server modes. Hostname/password are applied from the shared configuration; editing those two text values in the menu is not added yet. Dedicated-server process management, server browser and lobby/network services remain separate unfinished systems.

The client launch path validates map metadata/readiness before issuing commands. Singleplayer clears stale multiplayer mode cvars before starting the selected map. Deathmatch and Team Deathmatch set the Legacy deathmatch, coop and mp_teamplay cvars before map startup; max-player, hostname, password, LAN, difficulty and cheats values come from the shared session configuration. This command sequence still needs manual Steam Legacy verification; a successful DLL build does not prove that a listen-server session works in-game.


The three engine-owned GameUI resource overrides are checksum-locked in `tools/validate_brig.py`: `resource/gamemenu.res`, `resource/gameui_english.txt` and `resource/gameui_russian.txt`. CI accepts only the exact approved SHA-256 values for these files; other changes still fail the asset audit. The original preservation manifest is left intact.
