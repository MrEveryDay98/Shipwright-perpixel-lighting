# Metroid64 handoff

This is where the project stands, for whoever picks it up next: a new Claude Code session or a person. The rules
are in `CLAUDE.md`.

## The goal

The owner's goal: "accurately recreate the gameplay elements from the base game, with little to no guessing
used". Metroid64 is NES *Metroid* as a native C++ port on libultraship (Ship of Harkinian's engine), in
2.5D/3D, with an ImGui menu, mod support (models, room geometry, XML data in an O2R archive) and a mod editor.
The owner supplies the ROM; `metroid.o2r` is extracted from it on first run.

## How it was built

In this order:

1. A 6502/MMC1/PPU emulator and the lockstep tool, to have the original to compare against.
2. A RAM map, then every bank of the original translated into C++ (`metroid64/src/game/`), following the
   nmikstas disassembly. The whole game is ported: title, intro, all five areas, enemies, bosses, items,
   doors, elevators, the escape and the ending. Lockstep matches the original byte for byte in every area.
3. The game's data moved out of the ROM into typed resources in `metroid.o2r` (XML/PNG), which mods can
   replace.
4. The presentation:
   - The 3D play field from the live name tables, then whole room sections from room meshes, with door
     crossfades and fog.
   - Sprites as cards, a status overlay, and the APU synthesizer for sound.
   - Frame interpolation and smoothed movement.
   - Samus models: display-list flipbooks or Shipwright skeletons and animations, one per form.
   - Room models.
   - Controller remapping.

## The improvement batch (all done; each is optional and off in vanilla comparisons)

1. **No sprite flicker** (`Options::gNoFlicker`).
2. **No slowdown.** The port already plays without the console's lag frames (measured with `lockstep --solo`).
3. **Smooth model animation:** blending between animations and when turning around.
4. **Keep missiles/tanks.** Skipped: the original already keeps them.
5. **Saves.** These replaced passwords:
   - **Super Metroid-style progress saves.** `game/Progress.*` writes `metroid64.sav` (format "M64P"). A save
     keeps the stats, area, energy, missiles, the room and place, and the map. Loading starts that room
     afresh, like a password start.
   - **Where a save puts Samus.** On a save point (Up) it saves where she stands. From the pause menu
     (Select) it puts her at the door she last came into the room by (`Progress::TrackEntry`).
   - **Dying and CONTINUE.** Dying returns to the save; CONTINUE loads it.
   - **Old saves.** Older snapshot saves ("M64S", `game/SaveState.*`) still load. `SaveState` is otherwise
     only used by `lockstep --save-test`.
6. **Super Metroid-style map:** a minimap while playing and the area map on pause (`Explored.*`, `MapView.*`).
7. **Shots fly past the old screen edge** in the wide view (`Options::gShotsPastEdge`).
8. **Everything active.** Every enemy, enemy shot, Zeb pipe, Tourian object and Memu in the section runs, not
   only those on screen (`AllActive.*`, the menu's "outside view" setting 2).

After the batch:

- **Passwords removed** from the app (`Options::gNoPasswords`).
- **Save points.** They come from `metroid64/savepoints.json` in a mod; the original has none.
- **Props.** They come from `metroid64/props.json`.
- **Special-item doors counted in sections.** Tourian went from 1 section to 5; data version 12.
- **The editor** (`metroid64/editor/index.html` + `js/`):
  - It's a standalone page that opens `metroid.o2r`.
  - It lets you browse areas in 3D, move enemies and items, add save points, and import Fast64 exports
    (Shipwright-format display-list XML) as props or room models.
  - It exports a mod zip.
- **Windows fixes:** a startup crash/hang from replacing `metroid.o2r` while it was open (fixed by releasing the
  archive first), and save-load audio.

## Open items and ideas

- **Samus animations.** The Samus model needs animations exported by the owner; the rest pose works. Format:
  `docs/MODELS.md`.
- **Test the new saves in the app.** The newest saves (progress plus entry door) passed the lockstep tests
  but haven't been played in the app yet. Check a pause save, a save-point save, dying, and CONTINUE.
- **Not done yet** (from `README.md`):
  - models for enemies and items;
  - in vertical shafts the view can't extend sideways, because the game keeps only one screen's width there.
- **Don't trust editor exports unseen.** The editor was tested headless with Playwright, so check its
  exports in the app when changing it.

## How the owner works

- One item at a time. After each item: test, commit, push, report briefly, and ask before the next one.
- Accuracy first. Anything that changes gameplay is optional, and vanilla must remain identical.
- The owner plays on Windows. Crash reports come as `Metroid64.log` (libultraship's log, next to the game).
- Explain results plainly: what was done, how it was tested, and what wasn't tested.

## Testing quick reference

- **The battery.** `metroid64/tools/lockstep/checks.sh "<rom>" <lockstep binary>` must print
  `ALL CHECKS PASSED`.
- **Starting points and play.**
  - `--password <area,gear,missiles>` starts in an area: Brinstar `10`, Norfair `11`, Kraid `12`,
    Tourian `13`, Ridley `14`.
  - `--random <seed>` plays randomly.
  - `--poke 0107=09,6878=FF,6879=40,687A=40,6877=06` gives items and energy.
- **Testing optional behaviour.** `--solo` plays the port alone, for optional behaviour: `--progress-test`,
  `--save-test`, `--all-active`, `--no-flicker`, `--shots-past-edge`.
- **Scripted runs of the app** without a person: see the developer environment variables in
  `docs/ENGINE.md`, such as `METROID64_INPUT`.
