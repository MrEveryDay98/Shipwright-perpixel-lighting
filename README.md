# Metroid64

A 2.5D remake of NES *Metroid* built on [libultraship](https://github.com/Kenix3/libultraship), the engine
behind Ship of Harkinian (Shipwright). Every 8x8 background tile of the original becomes a textured 3D block,
viewed by a perspective camera that follows Samus.

This project began as a fork of Shipwright. All of the Ocarina of Time code, its asset pipeline (Torch) and its
tooling have been removed. What's left is libultraship (window, Fast3D renderer, input, ImGui, archives) and the
Metroid64 game in [`metroid64/`](metroid64).

## You need your own ROM

Metroid64 ships **no Nintendo data**. At startup it reads the level layout, tiles, palettes, items, doors,
elevators and Samus' animation frames from your own `Metroid (USA).nes`. Only that exact dump is accepted
(its PRG ROM is checked against a CRC32).

You can provide the ROM in any of these ways:

- drag and drop the `.nes` file onto the game window,
- pass it on the command line: `metroid64 "Metroid (USA).nes"`,
- put it next to the executable as `metroid.nes` (any `.nes` file in that folder is tried).

The path is remembered in `metroid64.json`.

## Building

Linux (Debian/Ubuntu):

```sh
git submodule update --init --recursive
sudo apt-get install g++ $(cat linux-build-deps/apt.txt)
cmake -H. -Bbuild -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/metroid64/metroid64 "path/to/Metroid (USA).nes"
```

The build produces `build/metroid64/metroid64` and `metroid64.o2r`. The archive holds only libultraship's
renderer shaders and must sit next to the executable.

If CMake can't download `stb_image.h` (offline or behind a proxy), install `libstb-dev` and the build uses the
system copy.

Windows builds come from CI: see the `metroid64-windows` artifact on the repository's Actions page. CI builds
with MSVC and a static vcpkg. macOS keeps the CMake setup inherited from Shipwright but has not been tried.

## Controls

Defaults use libultraship's N64-style mapping. Gamepads work out of the box. Press **F1** for the menu,
then **Controls → Controller configuration** to remap anything.

| Action | Keyboard | N64 button |
| --- | --- | --- |
| Move / aim up / morph (down) | W A S D | Stick or D-pad |
| Jump (hold for height) | X | A |
| Shoot, or lay a bomb as the ball | C | B |
| Toggle missiles | R or Z | R / Z |
| Pause | Space | Start |

## Menu (F1)

- **Game**: Open ROM, Reset, Pause, Quit.
- **Graphics**:
  - Fullscreen and VSync.
  - Frame interpolation: logic stays at 60 Hz and rendering is interpolated up to the monitor's refresh rate or a target FPS.
  - Internal resolution, MSAA, low-resolution modes, sharp (nearest-neighbour) upscaling, texture filter.
  - Camera FOV, distance and height.
  - Renderer backend.
- **Controls**: libultraship's controller configuration window.
- **Developer**: stats and console windows, infinite energy/missiles, give all items.

## What works

- The whole world map: all five areas, decoded from the ROM, with doors and alternate palettes.
- Samus:
  - run, variable-height jump, somersault, aim up, morph ball and bomb jumps;
  - animations use the ROM's own animation tables and frames, drawn as flat sprites;
  - contact damage, knockback, Varia's damage reduction, death and respawn at the last elevator.
- Enemies placed from the ROM's room data and pipes, with sprites and animations from the ROM's per-area enemy tables:
  - Brinstar: Zoomer, Skree, Ripper, Rio, Reo, Zeb;
  - Norfair: Geruta, Ripper II, Nova, Gamet, Multiviola, Dragon, Squeept;
  - Tourian: Metroids, Rinka;
  - Kraid's Lair: Sidehoppers, Zeela, Geega, Kraid;
  - Ridley's Lair: Holtz, Dessgeega, Viola, Zebbo, Multiviola, Ridley.

  Each kind runs a reimplemented behaviour: crawling, diving, swooping, hopping, pipe swarms, latching, bouncing, spitting, and simple boss patterns.
- Combat:
  - beams (short, long, wave, ice) and missiles;
  - the ice beam freezes enemies into platforms, Rippers only take missiles, and Metroids only take damage while frozen;
  - the screw attack kills on contact;
  - enemies drop energy and missiles.
- Blastable blocks that regrow, and blue, red (5 missiles) and orange (10 missiles) doors.
- All 38 power-ups and expansions, using the ROM's item sprites.
- Elevators between all areas.
- Model support: any character can be replaced by a 3D model with per-pose animations. See [docs/MODELS.md](docs/MODELS.md).

## Not done yet

- Enemy behaviour is reimplemented and not frame-exact to the original.
- Some enemy kinds the ROM defines aren't placed or simulated. Mellows and Memus come from special item data, which isn't read yet.
- Mother Brain, Zebetites, Tourian cannons, and statues/bridges.
- Lava and acid damage, and the escape sequence.
- Audio, passwords/saving, title screen and ending.
- Skeletal animation for models (per-pose flipbooks work today).

## Developer notes

- `METROID64_WARP=x,y` starts Samus in map screen `(x, y)` (for example `22,11` is Brinstar's elevator to Norfair).
- `METROID64_FRAMES=n` quits after `n` frames (one game tick per frame), for headless smoke tests.
- `METROID64_ALLITEMS=1` starts with every upgrade.
- [`docs/ROM_FORMAT.md`](docs/ROM_FORMAT.md) records the ROM addresses and data formats the decoder relies on.

Source layout:

| File | Purpose |
| --- | --- |
| `metroid64/src/Rom.*` | ROM validation and decoding: world map, rooms, structures, macros, CHR, palettes, items, Samus frames |
| `metroid64/src/Game.*` | Gameplay: physics, weapons, damage, doors, items, elevators |
| `metroid64/src/Enemy.*` | Enemy spawning, AI per archetype, combat and drops |
| `metroid64/src/Visual.*` | Sprite and model visuals; model manifest loading |
| `metroid64/src/Renderer.*` | Turns the world into Fast3D display lists (cached per screen), draws visuals |
| `metroid64/src/Menu.*` | F1 menu: graphics, frame rate, controls, developer |
| `metroid64/src/Hud.*` | ImGui overlay: energy, missiles, messages, ROM prompt |
| `metroid64/src/main.cpp` | libultraship bootstrap and main loop |
