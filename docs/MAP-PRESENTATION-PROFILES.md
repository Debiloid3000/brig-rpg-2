# Map-local player models, sounds and dialogue

The project uses the same map-local pattern as GoldSrc map configuration: a map can have a CFG for settings and one shared sound replacement list for ordinary audio and named sentence lines.

## Map configuration

The server automatically checks for:

```text
maps/<mapname>.cfg
```

The map name is normalized to lowercase. For example, `They_Hunger` uses `maps/they_hunger.cfg`.

Two settings are understood both as worldspawn keyvalues and as commands in the map CFG:

| Setting | Purpose |
| --- | --- |
| `forcepmodels` | Comma-separated player-model folder names under `models/player` |
| `globalsoundlist` | Path to the map's sound replacement list |

Example `maps/they_hunger.cfg`:

```cfg
forcepmodels "survivor_a,survivor_b,scientist"
globalsoundlist "maps/they_hunger.gsr"
```

The model names above are examples only. Use folder names that actually exist in this mod's `models/player` directory.

The equivalent settings can be set on the map's worldspawn entity using the `forcepmodels` and `globalsoundlist` keyvalues. A CFG can override those values.

When `forcepmodels` is empty, the normal player-model selection is left unchanged. When it contains a list, only those model folders are accepted. If a player has selected a model outside that list, the first valid listed model is used instead. The listed player models are precached automatically.

When `globalsoundlist` is empty, the loader checks for `maps/<mapname>.gsr` automatically. You can point it to another relative `.gsr` path from the map CFG or worldspawn.

## One sound list for sounds and dialogue

There is no separate sound prefix and voice prefix. Both are handled by the same map-local `.gsr` file.

Example `maps/they_hunger.gsr`:

```text
"aslave/slv_alert1.wav" "they_hunger/slv_alert1.wav"
"scientist/sci_pain1.wav" "they_hunger/sci_pain1.wav"
"!SC_HEAL2" "they_hunger/sc_heal2.wav"
```

The first column is the original sound path or a named sentence beginning with `!`. The second column is the replacement file, relative to the mod's `sound/` directory. Do not add a leading `sound/` to either audio path.

A named sentence replacement is a direct audio-file replacement. This is suitable for replacing an existing voice line with one WAV file. It does not turn a one-file replacement into a newly assembled multi-part sentence.

Entries must be quoted path pairs. Blank lines and lines beginning with `#` or `//` are ignored. Unsafe, absolute or traversal paths are rejected. Audio paths associated with weapons are deliberately not accepted by this map sound-list implementation.

## Asset layout

For the example above, the relevant files would be arranged like this:

```text
maps/
  they_hunger.cfg
  they_hunger.gsr

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
    slv_alert1.wav
    sci_pain1.wav
    sc_heal2.wav
```

This is a layout example, not a claim that those model folders or audio files are already present. The actual model names and replacement audio must match files included with the mod.

## Map changes and reset behavior

Map-local values are cleared when the server deactivates a map. The next map then reads its own worldspawn settings, CFG and sound list, so the previous map's player-model restrictions or sound substitutions are not intentionally carried over.
