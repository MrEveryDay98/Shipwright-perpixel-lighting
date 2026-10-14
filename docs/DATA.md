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

## Enemies: `metroid/enemies/<area>`

```xml
<Enemies Version="0" Area="brinstar" MoveSetArea="30">
    <Enemy Type="00" HitPoints="08" Delay="08" Timer="01" Distance="00" Flags="01" Flags2="64"
           ExplodeAnim="27 27" RunAnim="05 05" TurnAnim="05 05" TurnAnimRewind="00" MoveSet="00" Moves="01 02"/>
    ...
    <Move Number="00" Script0="00" Script1="01" AccelY="127" AccelX="0" SpeedY="-10" SpeedX="0"/>
    ...
    <Projectile Type="00" Anim="00 00" OffsetX="12 -12" OffsetY="-12"/>
    <FireballPath Number="00" Script="14"/>
    <Script Number="02" Address="97F5" Size="75">
        <Step Count="02" Y="-7" X="2"/>
        ...
        <ResetCount/>
        <Restart/>
    </Script>
</Enemies>
```

The 16 enemy types of an area (the type is what rooms and special items give as `Type`) and how they move:
the tables at `$960B`-`$97AE` of the area bank. Each attribute is one table, named after what the code
(`Enemies.cpp`, `AreaCommon.cpp`, `Objects.cpp`) does with it. Pairs are by direction: first for direction 0
(facing right, or down for enemies that move vertically: `$0405` bit 0, or bit 2 when bit 7 is set), then
direction 1 (left, or up). Animations are indexes into the area's enemy animations (`metroid/sprites/<area>/
enemies`). Signed values (accelerations, speeds, offsets, steps) are decimal, the rest hex.

- `HitPoints` (`EnemyHitPointTbl`, `$962B`): doubled for tough enemies (rooms' `Tough="1"`). An enemy
  with `FF` hit points isn't hurt by weapons (they clang).
- `Delay` (`EnemyInitDelayTbl`, `$96BB`): how many frames a resting enemy waits before it picks a move (it
  also waits while `Distance` keeps it asleep). Reloaded when the enemy rests again.
- `Timer` (`$969B`): reloaded into `$040D`. A counter that, when it runs out, turns the enemy toward Samus
  (see `Flags`); also how long the enemy stays frozen (counted every 8 frames).
- `Distance` (`$96AB`): bits 0-6: how close Samus must be (in 16-pixel units, along x, or y for vertical
  movers), 0: always. Bit 7 set: farther away the enemy doesn't wake up (`$0405` bit 3); clear: it doesn't
  fire (`$0405` bit 4).
- `Flags` (`$968B`, a neutral name: several uses): bit 7 turns it toward Samus horizontally when `Timer`
  runs out; bit 1: it keeps doing so once moving; bit 4: touching it doesn't hurt Samus; bit 5: it doesn't
  turn around when blocked; bits 2-3 pick the sound when hit (`0C`: the Metroid's, `08`: the big enemy's,
  else the normal one); bit 0: lets it leave pickup `89` when it dies. Bit 6 isn't read by this code.
- `Flags2` (`$977B`, neutral too): bit 7 makes it a vertical mover (`$0405` bit 7); bit 6 makes it move by
  acceleration (the move's `Accel`/`Speed`) instead of by script; bits 0 and 1: it turns around when blocked
  horizontally / vertically; bits 2-3 go to `$6B03` (bit 3: script commands check the background above,
  else below; both clear: no check); bit 4: only missiles hurt it (other weapons just make the Metroid-hit
  sound), freezing sets its hit points to 5, it is frozen again after being stunned, and the code at `$DCFC`
  stores this bit in the dead enemy's `EnDataIndex`.
- `ExplodeAnim` (`$960B`), `RunAnim` (`$963B`), `TurnAnim` (`$965B`): the animations when it dies, moves,
  turns. `TurnAnimRewind` (`$967B`): bits 0-6: the turn animation starts that many entries before
  `TurnAnim` (it loops from `TurnAnim`); bit 7 isn't read by this code.
- `Moves` (`$96CB` and the move sets at `$97D1`): the moves it picks from at random when it starts moving
  (1, 2, 4, 8... of them). `MoveSet` is where the set was (offset from `$97D1`); leave it. A type whose
  `MoveSet` doesn't lead to a set has no `Moves` and keeps its byte as it is. Sets live in the
  `MoveSetArea` bytes from `$97D1`; sets that are the same are stored once. Bigger sets take the place of
  the scripts that follow (those move to free space), but the game can't reach past `$97D1` + 255.

`Move` (moves 00-13): `Script0`, `Script1` (`$96DB`, pointers by direction) are the movement scripts of
enemies that move by script; `AccelY`, `AccelX`, `SpeedY`, `SpeedX` (`$972B`, `$973F`, `$9753`, `$9767`) the
start values of enemies that move by acceleration (`Flags2` bit 6).

`Projectile` (types 0-3, what the area code fires): `Anim` (`$978B`), `OffsetX` (`$979B`, by direction),
`OffsetY` (`$97A3`): its animation and where it starts from the enemy. `FireballPath` 0-3 (`$97A7`): the
paths of the fireballs (a `Script` with `Path="1"`, only steps and `Restart`).

A `Script` is the list of steps an enemy follows while it moves (one step per movement update, normally
once a frame):

- `Step Count Y X`: move Y and X pixels per update (-7 to 7; `-0` keeps a sign bit that does nothing),
  Count times. Count is below `F0` in movement scripts.
- `Restart` (`FF`, or `Code="F0"`-`F9`): back to the first step. `Stop` (`FA`): stop moving. `Hold` (`FB`):
  stay on this step without moving. `ResetCount` (`FD`): set `$6B01` (a count of the pixels moved) to 0.
- `RepeatWhileFree` (`FC`): repeat the previous step while the background next to the enemy is free (above
  or below, by `$6B03`; with `$6B03` 0, forever), then go on. `RepeatWhileFreeThenStop` (`FE`): the same,
  but then stop (turning around first if `Flags` bit 5 is set).

Scripts several moves share are one `Script`; a changed or bigger script goes back where it was if it
fits, else into free space, and the pointers follow. Tourian's types are mostly driven by its own code; its
tables are there all the same.

## Sprites: `metroid/sprites/objects`, `metroid/sprites/<area>/enemies`

```xml
<Sprites Version="0" Kind="objects" Banks="1 2 3 4 5" AnimTable="8572" AnimSize="153" FrameTable="860B"
         Frames="106" PlaceTable="86DF" Placements="15">
    <Animation Index="0E">17 18 19 1A</Animation>
    <Frame Number="00" Address="87CB" Size="18" Placement="0" Palette="0" FlipX="1" RadiusY="0F" RadiusX="04">
        <Sprite Tile="00"/>
        <Attributes Value="20"/>
        <Skip/>
        <Offset Y="-8" X="0"/>
        ...
    </Frame>
    <Frame Number="01" SameAs="00"/>
    <Placement Number="00" Address="8701" Size="30">-16,-8 -16,0 -8,-16 ...</Placement>
    <Placement Number="07" Address="875D" Size="24">*0,*0 *1,*1 ... -12,-8 -12,0 ...</Placement>
