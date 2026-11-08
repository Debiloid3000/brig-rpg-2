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
- Create Game cycles Singleplayer, Deathmatch and Team Deathmatch, selects only installed maps whose BSP and map metadata pass validation, and exposes max-player, LAN, editable server-name and password fields for listen-server modes. Dedicated-server process management and Steam Lobby/party services remain unfinished; the LAN Server Browser and validated Direct Connect are implemented separately.
- Server Browser uses the Steam Legacy engine's `pNetAPI` for GoldSrc LAN broadcast discovery, copies each transient response before the engine frees it, sorts results by ping, paginates the result list and joins a selected host through the validated Direct Connect path. The stock Internet master-server list is not enabled because the legacy `hl1master.steampowered.com` service is marked shut down in the GoldSrc protocol implementation; this LAN browser does not claim to enumerate public Internet servers.
- Connect to Server accepts a validated host/IP and optional port, then invokes the Legacy engine's `connect` command. Steam Lobby/party creation remains separate unfinished functionality.
- Game Settings opens a paginated Server Settings page backed by the same SessionConfiguration. It exposes time/frag limits, friendly fire, fall damage, weapon stay, flashlight, monster and spectator toggles.
- Multiplayer launch queues the matching Legacy cvars: mp_timelimit, mp_fraglimit, mp_friendlyfire, mp_falldamage, mp_weaponstay, mp_flashlight, mp_allowmonsters and allow_spectators. These settings are applied before and after map startup to override listen-server config defaults.

The client launch path validates map metadata/readiness before issuing commands. Singleplayer clears stale multiplayer mode cvars before starting the selected map. Deathmatch and Team Deathmatch set the Legacy deathmatch, coop and mp_teamplay cvars before map startup; max-player, hostname, password, LAN, difficulty, cheats and server rule values come from the shared session configuration. This command sequence still needs manual Steam Legacy verification; a successful DLL build does not prove that a listen-server session works in-game.


The three engine-owned GameUI resource overrides are checksum-locked in `tools/validate_brig.py`: `resource/gamemenu.res`, `resource/gameui_english.txt` and `resource/gameui_russian.txt`. CI accepts only the exact approved SHA-256 values for these files; other changes still fail the asset audit. The original preservation manifest is left intact.
