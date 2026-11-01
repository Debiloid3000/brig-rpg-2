# Map presentation profiles

A map presentation profile is a small server-side config file that is loaded automatically when a map starts. It lets a map choose its weapon pickups, player model folder, first-person weapon model folder, sound set and sentence-group aliases without turning the map into a separate game mode.

## Player view settings

Put personal settings in the player's `autoexec.cfg` (or add the same lines to a config that is executed for that player):

```cfg
default_fov 90

viewmodel_offset_x 0
viewmodel_offset_y 0
viewmodel_offset_z 0
viewmodel_fov 68
viewmodel_presetpos 1

// Optional: remove bob and camera roll.
cl_bob 0
cl_rollangle 0
```

The offset variables move only the first-person weapon model:

| Variable | Meaning | Clamped range |
| --- | --- | ---: |
| `viewmodel_offset_x` | Left / right | -2 to 2.5 |
| `viewmodel_offset_y` | Forward / backward | -2 to 2 |
| `viewmodel_offset_z` | Down / up | -2 to 2 |
| `viewmodel_fov` | Weapon model size control | 54 to 68 |

`viewmodel_presetpos 1`, `2` and `3` apply built-in starting layouts. They are convenience presets, not exact copies of CS2's layouts. The individual offsets can be changed afterwards.

**Important FOV limitation:** `default_fov` is the actual camera FOV already supported by the game. The current client-DLL implementation of `viewmodel_fov` scales the viewmodel as an approximation. A truly independent viewmodel projection FOV requires changing the GoldSrc renderer/engine, not only the game DLL. The new command does not claim to provide that renderer-level feature.

This project registers `cl_rollangle` and `cl_bob`. Use those client settings to remove view roll and weapon bob. The screenshot's `sv_rollangle` is not the corresponding client-side variable in this codebase.

## Per-map config

Create a file named after the map in the mod's `maps` directory:

```text
maps/<mapname>.cfg
```

Map names in the loader are normalized to lowercase. For example, a map named `They_Hunger` loads `maps/they_hunger.cfg`.

Example profile:

```cfg
// Logical profile name for server logs / future extensions.
brig_map_profile "they_hunger_classic"

// Exact weapon entity classnames allowed as map pickups.
// Empty, "*" or "all" means no weapon-pickup restriction.
brig_map_weapon_allowlist "weapon_crowbar,weapon_9mmhandgun,weapon_shotgun"

// Player model directory name (the folder under models/player).
// Leave empty to keep the player's selected model.
brig_map_player_model "hunger_survivor"

// Replacement first-person viewmodels are looked up by basename:
// <prefix>/v_9mmhandgun.mdl, <prefix>/v_shotgun.mdl, etc.
brig_map_viewmodel_prefix "models/brig/profiles/they_hunger"

// Replacement weapon/NPC sounds preserve the original relative filename:
// sound/<prefix>/weapons/357_shot1.wav, etc.
brig_map_sound_prefix "brig/profiles/they_hunger"

// Sentence aliases are resolved as <prefix><original sentence/group name>.
brig_map_voice_prefix "TH_"
```

The profile loader clears the previous map's values before executing the new map config, so settings do not intentionally carry over between maps.

## Asset layout

For the example above, add only the replacement assets that the map actually needs:

```text
models/
  player/
    hunger_survivor/
      ...
  brig/
    profiles/
      they_hunger/
        v_9mmhandgun.mdl
        v_shotgun.mdl

sound/
  brig/
    profiles/
      they_hunger/
        weapons/
          357_shot1.wav
          ...
  sentences.txt
```

- A missing replacement viewmodel falls back to the original `v_*.mdl` model.
- A missing replacement sound falls back to the original sample.
- `brig_map_player_model` is a player-model folder name, not a filesystem path.
- The voice prefix does not load a separate per-map `sentences.txt`. Add prefixed sentence entries (for example, `TH_HEV_ON` or `TH_SC_HELLO`) to the mod's normal `sound/sentences.txt` and place their audio files in the normal sound locations. If a prefixed entry is not present, the original sentence is used.

## Current weapon restriction scope

`brig_map_weapon_allowlist` filters map-spawned entities whose classname begins with `weapon_`. It does not yet strip weapons that a player already carries through a level transition or changes to the game's default starting inventory. That needs a separate inventory policy and should be designed per game rules, rather than silently deleting player equipment.

## Classic-like presentation

Use `brig_map_profile` as the profile identifier and combine the settings above to create a classic-like presentation on selected maps: different world weapon pickups, different first-person weapon models, a selected player model, replacement weapon sounds and alternate voice lines. It is intentionally data-driven; it does not enable a separate ruleset, campaign, HUD, or complete Classic game mode by itself.

The exact player-model folder and the content of the They Hunger asset set must be filled in with the assets present in this mod. The example names above are placeholders, not claims that those models or sounds already exist.
