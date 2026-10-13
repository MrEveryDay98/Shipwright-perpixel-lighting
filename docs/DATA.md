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
data, music and text are getting types of their own next.

## How it fits together

`tools/assetmap/gen_assetmap.py` generates the list of data in the ROM (addresses and names, from the
community disassembly) as `src/game/data/AssetMap.inc`. `src/game/data/` extracts the resources, reads and
writes their XML, and builds what the game reads from them: every piece at the address the original used,
so the port's translated code reads it unchanged. The lockstep test runs the port on data that went through
the extraction and the XML, so it checks all of this against the original.
