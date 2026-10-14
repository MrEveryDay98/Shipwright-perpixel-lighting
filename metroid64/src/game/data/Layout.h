#pragma once

// Room layouts: what the game draws into room RAM for a map screen, and the scrolling sections (the
// presentation's rooms) the world map is made of. Used to build the room meshes and by the renderer.

#include "game/data/Level.h"

#include <cstdint>
#include <functional>
#include <tuple>
#include <vector>

namespace Game::Data {

// A screen as the game draws it into room RAM ($6000 or $6400): 30 rows of 32 tiles, then the
// attribute table (64 bytes, one palette number per 2x2 macro, 4 per byte).
struct ScreenTiles {
    uint8_t ram[0x400] = {};

    uint8_t Tile(int x, int y) const {
        return ram[y * 32 + x];
    }
    // The background palette (0-3) of tile (x, y).
    uint8_t Palette(int x, int y) const {
        uint8_t attr = ram[0x3C0 + (y >> 2) * 8 + (x >> 2)];
        return (attr >> (((y & 2) << 1) | (x & 2))) & 3;
    }
};

// Reads a byte of bank 7 (the engine's tables the drawing uses: $EF9A, $EFF4, $F018).
using EngineRead = std::function<uint8_t(uint16_t)>;

// Draws room `number` of the area bank like SetupRoom/DrawRoom ($EA2B-$EFF7): room RAM filled with
// blank tiles and the room's palette, then each object's structure, macro by macro. False if the room
// doesn't exist; structures that don't exist are skipped (the game would draw whatever its pointer
// table points at).
bool DrawScreen(const Level& level, int bank, uint8_t number, const EngineRead& engine, ScreenTiles& out);

// A scrolling section: the map screens the game scrolls through without going through a door. Horizontal
// sections stop at a door on the side they would scroll to (DoorOnNameTable, set by LoadDoor); vertical
// ones at a map cell with no room ($FF) (GetRoomNum, $E720).
struct Section {
    int bank = 1;
    bool horizontal = true;
    uint8_t x = 0, y = 0; // first screen (leftmost or topmost)
    uint8_t length = 1;   // screens

    uint8_t ScreenX(int i) const {
        return (uint8_t)(horizontal ? x + i : x);
    }
    uint8_t ScreenY(int i) const {
        return (uint8_t)(horizontal ? y : y + i);
    }
    bool operator<(const Section& o) const {
        return std::tie(bank, horizontal, x, y) < std::tie(o.bank, o.horizontal, o.x, o.y);
    }
    bool operator==(const Section& o) const {
        return bank == o.bank && horizontal == o.horizontal && x == o.x && y == o.y && length == o.length;
    }
};

// Door sides of a room ($10 of a door's info: left; else right): bit 0 right, bit 1 left.
uint8_t RoomDoors(const Level& level, int bank, uint8_t number);

// The section containing map screen (x, y) when the game scrolls that way.
Section FindSection(const Level& level, int bank, uint8_t x, uint8_t y, bool horizontal);

} // namespace Game::Data
