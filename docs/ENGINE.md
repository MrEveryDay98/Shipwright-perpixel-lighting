# How Metroid64 runs Metroid

Metroid64 is a native C++ port of NES *Metroid* (`metroid64/src/game/`), written the way the Ship of
Harkinian is built on the Ocarina of Time decompilation: every routine of the original was translated by hand
into C++, keeping the original's variables at their original addresses and its logic, quirks and bugs.
Physics, enemy behaviour, items, doors, passwords and timing are the original's. Tables, graphics, rooms and
music come from your ROM: on first run they are extracted into `metroid.o2r` (see below); the repository
contains no Nintendo data. Metroid64 then presents
what the game draws in 3D (`metroid64/src/Renderer.cpp`). [PORTING.md](PORTING.md) describes how the port
is written and checked.

## What the game talks to

Where the original wrote to the console's hardware, the port updates plain models of it (`game/Hw.h`):

| Part | What it is |
| --- | --- |
| Video memory | Name tables with the cartridge's mirroring, palette, pattern tables (CHR RAM), sprite memory, scroll and the PPU control/mask registers. `Picture.cpp` draws the classic 2D picture from it (background, sprites, priorities, 8 sprites per line); it is checked pixel for pixel against an emulated PPU. |
| Mapper | The MMC1's bank and mirroring registers: which area bank the game reads at `$8000`. |
| Sound | The game's sound engine is translated too; its register writes go to an APU synthesizer (`src/nes/Apu.cpp`: pulse, triangle, noise and DMC channels with the console's mixer and filters). |
| Controller | The buttons held, read the way the game reads the pad. |

The game runs one frame per 60 Hz tick: the vblank (NMI) work, then one pass of the main loop.

## The game's data

`tools/assetmap/gen_assetmap.py` lists, from the disassembly, every run of data in the ROM (addresses and
names only) as `src/game/data/AssetMap.inc`; a few instruction bytes the original reads as data (tables read
past their end) are included. On first run the app copies those blocks out of the ROM into `metroid.o2r`
as XML resources, `metroid/data/<area>/<label>`:

```xml
<DataBlock Version="0" Bank="1" Address="A271" Name="Palette00">
    3F 00 20 0F 22 12 1C 0F 22 12 1C 0F 27 11 07 0F
    ...
</DataBlock>
```

At startup the resources (with any mods on top) are put back where the original kept them, and the game
reads them there; nothing else of the ROM is used, the code being the port's own. The lockstep tool runs the
port on data that went through the same extraction and XML, so the tests cover it. Typed resources (rooms,
structures, palettes, graphics, music) replace these generic blocks one kind at a time.

## Known difference: slowdown

On the console, when the main loop has more to do than fits in a frame (many enemies, loading a room), it
finishes in the next frame and the game slows down. The port does the same work in one frame of the app,
since it can only tell when the original would have run out of time where the original explicitly waits for
the next frame (`WaitNMIPass`: turning the screen off and on, e.g. while an elevator changes area). So
Metroid64 has none of the console's slowdown in play, and starts the title screen a few frames earlier. What
happens in the game is the same; only the pacing differs.

Measured with the lockstep tool (random play from each area's password, 4000 frames): the original lagged
6 to 287 times per run; the port, playing alone (`--solo`), only waited while the screen was off for an
elevator.

## From the game to 3D

- **Rooms**: the 3D view shows the scrolling section Samus is in: the screens the game scrolls through
  without passing a door (`game/data/Layout.cpp`). Horizontal sections end at a door on that side
  (`GetRoomNum`, `$E720`), vertical ones at a map screen with no room. Each section's mesh is made from the
  rooms when the data is extracted (`metroid/meshes/...`, see [DATA.md](DATA.md)) and drawn whole, so the
  view can show past the original screen's edges. Nothing of other sections is drawn.
- **Tiles the game changes**: blasted and regrowing blocks, Mother Brain's room and the like are written to
  room RAM (`$6000`/`$6400`, one room per name table). Where room RAM differs from the room's own tiles,
  that 8x8-tile block of the mesh is remade from room RAM; it keeps what was last seen there after the
  room has scrolled out of the name tables.
- **Doors**: as soon as the door's direction is known, the next section is worked out the way the game will
  scroll (an `$A0` door turns horizontal scrolling vertical, `$A1` keeps it, a shaft's doors turn it
  horizontal) and fades in while the old one fades out. Elevators and new areas switch the same way.
- **Outside the original screen**: the game only runs enemies and items on its 256x240 screen, so by
  default the view darkens everything outside it (Graphics > View > Outside the original screen).
- **Camera**: the scroll at the top of the picture, unwrapped across nametable boundaries and
  interpolated between game frames.
- **Without the game's data** (no level loaded) the play field is drawn from the name tables instead.
- **Sprites**: OAM entries are drawn as flat cards. The game's status display (energy, missiles) is drawn
  as a 2D overlay.

## Everything active (an optional way to play)

Graphics > View > Outside the original screen > **Everything active** changes the game: every enemy of the
section Samus is in keeps running, not only those on the two screens the original keeps loaded
(`game/AllActive.cpp`).

- On entering a section, each of its rooms is loaded the game's own way (`SetupRoom`) in a stand-in, and the
  enemies it places go into a roster with their place in the section. The room loader no longer places
  enemies itself (`GetEnemyData`), except Kraid and Ridley.
- Before each frame, enemies near an edge of the screen that has more of the section past it (centre less
  than 20 pixels inside) leave the game's six enemy slots for the roster, and roster enemies 32 pixels
  inside the screen, on a screen the game has loaded, go into free slots. The game only draws its own
  256x240 screen, so near such an edge the roster draws them whole; at the section's ends the edge is the
  room's and the game keeps them as usual.
- Every other roster enemy runs one frame of the game's own enemy code (`DoOneEnemy`) in a stand-in: the
  game's memory is saved, two screens of the section's room RAM are put around the enemy, the screen
  looks at it, Samus is where she is (or at the far side if she isn't on those screens), and nothing else
  is there; the enemy's state and the sprites it drew are kept and the memory put back. So they move,
  collide with the room and react to Samus like the original enemies.
- Enemies stay where they went, and dead, until Samus leaves the section.
- Doors, power-ups, the elevator and the statues: each screen's are loaded and drawn with the roster (in
  the stand-in), so the whole section shows them, collected items left out. The two screens the game has
  loaded are drawn again from the game each frame (doors opening, items taken, the elevator moving) and
  keep how they last looked when they scroll out. The game's own sprites for them (which only cover its
  screen) aren't drawn meanwhile.
