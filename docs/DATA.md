# The game's data

On first run Metroid64 extracts the game's data from your ROM into `metroid.o2r` (next to the game). It is
XML, in libultraship resources, so a mod can replace any of it: put a file with the same path in a mod
(an `.o2r`/`.zip` archive or a folder in `mods/`). Mods are loaded after `metroid.o2r`, so their files win.

Numbers in these files are hexadecimal. `<area>` is `brinstar`, `norfair`, `tourian`, `kraid` or `ridley`.

## Levels

### World map: `metroid/world/map`

```xml
<WorldMap Version="0" Address="A53E">
    <Row Y="0E">FF 08 17 09 14 13 ...</Row>
    ...
</WorldMap>
```

32 rows of 32 room numbers, one per screen of the world map (`FF`: nothing there). Which area a screen
belongs to depends on where Samus came from (the elevators), as in the original. Samus starts in Brinstar
at x 03, y 0E: room 09.

### Rooms: `metroid/rooms/<area>/room_<NN>`

```xml
<Room Version="0" Area="brinstar" Number="09" Palette="03" Address="A5B9" Size="36">
    <Object X="0" Y="0" Structure="11" Palette="01"/>
    ...
    <Items>
        <Enemy Slot="5" Type="05" X="5" Y="2"/>
        <Door Side="Right" Palette="1"/>
    </Items>
</Room>
```

- `Palette`: the attribute palette the room starts filled with.
- `Object`: a structure placed at X, Y in 16x16-pixel blocks (16 across, 15 down), drawn with that palette.
  Objects are drawn in order. `<Pause/>` makes the game finish the room on the next frame.
- `Items` (optional): what the room loads besides its background:
  - `Enemy Slot Type X Y [Tough="1"] [MiniBoss="1"]`: an enemy in slot 0-5. Types are per area.
  - `Door Side="Left|Right" Palette`: with palette 1 or 3 the door is always there; with 0 or 2 it is a
    one-time door, remembered in the item history once opened (the loader is `LoadDoorData` in `Room.cpp`).
  - `Elevator Info`, `Statues`, `ZebHole Slot Type X Y`.
- `Address`, `Size`: where the room was in the ROM. Leave them: a room that still fits goes back there,
  a bigger or new room is placed in free space automatically.

### Structures: `metroid/structures/<area>/struct_<NN>`

```xml
<Structure Version="0" Area="brinstar" Number="02" Address="ACB0" Size="25">
    <Row X="0">04 05</Row>
    ...
</Structure>
```

Rows of macros, top to bottom; `X` shifts a row right (in macros). A row has 1 to 16 macros.

### Macros: `metroid/macros/<area>`

```xml
<Macros Version="0" Area="brinstar" Address="AEF0" Size="272">
    <Macro Number="00">F1 F1 F1 F1</Macro>
</Macros>
```

2x2 blocks of background tiles: top left, top right, bottom left, bottom right. A tile below `A0` is solid.

### Special items: `metroid/items/<area>`

```xml
<SpecialItems Version="0" Area="brinstar" Address="A3D6" Size="107">
    <Screen X="0F" Y="02">
        <PowerUp Type="05" X="7" Y="3"/>
    </Screen>
    ...
</SpecialItems>
```

What a map screen holds besides its room: the game checks the screen's entry whenever the room there is
loaded. Items:

- `PowerUp Type X Y`: types 0 bombs, 1 high jump, 2 long beam, 3 screw attack, 4 Maru Mari, 5 varia,
  6 wave beam, 7 ice beam, 8 energy tank, 9 missiles. Unique ones are remembered once taken (item history).
- `Enemy Index Type X Y` (`Index`: the enemy slot), `Memus`, `Elevator Info`, `Door Info`, `Palette`.
- Tourian: `Cannon Index Info`, `MotherBrain`, `Zebetite Index`, `Rinka Index`.

## Graphics: `metroid/graphics/<name>` and `metroid/graphics/<name>.png`

The tiles the game copies into the pattern tables, one resource per block the graphics loaders use (the
`GFXInfo` table at `$C6E0`, read by `LoadGFX` in `Engine.cpp`). The resource says where the block goes and
names its image:

```xml
<Graphics Version="0" Bank="6" Address="8000" Name="GFXSuitedSamus" Tiles="154" Image="metroid/graphics/GFXSuitedSamus.png"/>
```

The image is a PNG of the tiles, 16 per row (128 pixels wide, 8 pixels high per row of tiles), left to right
then top to bottom; slots after the last tile are ignored. Each pixel is a tile pixel's 2-bit value: the
extractor writes 2-bit indexed PNGs whose palette is four greys (index 0 black, 1 dark grey, 2 light grey,
3 white), so the indices are the values. Which colours they become in the game depends on the palette the
game draws the tile with (value 0 is transparent in sprites).

