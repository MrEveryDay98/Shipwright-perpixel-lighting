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

## Skeletal models

A model can instead be a skeleton with animations, in Ship of Harkinian's resource formats, so skeletons and
animations exported for Shipwright (for example with Fast64) load unchanged:

```json
{
  "scale": 0.1,
  "yaw_right": 90,
  "yaw_left": -90,
  "skeleton": "models/samus/skel",
  "fallback": "Stand",
  "animations": {
    "Stand": { "animation": "models/samus/anim_stand" },
    "Run":   { "animation": "models/samus/anim_run", "fps": 30 }
  }
}
```

- **`skeleton`** is a `Skeleton` resource (`OSKL`). Normal and Flex skeletons with Standard or LOD limbs (`OSLB`) work. Flex skeletons get their limb matrices through segment `0x0D`, as in Shipwright.
- **`animation`** is an `Animation` resource (`OANM`) of the Normal kind. It plays at `fps` animation frames per second (default 20, the rate Shipwright's animations are made for), interpolated between frames.
- Curve and Skin limbs, and Link's own animation format, are not supported.
- Animation resources can also be written as XML (`<Animation FrameCount StaticIndexMax>` with `<Values>` and `<JointIndices>` lists), which is handy for hand-made tests.

`docs/example-mod/skeltest` is a hand-written two-limb skeleton (a body and a swinging arm) with `Stand` and `Run` animations, all in XML.

## Examples

`docs/example-mod/testmodel` is a small hand-written example: a blocky stand-in for Samus with a two-frame run cycle, in XML. To try it, copy the `testmodel` folder into `mods/`.

## Current limits

- Only Samus can be replaced so far.
- Models are lit with one fixed light. They inherit the render state their display lists set.
