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

### Area settings: `metroid/areas/<area>`

```xml
<AreaInfo Version="0" Area="brinstar"
    Unused95CC="FF"
    MusicFlag="01"
    EnemyDamageLo="80"
    EnemyDamageHi="00"
    ItemMusicRooms="2B 2C 28 0B 1C 0A 1A"
    StartMapX="03"
    StartMapY="0E"
    StartScreenY="B0"
    .../>
```

- `MusicFlag`: the area's bit in the music flags (which song plays).
- `EnemyDamageLo`/`Hi`: base damage the area's enemies do (BCD, like the energy counter).
- `ItemMusicRooms`: room numbers that play the item room music.
- `StartMapX`/`StartMapY`/`StartScreenY`: where Samus appears when the game starts in the area.
- `PalToggle`, `ExplodeAnim`, `PieceAnim`, `FireballAnims`, `FireballEndAnims`, `MemuEnemy`: values the
  enemy code and the area start use (animation numbers, the Memus' enemy type).
- `Unused...`: bytes the game never reads.

## Palettes: `metroid/palettes/<area>`

```xml
<Palettes Version="0" Area="brinstar" Table="9560" Count="28">
    <Palette Number="00" Address="A271" Size="36">
        <Write Address="3F00">0F 22 12 1C 0F 22 12 1C ...</Write>
    </Palette>
</Palettes>
```

The palettes the game switches between in an area (the room's palette, flashes, Samus's suits...): palette
N is what the game loads when `PalDataPending` is N+1. Each is a list of writes to video memory:
`3F00`-`3F0F` are the four background palettes, `3F10`-`3F1F` the four sprite palettes, four NES colour
numbers each. `<Fill Address Count Value/>` writes one value Count times; `Vertical="1"` steps down a column
of the name table instead of across. A write holds at most 3F bytes. Palettes that are the same are stored
once.

Area `title` (bank 0) holds the palettes of the title, password and ending screens.

## Screens: `metroid/screens/<name>`

The title, password and ending screens' text and layouts, as writes to video memory (values are tile
numbers; the tiles are in the graphics):

- `intro` (the title screen), `password_title`, `game_over`, `start_continue`, `password_please`,
  `end_screen`: one string the game uses at its address, so it can't grow past its `Size`.
- `ending_palettes`, `end_messages_0`, `end_messages_1`, `credits`: tables of strings, as `<String Number>`
  elements; these can grow (they move to free space).
- `error_message`, `error_blank`: a row of 15 tiles the game copies itself (`<Row>`); they keep their length.

```xml
<Screen Version="0" Name="credits" Bank="0" Table="A291" Count="44">
    <String Number="00" Address="A2E9" Size="18">
        <Write Address="202C">11 0A 12 FF 22 1E 14 0A 16 12</Write>
        <Write Address="23C0">00</Write>
    </String>
</Screen>
```

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

## Music and sound effects

The sound engine is in each of banks 0-5 with its own copy of every table, so these resources are per
bank: `<bank>` is the bank's folder (`title`, `brinstar`, `norfair`, `tourian`, `kraid`, `ridley`). A bank
has the songs that can play while it is mapped: in all of them `tourian`, `item_room`, `fade_in` and
`power_up`; besides, `title` has `intro` and `ending`, `brinstar` `brinstar`, `norfair` `norfair`, `tourian`
`escape` and `mother_brain`, and both `kraid` and `ridley` have `kraid` and `ridley`.

### Songs: `metroid/music/<bank>/<song>`

```xml
<Song Version="0" Bank="brinstar" Name="item_room" Address="BDCD" Size="42" Repeat="FF" NoteLengths="0B"
      TriangleCounter="03" SQ1Duty="92" SQ2Duty="96" SQ1Envelope="0" SQ2Envelope="0">
    <!-- L0-LF with NoteLengths 0B, in frames: 6 12 24 48 96 36 72 18 16 8 3 16 7 14 28 56 -->
    <Part Channel="Triangle">
        |:8 L0 E4 F4 F#4 G4 G#4 G4 F#4 F4 L6 - :|
    </Part>
    <Part Channel="SQ1">
        L8 -
    </Part>
    <Part Channel="SQ2">
        L3 - L2 A#6 - F6 - A6 - C#6 L4 - L2 C6 - F#6 - B6 L3 - L2 F7 - D7 L3 - end
    </Part>
</Song>
```

Settings (the song's entry in the engine's `InitMusicTbl`, and its init routine):

- `Repeat`: `00` plays once (fade in, power up, intro, ending), anything else starts over at the end.
- `NoteLengths`: where `L0` is in the note lengths (below): `00`, `0B` or `17` in the original. The comment
  lists what `L0`-`LF` then are, in frames.
- `TriangleCounter`: low digit not 0: every triangle note sounds that many quarter frames; else high digit
  not 0: until the next note; `00`: as long as the note (at most 15 frames).
- `SQ1Duty`, `SQ2Duty`: the square channels' duty and volume (APU register $4000/$4004). The engine has six
  init routines, so only their pairs work: `92 96`, `B6 F6`, `F4 F4`, `34 34`, `B3 B3`, `F5 F6`.
- `SQ1Envelope`, `SQ2Envelope`: the volume envelope (1-5, below) of each note; `0`: none, the duty/volume
  above throughout.

