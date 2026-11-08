# Local Steamworks SDK

BRIG needs the 32-bit Steamworks API headers and redistributables for its Steam Legacy client/server build targets. This folder is only the local destination; the SDK itself is not committed.

1. Sign in to the Steamworks Partner site and obtain the SDK from the official [Steamworks SDK page](https://partner.steamgames.com/doc/sdk).
2. Accept and comply with Valve's SDK Access Agreement.
3. Extract the archive's `sdk` folder into this directory, so `sdk/public/steam/steam_api.h` and `sdk/redistributable_bin/` exist.
4. Run `py tools/verify_steamworks_sdk.py` from the repository root.

The SDK verifier checks the Windows DLL is PE32/i386, the Windows import library is an archive, and the Linux shared library is ELF32/i386. It rejects a 64-bit SDK instead of silently accepting it.

The current project expects the 32-bit files `public/steam/steam_api.h`, `public/steam/isteammatchmaking.h`, `redistributable_bin/steam_api.dll`, `redistributable_bin/steam_api.lib`, and `redistributable_bin/linux32/libsteam_api.so`.

## Why this is not a Git submodule

The official distribution route is Valve's authenticated portal. A public GitHub repository labelled as a Steamworks SDK mirror is not a Valve-owned source-of-record repository. BRIG therefore does not depend on an unofficial mirror or distribute the SDK source through GitHub.

Valve's agreement allows licensees to reproduce SDK source locally for development and allows only designated `redistributable_bin` files to be distributed with licensee software in object-code form. The complete SDK, headers, and import libraries must remain local and ignored by Git.

## Compatibility gate

Possessing the SDK does not establish compatibility with Steam Legacy's client-side Steam interfaces or Half-Life's AppID context. Do not enable the matchmaking feature in release builds until `SteamAPI_Init`, callback dispatch, lobby create/search/join/leave, game-server handoff, and 32-bit Legacy runtime tests pass.
