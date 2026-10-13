# Models and animations

Every character in Metroid64 is drawn through a *visual*. By default that is the original sprite,
decoded from your ROM and drawn as a flat card. Drop a model into `mods/` and it replaces the sprite.
Gameplay does not change, because the game only ever tells a visual which **pose** to show.

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
| Enemy | `models/enemies/<area>_<type>.json`. Area is `brinstar`, `norfair`, `tourian`, `kraid` or `ridley`; type is the enemy type nibble in hex, e.g. `models/enemies/brinstar_5.json` for the Zoomer. |
| Power-up | `models/items/<id>.json`. IDs: 0 bombs, 1 high jump, 2 long beam, 3 screw attack, 4 maru mari, 5 varia, 6 wave beam, 7 ice beam, 8 energy tank, 9 missiles. |

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

- **Units** are NES pixels: Samus is about 32 tall and a block is 8. The model origin sits at Samus' feet, or at the centre of enemies and items. +Y is up and +X is screen-right.
- **`yaw_right` / `yaw_left`** rotate the model (in degrees, around the vertical axis) for each facing direction.
- **`animations`** are keyed by pose name:
  - Samus: `Stand`, `Run`, `Jump`, `Spin`, `Morph`, `RunShoot`, `JumpShoot`, `StandUp`, `RunUp`, `JumpUp`, `Front`.
  - Enemies: `Move`.
  - Items: `Stand`.
- **Missing poses** use `fallback`.
- **Each frame** is a libultraship `DisplayList` resource: binary (`ODLT`) or XML. Frames are played at `fps`.

Exporting display lists from Blender with [Fast64](https://github.com/Fast-64/fast64) (its Ship of Harkinian/O2R output) produces resources in this format. Textures, vertices and lights referenced by the display lists load the same way they do in Shipwright.

`docs/example-mod/testmodel` is a small hand-written example: a blocky stand-in for Samus with a two-frame run cycle, in XML. To try it, copy the `testmodel` folder into `mods/`.

## Current limits

- Animation is per-pose frame sequences (flipbook). Skeletal animation (Shipwright-style skeleton and animation resources) is not wired up yet. The pose interface above is where it would plug in.
- Models are lit with one fixed light. They inherit the render state their display lists set.
