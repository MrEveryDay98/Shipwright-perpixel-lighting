#pragma once

// Level data as resources: the world map, and per area its rooms, structures and macros.
//
//   metroid/world/map                    <WorldMap>   32 rows of 32 room numbers ($FF: no room)
//   metroid/rooms/<area>/room_<NN>       <Room>       one room: objects (structures), enemies, doors...
//   metroid/structures/<area>/struct_<NN> <Structure> rows of macros
//   metroid/macros/<area>                <Macros>     2x2 tile blocks the structures are made of
//
// <area> is brinstar, norfair, tourian, kraid or ridley (area banks 1-5). Numbers are hex.

#include "game/data/Image.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tinyxml2 {
class XMLElement;
}

namespace Game::Data {

struct WorldMap {
    uint16_t address = 0xA53E;  // in bank 0
    uint8_t rooms[32][32] = {}; // [y][x]
};

// One entry of a room: a structure placed on the 16x15 grid of 2x2-tile macros, or (after the objects)
// an enemy, door, elevator, the statues or a Zeb hole.
struct RoomObject {
    uint8_t x = 0, y = 0; // position in macros
    uint8_t structure = 0;
    uint8_t palette = 0; // attribute palette
    bool pause = false;  // $FE: the rest is drawn next frame
};

struct RoomItem {
    enum class Kind : uint8_t { Enemy = 1, Door = 2, Elevator = 4, Statues = 6, ZebHole = 7 };
    Kind kind = Kind::Enemy;
    uint8_t slot = 0;     // enemy / Zeb hole slot (high nibble of the first byte)
    uint8_t type = 0;     // enemy type, with bit 7 (tough) and bit 6 (mini boss)
    uint8_t x = 0, y = 0; // position in macros (enemies, Zeb holes)
    uint8_t info = 0;     // door: $A0 + side ($10: left) + palette; elevator: destination
};

struct Room {
    int bank = 1;
    uint8_t number = 0;
    uint16_t address = 0; // where it was in the ROM (0: new)
    uint16_t size = 0;    // its size there
    uint8_t palette = 0;
    std::vector<RoomObject> objects;
    bool hasItems = false; // $FD and an item list follow the objects
    std::vector<RoomItem> items;

    std::vector<uint8_t> Encode() const;
};

struct Structure {
    int bank = 1;
    uint8_t number = 0;
    uint16_t address = 0;
    uint16_t size = 0;
    struct Row {
        uint8_t x = 0; // offset in macros
        std::vector<uint8_t> macros;
    };
    std::vector<Row> rows;

    std::vector<uint8_t> Encode() const;
};

struct MacroSet {
    int bank = 1;
    uint16_t address = 0;
    uint16_t size = 0;
    std::vector<std::array<uint8_t, 4>> macros; // tiles: top left, top right, bottom left, bottom right
};

// Every level resource of the game.
struct Level {
    WorldMap world;
    bool haveWorld = false;
    std::map<int, std::map<uint8_t, Room>> rooms;           // [bank][number]
    std::map<int, std::map<uint8_t, Structure>> structures; // [bank][number]
    std::map<int, MacroSet> macros;                         // [bank]
};

// Reads the level out of the PRG ROM into resource files; marks the bytes it covers in `covered` (one
// byte per PRG byte).
void ExtractLevel(const uint8_t* prg, std::map<std::string, std::string>& files, std::vector<uint8_t>& covered);

// Resource XML: write, and read into the level (by the root element's name). False if it isn't a level
// resource or is malformed.
std::string WriteWorldMap(const WorldMap& map);
std::string WriteRoom(const Room& room);
std::string WriteStructure(const Structure& s);
std::string WriteMacros(const MacroSet& m);
bool ReadLevelResource(const tinyxml2::XMLElement* root, Level& level, std::string& error);

// Puts the level into the image (pointer tables included).
bool AssembleLevel(const Level& level, Image& image, std::string& error);

} // namespace Game::Data