Each `Part` is the data from where its channels (`SQ1`, `SQ2`, `Triangle`, `Noise`; one or more) start.
Parts are stored one after the other in this order: a channel reads on into the next part if it doesn't
end first, and the original relies on that (above, SQ1 rests one long note and then plays SQ2's notes).
A channel without a part is silent. The data, one byte per word:

- A note: `C4`, `A#3` (`Bb3` works too), from `A1` to `F7` (the notes of the note table: no `D#2`, no
  `D#7`/`E7`). The triangle sounds an octave lower than the name. `-`: a rest.
- On the noise channel the notes are drum beats `drum0`-`drum3` (the `DrumBeat` sound effects).
- `L0`-`LF`: the length of this and the next notes (from the note lengths, at the song's offset). The word
  after a length is always played as a note, whatever it is.
- `|:N` ... `:|`: repeat, N times in all (decimal, 0-62; 0 means 256).
- `end`: the end of the song (on any channel: all stop, or start over if it repeats).
- `$XX`: any other byte as it is (the original has a few: odd notes, and on Norfair's noise channel a
  leftover copy of other notes it never gets to).

`Address` and `Size` are where the data was in the ROM: a song that still fits goes back there, a bigger
one is placed in free space in the same bank and the engine's pointers to it are updated. The engine reads
a channel's data with an 8-bit index, so a channel can only reach 256 bytes from where it starts (repeats
included). A channel that runs past the song's last part without an `end` reads whatever follows in the
bank, as in the original (Tourian's square and triangle loops are longer than the song).

### Note table, note lengths, volume envelopes: `metroid/music/<bank>/notes`, `lengths`, `envelopes`

```xml
<NoteTable Version="0" Bank="brinstar" Address="BE77">
    <Note Name="A1" Period="7F0"/>
    ...
</NoteTable>
<NoteLengths Version="0" Bank="brinstar" Address="BEF7">
    <Lengths Offset="00">4 8 16 32 64 24 48 12 11 5 2</Lengths>
    ...
</NoteLengths>
<VolumeEnvelopes Version="0" Bank="brinstar" Address="BCB0">
    <Envelope Number="1" Address="BCBA" Size="11">01 02 02 03 03 04 05 06 07 08 FF</Envelope>
    ...
</VolumeEnvelopes>
```

- The note table has the APU timer period of each of the 64 notes (frequency: 1789773 / 16 /
  (Period + 1)). `Name` is the note's name in songs; changing a period retunes that note in every song of
  the bank.
- The note lengths are in frames (decimal), 34 in all; a song's `NoteLengths` picks where its `L0` is.
- The 5 volume envelopes are the square channels' volume for each frame of a note, from the second value
  on; `FF` holds the channel's own duty/volume from there, `F0` silences it. An envelope that grows is moved
  to free space.

### Sound effects: `metroid/sfx/<bank>/<name>`

```xml
<SoundEffect Version="0" Bank="brinstar" Name="Jump" Address="B251" Channel="Square"
    Duty="2" Halt="0" ConstantVolume="1" Volume="5" Sweep="1" SweepPeriod="0" SweepNegate="1"
    SweepShift="4" Period="140" Length="17">
</SoundEffect>
<SoundEffectTable Version="0" Bank="brinstar" Name="SpitFlamesTable" Address="B4FB">
    12 13 14 15 16 17 18 19 1A 1B 1C 1D 1B 1A 19 17 16 15 14 12
</SoundEffectTable>
```

`SoundEffect`: the APU registers a sound starts with, by field: square channels `Duty`, `Halt` (length
counter halt), `ConstantVolume`, `Volume`, `Sweep`, `SweepPeriod`, `SweepNegate`, `SweepShift`, `Period`
(11-bit timer), `Length` (length counter index); triangle `Control`, `LinearCounter`, `Period`, `Length`;
noise `Halt`, `ConstantVolume`, `Volume`, `Mode`, `Period` (0-F), `Length`. `Unused...` keeps bits of
registers that do nothing when they aren't 0. How long a sound plays and how it changes is in the engine's
code. The `DrumBeat0`-`3` entries are the music's drum beats (no $400D byte).

`SoundEffectTable`: the values some sounds step through: `SpitFlamesTable`, `BossHitTable`,
`MissilePickupTable`, `IceBeamTable`, `WaveBeamPeriodTable`, `WaveBeamVolumeTable` (the comment in each
says which register). Sound effects and their tables keep their size and address.

## Everything else: `metroid/data/<bank>/<name>`

Data without a type of its own yet is kept as the bytes of a table, named after the disassembly's label:

```xml
<DataBlock Version="0" Bank="1" Address="A271" Name="Palette00">
    3F 00 20 0F 22 12 1C ...
</DataBlock>
```

A block goes back exactly where it came from, so a mod can change its bytes but not its size. Folders:
`title` (bank 0: title, passwords, ending), `brinstar`, `norfair`, `tourian`, `kraid`, `ridley` (the area
banks: enemy data), `graphics` (bank 6) and `engine` (bank 7). Enemy data and the remaining
small tables are getting types of their own next.

## How it fits together

`tools/assetmap/gen_assetmap.py` generates the list of data in the ROM (addresses and names, from the
community disassembly) as `src/game/data/AssetMap.inc`. `src/game/data/` extracts the resources, reads and
writes their XML, and builds what the game reads from them: every piece at the address the original used,
so the port's translated code reads it unchanged. The lockstep test runs the port on data that went through
the extraction and the XML, so it checks all of this against the original.
