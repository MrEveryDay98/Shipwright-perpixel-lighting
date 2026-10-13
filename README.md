# Metroid64

NES *Metroid* in 2.5D, built on [libultraship](https://github.com/Kenix3/libultraship), the engine behind
Ship of Harkinian (Shipwright).

The original game runs unmodified on an emulated NES inside Metroid64, so every physics rule, enemy behaviour,
item and door is the original's. Metroid64 changes only how the game is shown: every solid 8x8
background tile becomes a textured 3D block, seen through a perspective camera that follows the game's own
scrolling. See [docs/EMULATION.md](docs/EMULATION.md).

This project began as a fork of Shipwright. All of the Ocarina of Time code, its asset pipeline (Torch) and its
tooling have been removed. What's left is libultraship (window, Fast3D renderer, input, ImGui, archives) and the
Metroid64 game in [`metroid64/`](metroid64).

## You need your own ROM

Metroid64 ships **no Nintendo data**. It runs your own `Metroid (USA).nes`. Only that exact dump is accepted
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

The game sees a standard NES controller. Defaults use libultraship's N64-style mapping. Gamepads work out of the
box. Press **F1** for the menu, then **Controls → Controller configuration** to remap anything.

| NES button | Does | Keyboard | N64 button |
| --- | --- | --- | --- |
| D-pad | move, aim up, morph (down) | W A S D | Stick or D-pad |
| A | jump | X | A |
| B | shoot, or lay a bomb as the ball | C | B |
| Select | toggle missiles | R or Z | R / Z |
| Start | start, pause | Space | Start |

## Menu (F1)

- **Game**: Open ROM, Reset (power cycle), Freeze emulation, Quit.
- **Audio**: volume, mute.
- **Graphics**:
  - Fullscreen and VSync.
  - Frame interpolation: the NES runs at 60 Hz and the camera is interpolated up to the monitor's refresh rate or a target FPS.
  - Internal resolution, MSAA, low-resolution modes, sharp (nearest-neighbour) upscaling, texture filter.
  - 3D play field on or off (off shows the original 2D picture), camera FOV, distance and height.
  - Renderer backend.
- **Controls**: libultraship's controller configuration window.
- **Developer**: stats and console windows.

## What works

- The game runs from its original code. Tested so far: the title screen, starting a game, and Brinstar's first corridor, doors and vertical shaft. Other areas, passwords and the ending have not been played through yet.
- The play field in 3D from the live nametables, including scrolling, doors, destroyed blocks and palette effects.
- Sprites as flat cards, with door bubbles placed on the walls and the status display as an overlay.
- Sound and music from an emulated APU (volume and mute in the Audio menu).
- Model support: Samus can be replaced by a 3D model animated from the game's own animation state. See [docs/MODELS.md](docs/MODELS.md).

## Not done yet

- Saving the emulator state (the game's own passwords are the only way to save).
- Models for enemies and items, and skeletal animation for models.
- In vertical shafts the view doesn't extend past the NES screen's left and right edges, because the game only keeps one screen's width there.

## Developer notes

- [docs/EMULATION.md](docs/EMULATION.md) covers the emulator, the RAM addresses and sprite tagging the 3D view relies on, and developer environment variables for scripted headless runs.

Source layout:

| File | Purpose |
| --- | --- |
| `metroid64/src/nes/Cpu6502.*` | 6502 CPU |
| `metroid64/src/nes/Nes.*` | NES: memory map, MMC1, PPU, controller, frame timing, 2D reference renderer |
| `metroid64/src/nes/Apu.*` | NES audio |
| `metroid64/src/Emulator.*` | Metroid on the NES: ROM check, sprite ownership, scroll tracking, RAM addresses |
| `metroid64/src/Renderer.*` | 3D presentation of the emulated picture; 2D screens |
| `metroid64/src/Model.*` | Model manifests and drawing |
| `metroid64/src/Menu.*` | F1 menu: graphics, frame rate, controls, developer |
| `metroid64/src/Hud.*` | ImGui overlay: ROM prompt, notices |
| `metroid64/src/main.cpp` | libultraship bootstrap, input and main loop |
