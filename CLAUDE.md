# Metroid64: instructions for Claude

Read `docs/HANDOFF.md` first: it covers the project's history, what is done, what is open, and how the owner
likes to work. This file covers the rules.

## What this is

NES *Metroid* ported routine by routine from the original 6502 code to C++ (`metroid64/src/game/`), running on
libultraship (the Ship of Harkinian engine, in the `libultraship` submodule) and shown in 2.5D/3D. The port
must play exactly like the original. The lockstep tool (`metroid64/tools/lockstep/`) runs the original on an
emulated NES next to the port, with the same inputs, and fails on the first byte of RAM or picture memory that
differs.

## Hard rules

- **Never commit Nintendo data or the owner's private files:** the ROM (`*.nes`), `metroid.o2r`, the
  disassembly, or the owner's model files. `.gitignore` covers the first two; check `git status` before every
  commit anyway.
- **No guessing at gameplay.** Every behaviour comes from the original code (the community disassembly by
  nmikstas, github.com/nmikstas/metroid-disassembly, and the ROM). When a fact is unknown, find it in the
  code or the ROM, or measure it with the lockstep tool. Don't invent it. Say so when something can't be
  verified.
- **Vanilla stays vanilla.** Every change to how the game plays (no flicker, shots past the edge, Everything
  active, saves instead of passwords, ...) sits behind `Game::Options` / `AllActive::gEnabled` / a CVar, and
  is off in the lockstep tool's comparisons. The vanilla battery must stay all OK.
- **One routine, one function**, named after the disassembly label, with its ROM address in a comment. RAM is
  accessed through the names in `game/Ram.h`. See `docs/PORTING.md`.
- **Work one item at a time.** Finish an item, test it, commit and push it, report, and ask before starting
  the next one.
- **Don't create pull requests** unless asked.
- **No AI model names** in commits, code or docs.

## Remotes and branches

- `https://github.com/MrEveryDay98/Metroid64`, branch `main`, is the project's home. Commit and push there.
- `https://github.com/MrEveryDay98/Shipwright-perpixel-lighting`, branch `claude/focused-allen-mlvvhw`,
  held the same commits during the cloud sessions. If this clone has that remote, keep pushing there too.

## Building

See `README.md` → Building. The test tool needs `-DMETROID64_TOOLS=ON`. A separate build directory for it keeps
things quick:

```
cmake -S . -B build-lockstep -G Ninja -DCMAKE_BUILD_TYPE=Release -DMETROID64_TOOLS=ON
cmake --build build-lockstep --target lockstep
```

On Windows, build from an x64 Native Tools prompt (MSVC, Ninja, vcpkg with `VCPKG_ROOT` set). CI builds Windows
and Linux on every push (`.github/workflows/build.yml`), and its `metroid64-windows` artifact is a ready build.

## Testing before every commit

1. Build the app and the lockstep tool with no errors.
2. Run the battery (about 30 s; it needs the owner's ROM, whose path you ask for):
   ```
   metroid64/tools/lockstep/checks.sh "path/to/Metroid (USA).nes" build-lockstep/lockstep/lockstep
   ```
   It must end with `ALL CHECKS PASSED`.
3. For a change to the game code, also run lockstep on what the change touches: `--password <area,gear,
   missiles>` starts in any area, `--random <seed>` plays randomly, and `--poke` gives items. The options are
   listed at the top of `metroid64/tools/lockstep/main.cpp`. Add a new `--solo` test mode for new optional
   behaviour, as was done for saves and Everything active.
4. Format with `./run-clang-format.sh` (or `run-clang-format.ps1`).
5. Update `README.md` and the `docs/` page for what changed.

## Commits

Write descriptive messages, with a summary line and then what changed and why. End them with the attribution
lines Claude Code gives you.

## Where things are

| Path | What |
| --- | --- |
| `metroid64/src/game/` | The port: one file per part of the original (`Engine.cpp` is bank 7, `Area*.cpp` the area banks, ...) |
| `metroid64/src/game/Options.*`, `AllActive.*`, `Progress.*`, `Explored.*` | The optional changes: options, Everything active, saves, map |
| `metroid64/src/game/data/` | Extracting the ROM into `metroid.o2r` (XML, PNG); bump the data version when the format changes |
| `metroid64/src/Port.*` | Runs the game for the app: frames, sound, saves, save points |
| `metroid64/src/Renderer.*`, `GameView.*`, `MapView.*`, `Hud.*`, `Menu.*` | Presentation, map, overlay, F1 menu |
| `metroid64/src/Model.*`, `Skeleton.*`, `Props.*`, `SavePoints.*` | Models, skeletal animation, mod props and save points |
| `metroid64/editor/` | The in-browser mod editor (plain JS, three.js, JSZip) |
| `metroid64/tools/lockstep/` | The test tool and `checks.sh` |
| `docs/` | `ENGINE.md` (how it runs), `PORTING.md` (how the port is written), `DATA.md`, `MODELS.md`, `example-mod/` |
