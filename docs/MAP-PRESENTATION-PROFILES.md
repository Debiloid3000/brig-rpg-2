# Map-local player models, dialogue and first-person controls

The map setup follows the Sven Co-op-style GoldSrc pattern: a map CFG selects allowed player-model folders and one shared sound replacement list. That one list can contain ordinary non-weapon audio paths and named sentence replacements, so there is no separate voice prefix.

## Map configuration

The server checks for this file automatically:

```text
maps/<mapname>.cfg
```

Map names are normalized to lowercase. For example, `They_Hunger` uses `maps/they_hunger.cfg`.

The supported map settings are also accepted as worldspawn keyvalues:

| Setting | Purpose |
| --- | --- |
| `forcepmodels` | Comma-separated allowed player-model folders under `models/player` |
| `globalsoundlist` | Path to one sound/sentence replacement list |

Example `maps/they_hunger.cfg`:

```cfg
forcepmodels "survivor_a,survivor_b,scientist"
globalsoundlist "they_hunger/soundrep.txt"
```

Model names are folder names, not model filenames. The folders and their matching `<folder>.mdl` files must exist under `models/player`. The example names are placeholders; use names that actually exist in this mod.

When `forcepmodels` is empty, normal player-model selection is unchanged. When it contains a list, only valid listed folders are accepted. A player's unsupported selection is replaced with the first valid model in the list. The selected folders are precached automatically.

## One file for ordinary audio and dialogue

`globalsoundlist` is one shared replacement file. Its default location is:

```text
sound/<mapname>/soundrep.txt
```

You can also specify a path explicitly in the map CFG or the worldspawn `globalsoundlist` keyvalue. A path beginning with `sound/` is used as-is; a path such as `they_hunger/soundrep.txt` is resolved relative to the mod's `sound/` directory. The older `.gsr` extension is still accepted for compatibility.

Example `sound/they_hunger/soundrep.txt`:

```text
"aslave/slv_alert1.wav" "they_hunger/slv_alert1.wav"
"scientist/sci_pain1.wav" "they_hunger/sci_pain1.wav"
"!SC_HEAL2" "they_hunger/sc_heal2.wav"
```

Each line is a quoted pair: original sound path or named sentence on the left, replacement file on the right. Audio paths are relative to the mod's `sound/` directory; do not put `sound/` at the start of the individual audio paths.

A named sentence begins with `!`. This lets the same replacement list redirect an existing voice line without adding a separate voice-prefix setting. A sentence replacement points to one audio file; it does not assemble multiple WAV files into a new multi-part sentence.

Blank lines and lines beginning with `#` or `//` are ignored. Absolute paths, traversal paths, invalid characters, and weapon-associated audio paths are rejected. Replacement files are precached when the map list loads, before map entities begin playing them. Missing or invalid list files leave the original audio unchanged.

## Camera and first-person weapon settings

These are personal client settings, not map-profile options. They are registered when the client HUD starts; changing them does not modify the server's map configuration.

| Cvar | Default | Purpose | Accepted range |
| --- | ---: | --- | ---: |
| `default_fov` | 90 | Player/world camera FOV | Existing game limits |
| `viewmodel_fov` | 68 | First-person weapon model size | 54–68 |
| `viewmodel_offset_x` | 0 | Move weapon left/right | -2 to 2.5 |
| `viewmodel_offset_y` | 0 | Move weapon forward/back | -2 to 2 |
| `viewmodel_offset_z` | 0 | Move weapon down/up | -2 to 2 |

The offsets are applied to the viewmodel only; they do not move the player's camera. The cvars are archived, so a player's chosen values persist after they change them.

`viewmodel_fov` is implemented as a viewmodel-only geometry scale in the client Studio renderer. It changes the apparent size of the hands/weapon without changing the world-camera `default_fov`. GoldSrc exposes a shared scene projection through this client interface, so this is an FOV-style visual control rather than a separate renderer projection matrix identical to CS2.

Three starting layouts are available:

```cfg
viewmodel_presetpos 1 // default / centered
viewmodel_presetpos 2 // larger / closer
viewmodel_presetpos 3 // classic-style offset
```

A preset sets the four viewmodel cvars; the individual values can then be adjusted normally. Presets are not applied automatically.

## Bob and camera roll defaults

Both settings default to zero for a fresh client configuration:

- `cl_bob 0` — disables weapon bob.
- `cl_rollangle 0` — disables camera roll.

These are client defaults, not map-profile settings. Players can still change either value in their own config or console.

## Asset layout

For the example above:

```text
maps/
  they_hunger.cfg

models/
  player/
    survivor_a/
      survivor_a.mdl
    survivor_b/
      survivor_b.mdl
    scientist/
      scientist.mdl

sound/
  they_hunger/
    soundrep.txt
    slv_alert1.wav
    sci_pain1.wav
    sc_heal2.wav
```

This is an example layout, not a claim that these model folders or audio files are already included. The names must match the assets present in the mod.

## Map changes and reset behavior

Map-local `forcepmodels` and `globalsoundlist` settings are cleared when the server deactivates a map. The next map loads its own worldspawn values, CFG and sound replacement list, so the previous map's player-model restrictions and dialogue substitutions are not intentionally carried over.
