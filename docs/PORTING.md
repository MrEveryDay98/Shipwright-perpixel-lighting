# Porting Metroid to C++

Metroid64 recreates Metroid by translating the original game's routines into C++, the way Ship of Harkinian
is built on the Ocarina of Time decompilation. No emulator runs in the game: the port is native code
working on the game's state. The original is used for one thing only: the lockstep test tool, which runs
it on an emulated NES next to the port and checks that both behave identically, frame by frame.

## Ground rules

- **No Nintendo data in the repository.** Code reads tables (rooms, enemy data, graphics, music) from the
  player's ROM at runtime through `Game::Read()` / `Game::RomByte()`. Translated code may contain small
  constants that are part of the logic (thresholds, masks), just as any rewrite would.
- **Same state, same meaning.** Variables keep their original addresses and names (`game/Ram.h`, from the
  community disassembly's `Metroid_Defines.asm`), so the port can be compared byte for byte with the
  original. `Ram.h` gives every variable a `Var`: `ObjAction[x]`, `GameMode == 3`, `Timer3--`.
- **One routine, one function**, named after the disassembly label, with its address in a comment.
- **Faithful, then readable.** The C++ must do exactly what the routine does, including RAM it writes as a
  side effect (temporaries at `$00-$0F` included: later routines read them). Use C++ control flow, not
  goto-per-instruction; keep the original's quirks and bugs. The one exception: a byte at `$00-$0F` that a
  routine only uses to save a register and restore it before returning may be left out.

## How a routine is translated

- Registers become parameters and return values. Only pass what the routine actually reads, and return
  what its callers actually use afterwards (A, X, Y or a flag). Check the callers. When several values come
  back, return a `Regs` (from `game/Original.h`).
- Arithmetic that depends on the carry uses `Adc`, `Sbc`, `Asl`, `Lsr`, `Rol`, `Ror` from `game/Memory.h`.
- Pointer reads (`LDA ($00),y`) are `Read(GenWord00.Word() + y)`. They can point into RAM, cartridge RAM or
  ROM; `Read` handles all three, with `$8000-$BFFF` reading the bank currently mapped.
- Hardware access goes through `game/Hw.h`: `Hw::PpuAddr`, `Hw::PpuData`, `Hw::PpuCtrl`, `Hw::OamDma`,
  `Hw::ApuWrite`, `Hw::JoypadRead`, `Hw::SetPrgBank`, `Hw::SetMmcControl`. These are plain models of the
  video memory, mapper and ports; the renderer and audio output read them.
- Jump tables (`JSR ChooseRoutine` followed by `.word` entries) become a `switch`.
  `ChooseRoutineEffects(table, index)` reproduces the RAM the original leaves behind.
- Stack tricks: a routine that ends its caller (`PLA`/`PLA`/`RTS`) becomes a return value the caller checks.
- A routine that waits for the NMI (`WaitNMIPass`, `ScreenOff`, `ScreenOn`) calls `WaitForNmi()`, which runs
  the NMI work immediately.

## Working incrementally

Every routine is translated now. `Game::Original(addr, regs)` remains for working incrementally on
changes: it stands for a routine that hasn't been translated yet. In the lockstep tool it runs the original
routine on the port's memory (the tool lists every one a run used); in the game it only logs. The only
call left is `AreaHook`'s fallback for an area-bank address no translated code handles, which the game's
data never reaches.

## The lockstep test

```sh
cmake -B build-tools -DMETROID64_TOOLS=ON ... && cmake --build build-tools --target lockstep
lockstep "Metroid (USA).nes" --script 120:08,125:00,200:08,205:00 --sync 650 --frames 3000 --random 7
```

- `--script frame:buttons,...` holds NES buttons (hex: `01` A, `02` B, `04` Select, `08` Start, `10` up,
  `20` down, `40` left, `80` right) from each frame on.
- `--sync n`: the original runs alone until frame `n`, then the port takes over its state.
- `--random seed`: random buttons after the script (Start excluded).
- `--poke addr=val,...`: written into both sides' memory before every frame, to reach parts of the game
  quickly (all items and full energy, an item placed on Samus, a later area).
- `--password area,gear,missiles[,suitless]` (hex): types a password made with the port's own password code
  on the title screen, so the game starts in that area (`10` Brinstar to `14` Ridley's hideout) with that
  gear. Combine with `--random` and `--poke` to play around an area.
- `--picture`: also compares the 2D picture drawn from the port's video memory with the emulated PPU's.
- `--dump addr[-addr],...`: prints the original's bytes at the end, to check a test reached what it was
  meant to (an item collected, a room entered).
- `--check-lag`: also compares after the original's lag frames and prints the differences without failing.
  Some are expected there (joypad, sound and sprite bytes, from the extra NMIs the original ran); anything
  else is a bug the normal run can't see. Heavy frames (loading a room, checking a password) are often lag
  frames, so run with this after translating one.
- It compares RAM (except the scratch bytes `$00-$0F`, `TempX`/`TempY` and the stack), cartridge RAM, name
  tables, palette, pattern tables, sprites, PPU control and the mapped bank after every frame, and prints
  the first differences with variable names.
- Frames where the original lags (its main loop not finished by the next vblank) can't be matched; the
  tool takes the original's state again after them.

## Reference

The translation follows the community disassembly by nmikstas (github.com/nmikstas/metroid-disassembly).
It is not included here.
