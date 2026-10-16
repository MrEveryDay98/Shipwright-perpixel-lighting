# Metroid64

NES *Metroid* in 2.5D, built on [libultraship](https://github.com/Kenix3/libultraship), the engine behind
Ship of Harkinian (Shipwright).

The game is a native C++ port of the original: every routine was translated from the original 6502 code, keeping
its variables, logic and quirks, and checked frame by frame against the original running on an emulator (a
test tool, not part of the game). So every physics rule, enemy behaviour, item and door is the original's.
Metroid64 changes how the game is shown: every solid 8x8 background tile becomes a textured 3D block, seen
through a perspective camera that follows the game's own scrolling, and mods can replace Samus and whole rooms
with models. See [docs/ENGINE.md](docs/ENGINE.md) and [docs/PORTING.md](docs/PORTING.md).

This project began as a fork of Shipwright. All of the Ocarina of Time code, its asset pipeline (Torch) and its
tooling have been removed. What's left is libultraship (window, Fast3D renderer, input, ImGui, archives) and the
Metroid64 game in [`metroid64/`](metroid64).

## You need your own ROM

Metroid64 ships **no Nintendo data**. On first run it extracts the game's data (rooms, graphics, music, tables)
from your own `Metroid (USA).nes` into `metroid.o2r` next to the game, the way Ship of Harkinian builds its
archive. After that the ROM isn't needed. Only that exact dump is accepted (its PRG ROM is checked against a
CRC32).

You can provide the ROM in any of these ways:

- drag and drop the `.nes` file onto the game window,
- pass it on the command line: `metroid64 "Metroid (USA).nes"`,
- put it next to the executable as `metroid.nes` (any `.nes` file in that folder is tried).

Giving a ROM again (or a new Metroid64 version that changes the data format) remakes `metroid.o2r`. The data
is XML inside the archive (world map, rooms, structures and more); a mod can replace any of those files.
See [docs/DATA.md](docs/DATA.md).

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

The game sees a standard NES controller. Press **F1** for the menu, then **Controls → Remap controls** for the
controller configuration window (like Ship of Harkinian's): each NES button can be bound to any keys,
gamepad buttons or stick directions, per controller port, with sensitivity and deadzone for the stick.
**Set Defaults** puts back these:

| NES button | Does | Keyboard | Gamepad |
| --- | --- | --- | --- |
| D-pad | move, aim up, morph (down) | arrow keys, or W A S D (stick) | D-pad or left stick |
| A | jump | X | A (bottom) |
| B | shoot, or lay a bomb as the ball | Z | B or X |
| Select | toggle missiles; while paused, save the game | Right Shift or Backspace | Back or left trigger |
| Start | start, pause | Enter | Start |

Bindings saved by an earlier version stay as they were until you press Set Defaults.

## Saving

Metroid64 saves the game instead of using passwords. `metroid64.sav` (next to the game's settings) holds where
you are, with your energy, missiles and items, and the map.

- **Pause menu:** pause with Start; **Save game** (or Select) saves where you are. Game → "Save anywhere from
  the pause screen" turns this off, leaving only save points.
- **Save points:** stand on one and press Up. A mod adds them with `metroid64/savepoints.json` (see
  [docs/example-mod/savepoint](docs/example-mod/savepoint)); the original game has none, so there are none
  until a mod or the editor adds them. They're drawn as a glowing pad and shown on the map with an "S".
- **Continue:** CONTINUE on the START/CONTINUE screen loads the save (Game → Load saved game does too).
- **Dying:** you go back to the save, paused, like Super Metroid (Game → "After dying, go back to the save");
  with no save, back to the START/CONTINUE screen.
- There are no passwords: game over and quitting from the pause screen (A + Up on controller 2) go back to the
  START/CONTINUE screen. Saving isn't possible during the escape.

## Map

Like Super Metroid's: a minimap of the screens around Samus in the top right corner while playing, and the
whole area when paused. Screens Samus has been to are pink, with walls where the room ends, doors (red ones
need missiles until opened), elevators (arrows) and a dot for each power-up not taken yet; Samus is the yellow
square. Game → Map has switches for both, and for showing the screens not visited yet (in blue, like after a
map station). The map is kept in saved games.

## Editor

`metroid64/editor/index.html` is a mod editor that runs in the browser: open your `metroid.o2r`, move enemies and
power-ups, add save points, import Fast64 models as props or room models, and export a mod. See
[metroid64/editor/README.md](metroid64/editor/README.md).

## Menu (F1)

- **Game**: Open ROM, Reset (power cycle), Freeze game, Save game, Load saved game, "After dying, go back to the save", the map's switches, Quit.
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

- The whole game is ported: title and intro, passwords, all five areas with their enemies and bosses, items, doors, elevators, the escape and the ending. The lockstep test matches the original in every area (random play from passwords) and in the ending.
- The play field in 3D from the live nametables, including scrolling, doors, destroyed blocks and palette effects.
- Sprites as flat cards, with door bubbles placed on the walls and the status display as an overlay.
- Sound and music: the game's sound engine, played through an APU synthesizer (volume and mute in the Audio menu).
- Room geometry: a mod can replace any room of the world map with its own model. See [docs/MODELS.md](docs/MODELS.md).
- Model support: Samus can be replaced by a 3D model, either a flipbook of display lists or a skeleton with animations in Shipwright's formats, animated from the game's own animation state. See [docs/MODELS.md](docs/MODELS.md).

## Not done yet

- The console's slowdown in busy rooms is not reproduced (see [docs/ENGINE.md](docs/ENGINE.md)).
- Models for enemies and items.
- In vertical shafts the view doesn't extend past the NES screen's left and right edges, because the game only keeps one screen's width there.

## Developer notes

- [docs/ENGINE.md](docs/ENGINE.md) covers how the game runs, the RAM addresses and sprite tagging the 3D view relies on, and developer environment variables for scripted headless runs.
- [docs/PORTING.md](docs/PORTING.md) covers how the port is written and the lockstep test that checks it against the original.

Source layout:

| File | Purpose |
| --- | --- |
| `metroid64/src/game/` | The game: the port of Metroid's code, its RAM, ROM access and hardware models |
| `metroid64/src/game/data/` | The game's data as resources: extraction from the ROM, XML, building what the game reads |
| `metroid64/src/GameData.*` | `metroid.o2r`: making it on first run, the resource types, loading the game's data |
| `metroid64/src/Port.*` | Runs the game for the app: frames, sound, scroll and sprite ownership for the renderer |
| `metroid64/src/Picture.*` | The 2D picture from the game's video memory |
| `metroid64/src/nes/Apu.*` | APU synthesizer for the game's sound |
| `metroid64/src/Renderer.*` | 3D presentation of the game; 2D screens |
| `metroid64/src/Model.*` | Model manifests and drawing |
| `metroid64/src/Skeleton.*` | Skeleton, limb and animation resources (Shipwright's formats), posing and limb drawing |
| `metroid64/src/Menu.*` | F1 menu: graphics, frame rate, controls, developer |
| `metroid64/src/Hud.*` | ImGui overlay: ROM prompt, notices |
| `metroid64/src/main.cpp` | libultraship bootstrap, input and main loop |
| `metroid64/tools/lockstep/` | Test tool: the original on an emulated NES (6502, MMC1, PPU) next to the port |