</Sprites>
```

How objects and enemies are drawn. `objects` are Samus, her shots, bombs, missiles and the doors: the same
tables in the five area banks (`ObjectAnimIdxTbl` `$8572`, `FramePtrTable` `$860B`, `PlacePtrTable`
`$86DF`), so one resource goes into all of them. `<area>/enemies` are the area's
`EnemyAnimIndexTbl`, `EnemyFramePtrTbl1` (frames 00-7F), `EnemyFramePtrTbl2` (frames 80 and up) and
`EnemyPlacePtrTbl`.

- `Animation Index`: a run of frame numbers ending with `FF` (`Open="1"`: without it). The game starts an
  animation at an index (or, for turns, a few entries before) and, at the end, goes back to where it
  started. Frame `F7` draws nothing. Indexes are what the code and the enemy tables use, so the runs stay
  where they are; the objects' table can't grow, an enemy table can (up to 256 bytes) and moves.
- `Frame`: `Placement` (0-F) picks the placement table, `Palette` (0-3) the sprite palette, `FlipX`/`FlipY`
  mirror the frame (its placement offsets too); `RadiusY`/`RadiusX`: the object's half-size (visibility,
  collisions). Then, in order: `Sprite Tile` draws an 8x8 tile at the next placement entry; `Skip` passes
  over one; `Offset Y X` moves the rest of the frame; `Attributes Value` sets the sprite attribute byte of
  the next sprites (bit 7 flip y, bit 6 flip x, bit 5 behind the background, bits 0-1 palette); before
  that, sprites have the frame's flips and palette and bit 5 set. `SameAs`: the frame is the same bytes as
  another (changing that one changes both); give it content of its own to change it alone.
- `Placement`: y,x offsets in pixels from the object's position, one per sprite. `*n` (byte `8n`) is a
  piece of an explosion: it flies out over time (bits 1-3: the speed set, bit 0: left or right) from the
  offset 6 entries further on. Placement tables overlap in the original (one starts inside another); each
  lists the entries its frames use, and they are put back sharing the bytes when unchanged.

`Address`/`Size` and the root's tables are where things were: leave them. Frames and placement tables that
grow go into free space; an enemy pointer table that gets more entries moves too (with its `AreaPointers`
entry), the objects' tables (at fixed addresses) can't grow.

## Room meshes: `metroid/meshes/<area>/<h|v>_<XX>_<YY>`

The 3D geometry of each scrolling section (see [ENGINE.md](ENGINE.md)): `h` or `v` for horizontal or
vertical, and the map position of its first screen. They are made from the rooms, structures and macros
when the data is extracted, so they aren't edited by hand: if a mod changes the rooms, a section whose
tiles differ from those its mesh was made from gets a new mesh when it is entered. To give a room a
different look, use a room model (see [MODELS.md](MODELS.md)).

The file is binary (little endian): `RMSH`, version (16 bits, 1), area bank, horizontal (0/1), first screen
X and Y, length in screens, a padding byte, a 64-bit hash of the tiles it was made from (four 16-bit
words, low first), then the 8x8-tile blocks: screen, block column, block row, group count (16 bits), and
per group the tile number, palette (0-3), quad count (16 bits) and the quads: four corners of x, y, z
(signed 16 bits, pixels; y up, so the section's top-left is 0,0 and y is negative) and texel u, v (8 bits
each), then a brightness byte. Solid tiles (below `$A0`) are blocks from z -8 to +8 with faces only
where they don't touch another solid tile; other tiles are flat at z -6; tile `$FF` (the blank tile
room RAM is filled with) has no faces. The pictures come from the game's pattern table and palettes when
they are drawn.

## Everything else: `metroid/data/<bank>/<name>`

Data without a type of its own yet is kept as the bytes of a table, named after the disassembly's label:

```xml
<DataBlock Version="0" Bank="7" Address="DE35" Name="ItemDropTbl">
    ...
</DataBlock>
```

A block goes back exactly where it came from, so a mod can change its bytes but not its size. Folders:
`title` (bank 0: title, passwords, ending), `brinstar`, `norfair`, `tourian`, `kraid`, `ridley` (the area
banks), `graphics` (bank 6) and `engine` (bank 7). What is left here is small tables: engine and title
settings, the password tables, the title and ending sprites, and tables that hold code addresses.

## How it fits together

`tools/assetmap/gen_assetmap.py` generates the list of data in the ROM (addresses and names, from the
community disassembly) as `src/game/data/AssetMap.inc`. `src/game/data/` extracts the resources, reads and
writes their XML, and builds what the game reads from them: every piece at the address the original used,
so the port's translated code reads it unchanged. The lockstep test runs the port on data that went through
the extraction and the XML, so it checks all of this against the original.
