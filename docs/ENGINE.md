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

On the console, when the main loop has more to do than fits in a frame (loading a room, many enemies), it
finishes in the next frame and the game slows down. The port does the same work, but it can only tell when
the original would have run out of time where the original explicitly waits for the next frame. So
Metroid64 shows less slowdown than the console, mostly in busy rooms, and starts the title screen a few
frames earlier. What happens in the game is the same; only the pacing differs.

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
- Not covered yet: Tourian; enemies from Zeb holes (pipes), which the pipe removes off screen as in the
  original; Memus; enemy shots fired off screen (dropped).
- With it off nothing of this runs; the lockstep tool's `--all-active` plays the port alone with it on.
- **Camera**: the scroll at the top of the picture, unwrapped across nametable boundaries and
  interpolated between game frames.
- **Without the game's data** (no level loaded) the play field is drawn from the name tables instead.
- **Sprites**: OAM entries are drawn as flat cards. The game's status display (energy, missiles) is drawn
  as a 2D overlay.

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
