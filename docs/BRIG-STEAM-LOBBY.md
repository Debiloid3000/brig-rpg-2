# BRIG RPG 2 — Steam Lobby Integration Contract

## Current state

The multiplayer menu has Lobby and Create Lobby navigation pages, but Steam matchmaking is not implemented. The current repository contains the Legacy engine `pNetAPI` server-query path and Steam utility types, but no Steamworks matchmaking headers, client library integration, Steam API initialization, or `ISteamMatchmaking` bridge. The menu therefore reports the feature as unavailable instead of claiming that a lobby can be created.

`pNetAPI` is used only for the existing LAN server browser. It does not create Steam lobbies, list Steam lobbies, invite friends, or join a Steam lobby by ID.

## Local Steamworks SDK setup

Valve's official distribution route is the authenticated [Steamworks SDK portal](https://partner.steamgames.com/doc/sdk). No Valve-owned public GitHub source repository is used by this project. Public repositories labelled as SDK mirrors are third-party mirrors, not an official source of record, so BRIG does not add one as a submodule.

After obtaining access to the Steamworks SDK and accepting Valve's SDK Access Agreement, extract the archive's `sdk` directory to:

```text
_devfolder/steamworks/sdk/
  public/steam/steam_api.h
  public/steam/isteammatchmaking.h
  redistributable_bin/steam_api.dll
  redistributable_bin/steam_api.lib
  redistributable_bin/linux32/libsteam_api.so
```

Run `py tools/verify_steamworks_sdk.py` to validate the local 32-bit Windows and Linux files required by this Steam Legacy target. The contents of `_devfolder/steamworks/sdk/` are intentionally git-ignored. Do not commit proprietary headers, import libraries, examples, or other SDK source files. Valve's agreement permits local reproduction of the SDK source to develop licensee software and permits redistribution only of the designated `redistributable_bin` files with licensee software in object-code form; it does not make the full SDK open source.

SDK presence is only dependency preparation. It does not prove that a current Steamworks SDK client interface is compatible with the installed Steam Legacy client, nor that a mod loaded inside Half-Life can initialize the expected Steam AppID/interface versions. API initialization, callback pumping, matchmaking calls, game-server association, and Legacy runtime compatibility must be built and tested before Lobby/Create Lobby are marked ready.
## Required implementation

1. Add the intended Steamworks SDK dependency for the target runtime and document its version, x86 library, runtime DLL, App ID/test setup, and redistribution constraints. Do not invent interface vtables or copy undocumented ABI layouts.
2. Initialize and shut down the Steam API safely in the client process, verify Steam is running and the user is logged in, and pump callbacks on the client main thread.
3. Implement asynchronous CreateLobby, RequestLobbyList, JoinLobby and LeaveLobby flows with explicit pending/success/failure states and callback lifetime handling.
4. Publish lobby metadata for protocol/build compatibility, selected game mode, map, player capacity, password/private state, and readiness. Filter incompatible or full lobbies.
5. Display actual member identities and capacity; update the view on join/leave/member-data callbacks.
6. Add invites through the supported Steam overlay/friend invite mechanism, plus handling for an invite received while the game is running or launched with a lobby-connect command.
7. Define how a lobby starts a game: the owner selects Listen Server or a dedicated server, publishes the server address/Steam game-server identity, and clients join through a supported connection route. A lobby by itself does not carry GoldSrc game traffic.
8. Handle owner migration, lobby closure, host disconnect, failed server startup, reconnect/timeout, leaving the lobby, and returning to the main menu.
9. Keep LAN Server Browser and Direct Connect working when Steam matchmaking is unavailable; do not conflate them with Steam lobbies.

## Completion tests

- Two separate Steam Legacy clients can create, find, join and leave a lobby.
- A private/friends-only/public lobby uses the chosen visibility and invite behavior.
- Member lists and metadata stay synchronized during joins/leaves.
- Invite acceptance while in-game and while launching the game reaches the intended lobby.
- The lobby owner can start a listen or dedicated server and all members reach the same server/map/mode.
- Missing Steam client, missing runtime dependency, unavailable API, callback errors and full/incompatible lobbies produce a clear message without crashing.
- The existing LAN browser, Direct Connect and Singleplayer routes still work.

## Official API references

- `ISteamMatchmaking`: CreateLobby, RequestLobbyList, JoinLobby, LeaveLobby, invite and lobby metadata callbacks.
- Steam Matchmaking & Lobbies: the asynchronous lobby lifecycle, lobby ownership and connecting lobby members to a game server.

Source references: https://partner.steamgames.com/doc/api/isteammatchmaking and https://partner.steamgames.com/doc/features/multiplayer/matchmaking
