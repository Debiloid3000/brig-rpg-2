# BRIG RPG 2 — Developer-only files

This folder contains local development dependencies and helper notes that are not part of the installable mod.

## Steamworks SDK

Place a Steamworks SDK obtained under Valve's developer agreement in `_devfolder/steamworks/sdk/`. The SDK source and import libraries are proprietary and intentionally excluded from Git. See `_devfolder/steamworks/README.md` and run `py tools/verify_steamworks_sdk.py` after extraction.

Do not copy a third-party mirror into this folder or commit Valve SDK files. The checked-in `.gitkeep` only preserves the expected local destination.
