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
banks: palettes, enemy data, music), `graphics` (bank 6) and `engine` (bank 7). Palettes, graphics, enemy
data and text are getting types of their own next.

## How it fits together

`tools/assetmap/gen_assetmap.py` generates the list of data in the ROM (addresses and names, from the
community disassembly) as `src/game/data/AssetMap.inc`. `src/game/data/` extracts the resources, reads and
writes their XML, and builds what the game reads from them: every piece at the address the original used,
so the port's translated code reads it unchanged. The lockstep test runs the port on data that went through
the extraction and the XML, so it checks all of this against the original.
