# Metroid (USA) ROM notes

These are the facts `metroid64/src/Rom.cpp` relies on. Addresses are CPU addresses inside a bank. The ROM is
iNES, mapper 1, eight 16 KiB PRG banks and no CHR ROM (graphics are copied into CHR RAM).

- Banks 1-5 hold the area data: 1 Brinstar, 2 Norfair, 3 Tourian, 4 Kraid, 5 Ridley.
- Bank 7 is fixed at `$C000`.

The PRG CRC32 accepted is `0x70080810`.

## World map

Bank 0 `$A53E` holds a 32x32 grid of room numbers, with `$FF` meaning empty.

The map does not record which area a screen belongs to, because the game only tracks the current area.
`kAreaLayout` in `Rom.cpp` assigns screens to areas. It was derived from flood fills over door and elevator
connectivity, then checked against a reference map. Lower-case letters in that table mark screens that use the
area's alternate background palette.

## Area bank header (same layout in banks 1-5)

| Address | Contents |
| --- | --- |
| `$9598` | pointer: special item table |
| `$959A` | pointer: room pointer table |
| `$959C` | pointer: structure pointer table |
| `$959E` | pointer: macro definitions (4 tile numbers per 16x16 macro) |
| `$95D7` / `$95D8` | start map X / Y |
| `$860B` | Samus/object frame pointer table (106 frames) |
| `$86DF` | object placement tables (signed y,x offset pairs) |

## Rooms

Each room is laid out as follows:

1. One attribute byte.
2. Objects of 3 bytes each, `[yx position in macros][structure][palette]`. The object list ends with `$FD` (entities follow) or `$FF` (room ends). `$FE` is a no-op.
3. Entities, until `$FF`. The low nibble of the first byte gives the type:
   - `1` enemy: `[slot|1][type][yx]`
   - `2` door: `[2][info]`. The high nibble of `info` is `A` for the right edge and `B` for the left; the low two bits are the colour (0 red, 1 blue, 2 orange, 3 Tourian).
   - `4` elevator: `[4][info]`
   - `6`: 1 byte
   - `7`: 3 bytes

A structure is a list of rows, ending with `$FF`. Each row is `[x offset << 4 | count]` (count 0 means 16) followed by that many macro numbers.

## Special items

The table is a linked list of map rows: `[y][next row pointer]`, terminated by `$FFFF`.

Each row is a linked list of map columns: `[x][offset to next column or $FF]`, followed by typed entries and a
terminating `$00`. The entry types used are:

- `2` power-up: `[2][item][yx]`. Items are 0 bombs, 1 high jump, 2 long beam, 3 screw attack, 4 maru mari, 5 varia, 6 wave beam, 7 ice beam, 8 energy tank, 9 missiles.
- `4` elevator: `[4][info]`. Bit 7 set means the elevator goes up. The low nibble is the destination area, and `$F` is the ending.
- `9` door: `[9][info]`, with the same format as room doors.

## Graphics

The GFX loading table is at bank 7 `$C6E0`. Its 7-byte entries are `[bank][source][PPU destination][length]`.

The area init code loads these entries:

| Area | Entries |
| --- | --- |
| Brinstar | `03 04 05 06 19 16` |
| Norfair | `04 05 07 08 09 19 16` |
| Tourian | `05 0A 0B 0C 0D 0E 1A 1C 19 16` |
| Kraid | `04 05 0A 0F 10 11 19 16` |
| Ridley | `04 05 0A 12 13 19 16` |

Entry `00` (Samus and item sprites) is loaded first. Background tiles use pattern table `$1000`.

Palettes are PPU strings (`3F 00 len` header, then data):

| Area | Main (32 bytes) | Alternate (16 bytes) |
| --- | --- | --- |
| Brinstar | bank 1 `$A274` | `$A2B0` |
| Norfair | bank 2 `$A17B` | `$A1B7` |
| Tourian | bank 3 `$A71B` | none |
| Kraid | bank 4 `$A158` | none |
| Ridley | bank 5 `$A0EE` | `$A12A` |

## Tile collision

These rules come from bank 7 `$D651`, `$E7F9` and `$E9C6`:

- Patterns `>= $A0` are passable.
- Patterns below `$70` are solid. In Brinstar the limit is `$80` instead.
- Patterns between that limit and `$97` are solid but destructible (they regrow).
- Patterns `$98`-`$9F` are solid.

## Samus frames

Each frame is laid out as follows:

1. A header byte: bits 4-7 are flags (`$40` mirrors the placement), bits 0-3 select the placement table.
2. Two size bytes.
3. Tile numbers, until `$FF`. `$FD n` sets the sprite attributes for the following tiles: palette, h/v flip. `$FE` skips a placement slot.

The useful frames are:

| Frames | Pose |
| --- | --- |
| 0 | stand |
| 4, 5, 16 | run |
| 8 | stand and shoot |
| 13-15 | run and shoot |
| 17 | jump |
| 19-22 | somersault |
| 24, 26 | morph ball |
| 43 | aim up |
