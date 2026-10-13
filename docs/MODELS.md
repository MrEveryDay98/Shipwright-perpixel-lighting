# Models and animations

By default Metroid64 draws characters with the game's own sprites, as flat cards. Drop a model into
`mods/` and it replaces those sprites. The game itself is unchanged: the model is placed and animated from
the running game's memory (see [EMULATION.md](EMULATION.md)).

## Where mods go

Put mods in the `mods/` folder next to `metroid64.o2r`; the game creates the folder on first launch.
A mod can be any of these:

- an `.o2r` (zip) archive,
- an `.otr` archive,
- a `.zip` archive,
- a plain folder.

Files in mods override files of the same path in earlier archives.

## Which file replaces what

| Character | Manifest path |
| --- | --- |
| Samus | `models/samus.json` |

Enemies and items still use their sprites. Their positions and types are in RAM, so they can get models the
same way.

## Manifest format

```json
{
  "scale": 1.0,
  "offset": [0, 0, 0],
  "yaw_right": 0,
  "yaw_left": 180,
  "fallback": "Stand",
  "animations": {
    "Stand": { "fps": 1,  "frames": ["models/samus/stand"] },
    "Run":   { "fps": 12, "loop": true, "frames": ["models/samus/run_0", "models/samus/run_1"] }
  }
}
```

- **Units** are NES pixels: Samus is about 32 tall and a block is 8. The model origin sits at Samus's feet. +Y is up and +X is screen-right.
- **`yaw_right` / `yaw_left`** rotate the model (in degrees, around the vertical axis) for each facing direction.
- **`animations`** are keyed by the name of the game's current animation: `Stand`, `Run`, `Jump`, `Spin`, `Morph`, `RunShoot`, `JumpShoot`, `StandUp`, `RunUp`, `JumpUp`, `Front`. An animation restarts whenever the game switches animation.
- **Missing animations** use `fallback`.
- The model is hidden whenever the game doesn't draw Samus (while she blinks after taking damage, during the intro and so on).
- **Each frame** is a libultraship `DisplayList` resource: binary (`ODLT`) or XML. Frames are played at `fps`.

Exporting display lists from Blender with [Fast64](https://github.com/Fast-64/fast64) (its Ship of Harkinian/O2R output) produces resources in this format. Textures, vertices and lights referenced by the display lists load the same way they do in Shipwright.

`docs/example-mod/testmodel` is a small hand-written example: a blocky stand-in for Samus with a two-frame run cycle, in XML. To try it, copy the `testmodel` folder into `mods/`.

## Current limits

- Animation is per-animation frame sequences (flipbook). Skeletal animation (Shipwright-style skeleton and animation resources) is not wired up yet.
- Models are lit with one fixed light. They inherit the render state their display lists set.
