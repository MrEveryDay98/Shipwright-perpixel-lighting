# How Metroid64 runs Metroid

Metroid64 does not reimplement Metroid. It runs the original game code from your ROM on an emulated NES
(`metroid64/src/nes/`) and presents what the game draws in 3D (`metroid64/src/Renderer.cpp`). Physics,
enemy behaviour, items, doors, passwords, and timing are all the original game's.

## The emulated NES

| Part | What is emulated |
| --- | --- |
| CPU (`Cpu6502`) | The 2A03's 6502 core: every official opcode with its cycle count and page-crossing penalties, dummy writes of read-modify-write instructions, the `JMP ($xxFF)` page bug, NMI/IRQ/BRK. No decimal mode (the 2A03 has none). |
| Cartridge (`Nes::Machine`) | MMC1 (SNROM, like Metroid): serial shift register, PRG modes 0-3, mirroring control, 8 KiB of PRG RAM at `$6000`, 8 KiB of CHR RAM. |
| PPU | Registers `$2000`-`$2007` with the internal `v`/`t`/`x`/`w` scroll registers, buffered `$2007` reads, palette mirrors, nametable mirroring from MMC1, OAM and `$4014` DMA (513/514 cycles), vblank flag and NMI, sprite 0 hit at scanline granularity. |
| Timing | 262 scanlines of 341/3 CPU cycles each (NTSC). The scroll used by every visible line is recorded. |
| Controller | Standard pad at `$4016` (strobe and serial read). |
| APU | Not emulated yet: writes are ignored, so there is no sound. |

`Nes::Machine::RenderFrame` draws the classic 2D picture (background, sprites, priorities, 8 sprites per
line). Metroid64 uses it for the title, password and ending screens, and when the 3D play field is turned off.

## From emulator state to 3D

- **Play field**: read straight from the nametables, attribute tables, palette and CHR RAM. A background
  tile below pattern `$A0` is solid in Metroid (bank 7 `$E7F9`), so it becomes a block; other tiles become
  cards behind the play plane. Geometry is cached per 64x64-pixel piece of each nametable and rebuilt
  when its tiles, the palette or CHR change, so doors, destroyed blocks and palette fades just work.
- **Camera**: the scroll at the top of the picture, unwrapped across nametable boundaries and
  interpolated between emulated frames.
- **What is shown**: a horizontal room (vertical mirroring) is 512x240 pixels of nametable, a vertical shaft
  256x480. Each distinct piece is drawn once, at the copy nearest the middle of the screen, so the view can
  extend past the NES screen edges without showing the same tiles twice.
- **Sprites**: OAM entries are drawn as flat cards. The game's status display (energy, missiles) is drawn
  as a 2D overlay.

## Who drew a sprite

The game builds its sprite list at `$0200` from the main loop at `$CB2D`. When the game writes a
sprite's Y coordinate, `Emulator` looks at the stack for the main-loop call that is running and tags the
sprite. The tags are copied when the game starts the OAM DMA, so they match the sprites on screen.

| Return address on the stack | Routine | Draws |
| --- | --- | --- |
| `$CB2F` | `$F345` | Enemies. `$4B` holds the enemy slot. |
| `$CB32` | `$D4BF` | Samus's beams, missiles and bombs |
| `$CB35` | `$CC0D` | Samus |
| `$CB50` | `$8B79` (area bank) | Door bubbles |
| `$CB59` | `$E0C1` | Status display |

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

- `METROID64_FRAMES=n` quits after `n` rendered frames, running one emulated frame per rendered frame.
- `METROID64_INPUT=frame:buttons,...` holds NES buttons (hex: `01` A, `02` B, `04` Select, `08` Start,
  `10` up, `20` down, `40` left, `80` right) from the given emulated frame on. `120:08,125:00` presses Start.
- `METROID64_FREEZE_AT=n` stops emulation at frame `n`, for comparing screenshots.
