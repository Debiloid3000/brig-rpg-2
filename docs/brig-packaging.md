# Brig resource layout and releases

`_folder/brig` is the standalone **mod directory** for the Steam Half-Life engine.
The release workflow builds both game DLLs from this repository for Windows x86;
the source asset directory intentionally contains no borrowed or stale game DLLs.
Half-Life must be installed and switched to `steam_legacy`. This does not provide
a separate engine, additional weapon/NPC code, RPG systems, or Sven game rules.

| Source | Destination / policy |
| --- | --- |
| `valve` | Protected HL base maps, models, sounds, sprites, events and UI; discard saves, caches, personal configs and engine/game binaries. Supply clean startup controls and Brig metadata. |
| `valve_russian` | HL Russian localization resources. |
| `valve_hd` | Optional `models/_Extra/valve_hd`, `sound/_Extra/valve_hd`, `sprites/_Extra/valve_hd`; never silently replace the base. |
| `svencoop` | Compatible models in `models/svencoop`, audio in `sound/svencoop`; add only missing compatible SPR files. Exclude Sven DLLs, scripts, events, configs, maps, HUD and UI definitions. |
| `svencoop_addon` | Overlay its models/audio onto the Sven namespace. Preserve **every original file**. Archive incompatible maps, GMR/GSR lists, configs and UI under `brig/_Extra/svencoop_addon`; conflicting sprites and adapted originals under `brig/_Extra/originals`. |
| `svencoop_event_*` | Separate namespaced model/audio variants, including nested Xmas variant folders; accompanying credits/UI images are inactive options. |
| `svencoop_downloads` | Downloaded map reports/metadata are excluded. |
| `cstrike`, `cstrike_hd` | Only `models` (strictly excluding `models/player`) and `sound`, under their own namespaces. No classes, cfg, maps, events, UI or game binaries. The supplied CS sound folder has only text metadata, no audio samples. |
| `Extras` | All originals preserved and sorted by resource type into `models/_Extra`, `sound/_Extra`, `sprites/_Extra`, `gfx/_Extra`, `resource/_Extra`, `docs/_Extra`. |

Resource namespaces prevent custom viewmodels, CS weapons, NPC skins and holiday
models from replacing the HL campaign assets. These are asset collections, and
must be selected by code/map resource references when developing the mod. Merely
installing a model never adds its original game's weapon/NPC mechanics.

Extra variants have stable directory IDs derived from their original parent path.
This keeps engine resource paths below `MAX_QPATH=64`, avoids name collisions and
keeps texture companion files beside their models. The manifest maps the short
paths back to the complete descriptive original names.

OGG (including files mislabeled `.wav`), stereo WAV and unsupported PCM formats are
adapted to mono PCM16 at 22050 Hz. Compatible WAVs are copied unchanged. Original
audio is preserved, and RIFF cue/loop markers are retained with resampled offsets.
Addon audio has priority over matching Sven audio. Base HL audio is protected.
Model sequence-group and sound-event fields are remapped in their existing
fixed-width binary fields when needed. SPR files are copied without conversion
or editing; incompatible/oversized files and conflicting addon sprites are kept
as inactive originals. A studio v9 file and models over legacy limits are also
archived rather than exposed as playable models.

The unified map registry and per-map CFGs are source-owned metadata already stored beside the BSPs under `_folder/brig/maps/`. The package builder copies `maps/brig_maplist.txt` and one `maps/<id>.cfg` for every identifier in that manifest from this mod source folder into the release mod directory. The allowlist is derived from the manifest, validates safe unique IDs, and requires each matching CFG to exist. All copied metadata files are byte-for-byte checked against their source copies during release packaging.

These hand-authored metadata files are kept outside the generated asset-audit JSON to avoid conflating map configuration with migrated/borrowed resources. No BSP is generated or copied by this metadata step; BSP payloads continue to come only from the audited asset manifest. Unlisted CFGs or map payload files are not implicitly swept into a release.

`docs/brig-assets.json` is the audit: every output file, SHA-256, original source,
adaptation, excluded source/reason, and preservation checks for all addon/Extras
originals. `python tools/validate_brig.py` validates checksums, case collisions,
studio v10 headers/tables/limits and dependencies, SPR v2 frame data, WAV format,
BSP v30 lumps, HUD sprite references, clean startup metadata and source-matched
`network/delta.lst`. Unknown payload files fail validation. Dot-prefixed file
synchronization temporary copies are ignored and never packaged.

Git attributes disable text conversion for the complete mod directory, source
startup templates and `network/delta.lst`. A Git autocrlf regression test checks
that Windows checkouts preserve the same audited bytes as Linux.

The migration script refuses to overwrite an existing Brig directory. To repeat
the migration, restore `_folder/need restock` from the `source_commit` in the audit
in a separate checkout, install ffmpeg, and run:

```sh
python tools/migrate_brig.py --remove-source
```

Only after the generated result passes validation is the original staging folder
deleted. Git history retains the input. Do not add that staging folder back to
new release commits.

## Binary compatibility

The copied SDK exposes anniversary-only tail callbacks despite a frozen API
version. Client initialization now copies only the legacy engine-table prefix;
the server does the same for its engine callbacks. Guard-page regression tests
put an unreadable page directly after the old table. HLTV stufftext is consumed
and ignored when the filtered client-command callback is absent. It is never
replaced by an unrestricted command call.

## CI and Releases

CI builds Linux SDK binaries with GCC/Clang, runs 32-bit guard-page tests, validates
the assets, builds the Windows x86 DLLs with v143, and makes a checked installable
Windows ZIP artifact. The release workflow publishes the Windows package only.

1. Merge the prepared branch into `master`.
2. Push a tag such as `v1.0.0`, or use **Actions → Release brig for steam_legacy →
   Run workflow**, select the desired branch/commit and enter a new `v*` tag.
3. The workflow compiles DLLs, validates resources, checks DLL architecture/exports,
   packages `brig/`, reads every ZIP member to verify CRC, and emits SHA-256.
4. It verifies that an existing tag resolves to the built commit, uploads the ZIP
   and checksum to a draft release, then publishes the complete release. A manual
   run creates a missing tag at the selected commit. Existing releases are never
   overwritten: use a new version. A failed publication can leave a draft release;
   remove that incomplete draft before retrying the same tag.

`GITHUB_TOKEN` needs `contents: write` only in the publish job. No personal token,
external packager or RAR installation is required. Repository Actions must be
enabled (GitHub disables inherited workflows in newly created forks).

## In-game acceptance check

CI cannot run a licensed graphical Steam Half-Life installation. Before marking a
version stable, test on a real `steam_legacy` install: menu startup, New Game
(`c0a0`), Hazard Course (`t0a0`), a local `crossfire` server, HUD/ammo/health,
weapon viewmodels, sound playback, save/load and reconnect. Resource checks and
successful compilation do not claim a completed engine playtest.