- Enemy projectiles (slots $60-$B0) are handed to the roster and back the same way, and what a roster
  enemy fires in its stand-in joins the roster (run through `DoOneEnemyObject`), so shots fired off screen
  fly on and the game's own don't vanish at the screen's edge.
- Enemies from Zeb holes (pipes) stay in the game's slots: the pipe takes its enemy back once it is off
  screen and sends another. With this on, it keeps it while it is within the 3D view's extra width past the
  edge (`KeepPastEdge`, on a room the game has loaded there), and it is drawn there.
- Tourian: a Metroid latched on Samus has a byte outside its slot (`$77F8 + slot / 16`); latched Metroids
  stay the game's, and the stand-in clears those bytes so none thinks it is on Samus. Rinkas are sent by their
  spawners into any slot not on screen, so they stay the game's.
- Memus need nothing: they hover near the top of the screen and dive at Samus (`$FD25` works from
  `ScrollY`), so they are never off screen; they come from the screen's special items, which the roster
  doesn't change.
- Kraid, Ridley and Mother Brain, the cannons and the Zebetites stay the game's.
- While Samus rides an elevator the roster is dropped, as in a door: the area bank changes on the way before
  the area's enemy tables ($41-$46) do, and an enemy run then would read its frames from the wrong tables
  (the sprite drawing never finds the end of the frame).
- With it off nothing of this runs; the lockstep tool's `--all-active` plays the port alone with it on.

## Saved games

`game/SaveState.cpp` captures the whole game: CPU RAM and cartridge RAM, picture memory (name tables,
palettes, CHR RAM, sprites, PPU registers), the mapper's bank and control, and the presentation's room and
sprite tags; restoring puts all of it back (about 21 KB). The game keeps nothing else between frames, so
play after a load is exactly what it was after the save: the lockstep tool's `--solo --save-test <n>` saves
after n frames, plays on, loads, replays the same input and checks the state comes out the same.

There are no passwords in the app (`Options::gNoPasswords`; the lockstep tool runs with the original's):
CONTINUE on the START/CONTINUE screen sets `Options::gContinueRequested`, for the app to load the save, instead of
going to the password screen; game over (`TitleGameOver`) and quitting from the pause screen (`GoPassword`) go
to the START/CONTINUE screen (`StartContScrn`) instead of showing a password. After a load, the buttons held
then count as held (`Joy1Status`), so the Start that chose CONTINUE doesn't pause the game.

Save points (`SavePoints.cpp`) come from `metroid64/savepoints.json` in the loaded archives: an area, a map
screen, and the middle and floor of the save point in that screen's pixels. Samus is on one when she is on that
screen (`Explored::SamusScreen`), her middle within 12 pixels of its middle and her feet (`ObjectY + ObjRadY`)
within 4 of its floor; pressing Up there saves. The renderer draws them in their section, the map marks them.

The app (`Port`) saves while the game is paused in play (MainRoutine 5), not during the escape, writing
`metroid64.sav` through a temporary file. When the game reaches game over (MainRoutine 4, after the death
animation) and a save exists, it loads the save instead (Game > "After dying, go back to the save"). The sound
chip is reset on a load (the game is paused, so it is silent anyway), and the presentation starts over
(`GameView::Generation`). With Everything active on, the section's roster is built again after a load, so its
enemies are back where their rooms place them.

## The map

`game/Explored.cpp` marks, after each frame of play, the map screen Samus is in: the room on the name table
she's on (`Present::gRooms[ObjectHi]`). It also keeps how that screen scrolled (`ScrollDir` bit 1, not while
a door is scrolling): a room's data doesn't say whether it is part of a shaft or a corridor (the door that
leads in decides), so walls are drawn only for screens Samus has been to, from the section the game scrolls
through from there that way (`FindSection`). The marks are cleared for a new game (`InitializeStats`) and kept in
saved games (version 2 of the format).

