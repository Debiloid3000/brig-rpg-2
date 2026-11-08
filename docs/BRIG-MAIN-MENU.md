## Main menu and navigation

The engine-owned main menu is described by `_folder/brig/resource/gamemenu.res`. The client HUD file `cl_dll/menu.cpp` is only the server-driven `ShowMenu` HUD element and is not the main menu.

- The GameUI resource keeps standard Resume, Disconnect, Save/Load, Options and Quit commands.
- Save and Load retain the engine's `notmulti` filter; Save additionally remains available only while in-game.
- The Play and Multiplayer rows invoke `engine brig_menu_play` and `engine brig_menu_multiplayer`. Those commands are registered by the client DLL and open VGUI1 pages inside the existing `TeamFortressViewport`.
- VGUI1 owns the BRIG page stack, mouse interaction, arrow-key selection, Enter activation and Escape/Back navigation. It does not replace or patch `hl.exe`.
- Play contains Singleplayer, Create Game and Game. Multiplayer contains Server Browser, Lobby, Create Lobby and Connect to Server. Each navigation route is implemented; pages for unfinished gameplay/network subsystems report their status instead of pretending to launch them.
- The Singleplayer summary is generated from `maps/brig_maplist.txt` and its per-map CFGs through `game_shared/map_catalog.h`. `game_shared/campaign_catalog.h` derives campaign groups from those map records and owns no second list.
- `game_shared/game_mode_registry.h` is the shared readiness registry. Co-op map eligibility does not mark Co-op gameplay complete.
- `game_shared/session_config.h` exposes one client-side `SessionConfiguration` object. The Game page reads that object; future Create Game and Server Settings controls must update this same object instead of introducing parallel settings.

The branch provides a menu foundation, not the later campaign launcher, Create Game editor, editable server settings, server browser, or lobby/network implementation. These remain explicit next-stage integrations. Steam Legacy in-game mouse, keyboard and GameUI layering still require manual runtime verification after the Win32 client DLL is built.