To change the graphics, edit the PNG (in a mod: the same path, `metroid/graphics/<name>.png`; the
`<Graphics>` file only needs to be in the mod if it changes) with any image editor. Saved back as:

- an indexed PNG with at most 4 palette entries: the index is the value (keep the palette order);
- anything else (greyscale, RGB, RGBA, indexed with a bigger palette, any bit depth): the nearest of the
  four greys (0, 55, AA, FF; a colour counts as the average of its red, green and blue), and fully
  transparent pixels are 0.

Interlaced PNGs aren't read.

- `Bank`, `Address`: where the block was in the ROM. They identify the block: the `GFXInfo` entries that
  load from there load this resource. Leave them.
- `Tiles` (decimal, 1-512): how many tiles the block has. Unchanged, the block goes back where it was. With
  more tiles, put them in the image too: the block is placed in free space (in its bank if there is room,
  else in any of banks 0-6) and the `GFXInfo` entries that loaded the whole block load all of it from there.
  With fewer, those entries load fewer. Entries that load only the first part of a block (the font is loaded
  partly in places) keep their length. The tiles are copied to the same place in the pattern tables as
  before, so more tiles overwrite the tiles after them there (which the game may load from another block),
  and must not run past the end of the pattern tables (`$2000`): that is refused.

The blocks, with the disassembly's labels for bank 6 and names after its comments for the others:

| Name | Bank | Address | Tiles | Loaded |
|------|------|---------|-------|--------|
| `GFXSuitedSamus` | 6 | 8000 | 154 | title, game start |
| `GFXIntroEnd` | 6 | 89A0 | 16 | title, game start |
| `GFXMetroidTitle` | 6 | 8BE0 | 80 | title |
| `GFXSuitlesSamus` | 6 | 90E0 | 123 | game start without the suit |
| `GFXExPoint` | 6 | 9890 | 1 | Tourian |
| `GFXBlankTiles` | 6 | 9980 | 4 | with every set |
| `GFXBrinstar1` | 6 | 9DA0 | 21 | Brinstar |
| `GFXBrinstar2` | 6 | 9EF0 | 128 | every area |
| `GFXNorfair1`, `GFXNorfair2` | 6 | A6F0, A950 | 38, 7 | Norfair |
| `GFXTourian1` | 6 | A9C0 | 46 | Tourian, Kraid, Ridley |
| `GFXTourian2`, `GFXMthrBrnRoom` | 6 | ACA0, B2A0 | 96, 9 | Tourian |
| `GFXTourian3` | 6 | B330 | 12 | Kraid |
| `GFXTourian4` | 6 | B3F0 | 12 | Ridley |
| `GFXFont` | 6 | B4C0 | 64 | title, password screen, game start, Tourian (part of it) |
| `GFXTheEndFont` | 1 | 8D60 | 64 | ending |
| `GFXBrinstarEnemies` | 1 | 9160 | 64 | Brinstar |
| `GFXNorfairEnemies`, `GFXTourianEnemies` | 2 | 8D60, 9160 | 64 | Norfair, Tourian |
| `GFXKraidEnemies`, `GFXRidleyEnemies` | 3 | 8D60, 9160 | 64 | Kraid, Ridley |
| `GFXSamusEnd` | 4 | 8D60 | 82 | ending |
| `GFXMiscBank4` | 4 | 9360 | 32 | Kraid |
| `GFXMiscBank5` | 5 | 8D60 | 69 | Brinstar, Norfair, Kraid, Ridley |
| `GFXJapaneseFont` | 5 | 91B0 | 48 | Tourian |

Bytes next to these blocks that the loaders don't copy (unused tiles, the title's star data) stay in data
blocks.

## Everything else: `metroid/data/<bank>/<name>`

Data without a type of its own yet is kept as the bytes of a table, named after the disassembly's label:

```xml
<DataBlock Version="0" Bank="1" Address="A271" Name="Palette00">
    3F 00 20 0F 22 12 1C ...
</DataBlock>
```

A block goes back exactly where it came from, so a mod can change its bytes but not its size. Folders:
`title` (bank 0: title, passwords, ending), `brinstar`, `norfair`, `tourian`, `kraid`, `ridley` (the area
banks: palettes, enemy data, music), `graphics` (bank 6) and `engine` (bank 7). Palettes, enemy data, music
and text are getting types of their own next.

## How it fits together

`tools/assetmap/gen_assetmap.py` generates the list of data in the ROM (addresses and names, from the
community disassembly) as `src/game/data/AssetMap.inc`. `src/game/data/` extracts the resources, reads and
writes their XML, and builds what the game reads from them: every piece at the address the original used,
so the port's translated code reads it unchanged. The lockstep test runs the port on data that went through
the extraction and the XML, so it checks all of this against the original.
