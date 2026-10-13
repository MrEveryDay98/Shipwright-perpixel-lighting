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

Windows and macOS keep the CMake plumbing inherited from Shipwright (vcpkg on Windows), but so far only the
Linux build has been tested.

## Controls

Defaults use libultraship's N64-style mapping. Gamepads work out of the box.

| Action | Keyboard | N64 button |
| --- | --- | --- |
| Move / aim up / morph (down) | W A S D | Stick or D-pad |
| Jump (hold for height) | X | A |
| Shoot, or lay a bomb as the ball | C | B |
| Toggle missiles | R or Z | R / Z |
| Pause | Space | Start |

## What works

- The whole world map: all five areas, decoded from the ROM, with doors and alternate palettes.
- Samus: run, variable-height jump, spin jump, aim up, morph ball, bomb jumps. Her sprite frames come
  from the ROM and are extruded into thin layers.
- Beams (short, long, wave, ice), missiles, bombs, blastable blocks that regrow.
- Blue, red (5 missiles) and orange (10 missiles) doors.
- All 38 power-ups and expansions, collected with the original item names.
- Elevators between Brinstar, Norfair, Kraid's Lair, Ridley's Lair and Tourian.

## Not done yet

- Enemies, bosses, Mother Brain, Zebetites and Metroids. Spawn data is decoded but not simulated.
- Damage, lava/acid, death and the escape sequence.
- Audio, passwords/saving, title screen and ending.
- A 3D Samus model. The renderer draws her as a billboard sprite for now; a model can replace it in
  `Renderer::BuildFrame`.

## Developer notes

- `METROID64_WARP=x,y` starts Samus in map screen `(x, y)` (for example `22,11` is Brinstar's elevator to Norfair).
- `METROID64_FRAMES=n` quits after `n` frames, for headless smoke tests.
- [`docs/ROM_FORMAT.md`](docs/ROM_FORMAT.md) records the ROM addresses and data formats the decoder relies on.

Source layout:

| File | Purpose |
| --- | --- |
| `metroid64/src/Rom.*` | ROM validation and decoding: world map, rooms, structures, macros, CHR, palettes, items, Samus frames |
| `metroid64/src/Game.*` | Gameplay: physics, weapons, doors, items, elevators |
| `metroid64/src/Renderer.*` | Turns the world into Fast3D display lists (cached per screen) |
| `metroid64/src/Hud.*` | ImGui overlay: energy, missiles, messages, ROM prompt |
| `metroid64/src/main.cpp` | libultraship bootstrap and main loop |