`MapView` draws it: each area's screens are those of its sections (`AllSections`); doors and their colour come
from the room's door entries (palette 1 and 3 are blue, the others need missiles: `LoadDoorData`), elevators and
power-ups from the special items table. A red door that was opened and a power-up that was taken are found in
the item history the way `CheckForItem` looks (`CreateItemID`'s ID).

## Shots past the screen edge

The original removes Samus's shots as soon as they leave its 256x240 screen (`KillIfOffScreen`), which the
wider 3D view shows. With Graphics > View > **Shots fly past the screen edge** (on by default;
`Options::gShotsPastEdge`) a shot outside the screen lives on, up to 160 pixels past it along the scrolling,
as long as the name table it is on holds the map screen that is really there (`PastEdgePosition`, from the room
tags): then it moves through that room's tiles, so it hits its walls and blocks, and the game's enemies there.
Where the game hasn't loaded the next room yet (it loads it while scrolling towards it), the shot is removed as
before. Such shots are drawn into `Present::gExtraBuffer` (in screen pixels, outside 0-255) instead of sprite
RAM, and the renderer draws those with the sprites. The lockstep tool's `--solo --shots-past-edge` counts the
frames with shots out there.

## No sprite flicker

Graphics > View > **No sprite flicker** (on by default; `game/Options.h`) draws every frame what the
original only draws every other frame: the elevator and Samus riding it, a statue
blinking before it rises (`$DA1A`) and Samus appearing at the start (`SamusIntro`). On the skipped frames
the same sprite frame is drawn again (`DrawFrame` without `UpdateObjAnim`), so animations keep their pace.
The 2D picture also shows every sprite on a line instead of the NES's 8 (the 3D view never had that limit).
Samus still blinks while she can't be hurt, since that tells the player something.

With it off the game is the original; the lockstep tool's `--solo` plays the port alone and counts the
frames Samus wasn't drawn (with `--no-flicker`, only in doors).

## Who drew a sprite

The game builds its sprite list at `$0200`, one routine after another, from `SpritePagePos` up
(`UpdateWorld`, `$CB2D`). `game/Present.cpp` notes which entries each routine added and tags them; the tags
are copied when the game starts the sprite DMA, so they match the sprites on screen.

| Routine | Draws |
| --- | --- |
| `DrawEnemy` (`$DD8B`, for each enemy) | Enemies, with the enemy's slot (`$4B`) |
| `UpdateProjectiles` (`$D4BF`) | Samus's beams, missiles and bombs |
| `UpdateSamus` (`$CC0D`) | Samus |
| `DrawDoors` (`$8B79`, area bank) | Door bubbles |
| `DisplayBar` (`$E0C1`) | Status display |

## Which room is on screen

Each of the two name tables shows one room of the world map. When `SetupRoom` starts drawing a room,
`Present` notes its area, map position and room number for that name table, and when `EndOfRoom` runs,
that the room is complete in room RAM. The renderer places the section from this (which screen of the
section each name table holds) and reads the changed tiles from room RAM.

Door bubbles have the "behind background" bit. On the NES they show through the door frame's
transparent pixels. Blocks are opaque in 3D, so Metroid64 draws them on the face of the wall instead.

## RAM used by the presentation

| Address | Meaning |
| --- | --- |
| `$1D` | 0 while the game engine runs (1 on the title screen) |
| `$1E` | main routine: 3 playing, 8 Samus appearing |
| `$4B` | object slot being processed |
| `$4D` | Samus's facing: 0 right, 1 left |
| `$0301` | Samus's half height |
| `$0303` | Samus's animation frame |
| `$0305` | Samus's animation: offset into the object animation table at `$8572` |
| `$030C` | bit 0: the nametable Samus is in |
| `$030D` / `$030E` | Samus's Y / X (her centre) |
| `$0106` / `$0107` | energy (BCD) |

Samus's animations, by their `$0305` value:

| Value | Animation | Model animation name |
| --- | --- | --- |
| `$00` | run | `Run` |
| `$04` | facing the screen | `Front` |
| `$07` | stand | `Stand` |
| `$0C` | jump | `Jump` |
| `$0E` | somersault | `Spin` |
| `$16` | morph ball | `Morph` |
| `$20` | jump while shooting | `JumpShoot` |
| `$22` | run while shooting | `RunShoot` |
| `$27` | stand aiming up | `StandUp` |
| `$37` | run aiming up | `RunUp` |
| `$3B` | jump aiming up | `JumpUp` |

## Developer switches

- `METROID64_FRAMES=n` quits after `n` rendered frames, running one game frame per rendered frame.
- `METROID64_INPUT=frame:buttons,...` holds NES buttons (hex: `01` A, `02` B, `04` Select, `08` Start,
  `10` up, `20` down, `40` left, `80` right) from the given game frame on. `120:08,125:00` presses Start.
- `METROID64_FREEZE_AT=n` stops the game at frame `n`, for comparing screenshots.
