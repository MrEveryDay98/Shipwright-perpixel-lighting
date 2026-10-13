# How Metroid64 runs Metroid

Metroid64 is a native C++ port of NES *Metroid* (`metroid64/src/game/`), written the way the Ship of
Harkinian is built on the Ocarina of Time decompilation: every routine of the original was translated by hand
into C++, keeping the original's variables at their original addresses and its logic, quirks and bugs.
Physics, enemy behaviour, items, doors, passwords and timing are the original's. Tables, graphics, rooms and
music are read from your ROM at runtime; the repository contains no Nintendo data. Metroid64 then presents
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

## Known difference: slowdown

On the console, when the main loop has more to do than fits in a frame (loading a room, many enemies), it
finishes in the next frame and the game slows down. The port does the same work, but it can only tell when
the original would have run out of time where the original explicitly waits for the next frame. So
Metroid64 shows less slowdown than the console, mostly in busy rooms, and starts the title screen a few
frames earlier. What happens in the game is the same; only the pacing differs.

## From the game to 3D

- **Play field**: read straight from the nametables, attribute tables, palette and CHR RAM. A background
  tile below pattern `$A0` is solid in Metroid (bank 7 `$E7F9`), so it becomes a block; other tiles become
  cards behind the play plane. Geometry is cached per 64x64-pixel piece of each nametable and rebuilt
  when its tiles, the palette or CHR change, so doors, destroyed blocks and palette fades just work.
- **Camera**: the scroll at the top of the picture, unwrapped across nametable boundaries and
  interpolated between game frames.
- **What is shown**: a horizontal room (vertical mirroring) is 512x240 pixels of nametable, a vertical shaft
  256x480. Each distinct piece is drawn once, at the copy nearest the middle of the screen, so the view can
  extend past the NES screen edges without showing the same tiles twice.
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
`Present` notes its area, map position and room number for that name table; custom room geometry (see
[MODELS.md](MODELS.md)) is looked up from that.

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
