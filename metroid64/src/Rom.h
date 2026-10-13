#pragma once

// Metroid (NES) ROM decoder.
//
// Metroid64 ships no Nintendo data. The player supplies their own "Metroid (USA).nes" and
// everything (level layout, tiles, palettes, items, doors, Samus' animation frames) is decoded
// from it at startup.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Metroid {

constexpr int kMapW = 32; // world map is 32x32 screens
constexpr int kMapH = 32;
constexpr int kScreenTilesW = 32; // a screen is 32x30 8x8 tiles (256x240 px)
constexpr int kScreenTilesH = 30;
constexpr int kWorldTilesW = kMapW * kScreenTilesW;
constexpr int kWorldTilesH = kMapH * kScreenTilesH;
constexpr int kScreenPxW = 256;
constexpr int kScreenPxH = 240;

enum Area : uint8_t { AREA_NONE = 0, AREA_BRINSTAR, AREA_NORFAIR, AREA_TOURIAN, AREA_KRAID, AREA_RIDLEY, AREA_COUNT };

enum TileKind : uint8_t {
    TILE_EMPTY = 0, // nothing there
    TILE_BG,        // drawn, passable (pattern >= $A0)
    TILE_SOLID,     // drawn, solid
    TILE_BLAST,     // drawn, solid, destroyed by beams/bombs and regrows
};

// Door colours as encoded in the low nibble of the door byte.
enum DoorType : uint8_t { DOOR_RED = 0, DOOR_BLUE = 1, DOOR_ORANGE = 2, DOOR_TOURIAN = 3 };

struct Door {
    int screenX, screenY;
    bool rightSide;
    uint8_t type;
};

enum PowerUp : uint8_t {
    PU_BOMBS = 0,
    PU_HIGH_JUMP,
    PU_LONG_BEAM,
    PU_SCREW_ATTACK,
    PU_MORPH_BALL,
    PU_VARIA,
    PU_WAVE_BEAM,
    PU_ICE_BEAM,
    PU_ENERGY_TANK,
    PU_MISSILES,
    PU_COUNT
};

struct Item {
    int worldPxX, worldPxY; // centre of the item in world pixels (NES coordinates, y down)
    uint8_t type;           // PowerUp
};

struct Elevator {
    int screenX, screenY;
    bool goesUp;
    uint8_t dest; // destination area 0-4 as the game numbers them, $0F = ending
};

struct EnemySpawn {
    int worldPxX, worldPxY;
    uint8_t type; // low nibble: enemy kind for the area; bit 7: tougher variant
    Area area;
    bool pipe; // spawned repeatedly from a pipe (room entity type 7)
};

// One decoded sprite frame: RGBA8 (0xRRGGBBAA) image with the object's origin at (originX, originY).
// minX..maxY is the opaque bounding box relative to the origin (empty frames have minX > maxX).
struct SpriteFrame {
    int w = 0, h = 0, originX = 0, originY = 0;
    int minX = 1, minY = 1, maxX = 0, maxY = 0;
    std::vector<uint32_t> rgba;
    bool Empty() const {
        return minX > maxX;
    }
};

// Frame numbers in the ROM's object animation table ($8572 in every area bank) for Samus' poses.
// Each value is the table offset the game stores in Samus' animation index.
enum SamusAnim : uint8_t {
    ANIM_RUN = 0x00,
    ANIM_FRONT = 0x04,
    ANIM_STAND = 0x07,
    ANIM_JUMP = 0x0C,
    ANIM_SPIN = 0x0E,
    ANIM_MORPH = 0x16,
    ANIM_JUMP_SHOOT = 0x20,
    ANIM_RUN_SHOOT = 0x22,
    ANIM_STAND_UP = 0x27,
    ANIM_RUN_UP = 0x37,
    ANIM_JUMP_UP = 0x3B,
};

// Object (Samus/projectile/item) frames used directly.
constexpr uint8_t kFrameBeam = 0x28;
constexpr uint8_t kFrameWaveBeam = 0x2A;
constexpr uint8_t kFrameMissileH = 0x24;
constexpr uint8_t kFrameMissileUp = 0x27;
constexpr uint8_t kFrameBomb0 = 0x4E;
constexpr uint8_t kFrameBomb1 = 0x4F;
constexpr uint8_t kFrameBombBlast = 0x4C;
constexpr uint8_t kFramePowerUpBase = 0x50; // + PowerUp id

struct WorldTile {
    uint8_t chr = 0;     // pattern index in the area's background pattern table
    uint8_t palette = 0; // 0-3
    TileKind kind = TILE_EMPTY;
};

class Rom {
  public:
    bool Load(const std::string& path, std::string& error);
    bool IsLoaded() const {
        return mLoaded;
    }

    Area ScreenArea(int sx, int sy) const;
    bool ScreenAltPalette(int sx, int sy) const;
    uint8_t ScreenRoom(int sx, int sy) const {
        return mWorldMap[sy * kMapW + sx];
    }

    const WorldTile& Tile(int tx, int ty) const {
        return mTiles[ty * kWorldTilesW + tx];
    }
    const std::vector<Door>& Doors() const {
        return mDoors;
    }
    const std::vector<Item>& Items() const {
        return mItems;
    }
    const std::vector<Elevator>& Elevators() const {
        return mElevators;
    }
    const std::vector<EnemySpawn>& Enemies() const {
        return mEnemies;
    }

    // 8x8 RGBA8 texels of a background tile as it appears in the given area/palette variant.
    // Colour 0 becomes transparent when transparentZero is set.
    void TilePixels(Area area, bool alt, uint8_t chr, uint8_t palette, uint32_t out[64], bool transparentZero) const;
    // True if every pixel of the tile uses colour 0.
    bool TileBlank(Area area, uint8_t chr) const;
    // NES master palette entry as 0xRRGGBBAA.
    static uint32_t NesColor(uint8_t idx);

    // Samus, beams, bombs and power-up item frames.
    const SpriteFrame& ObjectFrame(int idx) const {
        return mObjectFrames[idx];
    }
    int ObjectFrameCount() const {
        return (int)mObjectFrames.size();
    }
    // Frame sequence of an object animation starting at the given table offset (blank frames are $FF).
    std::vector<uint8_t> ObjectAnim(uint8_t offset) const;

    // Enemy frames of an area, and the animation an enemy type plays (from the per-area tables).
    const SpriteFrame& EnemyFrame(Area area, int idx) const {
        return mEnemyFrames[area][idx];
    }
    int EnemyFrameCount(Area area) const {
        return (int)mEnemyFrames[area].size();
    }
    std::vector<uint8_t> EnemyAnim(Area area, uint8_t type, bool facingRight) const;

    int StartScreenX() const {
        return mStartX;
    }
    int StartScreenY() const {
        return mStartY;
    }

  private:
    uint8_t Rd(int bank, uint16_t addr) const;
    uint16_t Rw(int bank, uint16_t addr) const;
    void BuildChr(Area area);
    void DecodeScreen(int sx, int sy);
    void DecodeItems(Area area);
    void DecodeSpriteFrames();
    SpriteFrame DecodeFrame(Area area, uint16_t framePtrTbl, uint16_t placeTbl, int idx) const;
    TileKind Classify(Area area, uint8_t chr) const;
    uint8_t ChrPixel(Area area, uint16_t base, uint8_t chr, int x, int y) const;

    bool mLoaded = false;
    std::vector<uint8_t> mPrg; // 128 KiB PRG ROM, eight 16 KiB banks
    std::array<uint8_t, kMapW * kMapH> mWorldMap{};
    std::array<std::array<uint8_t, 0x2000>, AREA_COUNT> mChr{}; // CHR RAM contents while in each area
    std::array<std::array<uint8_t, 32>, AREA_COUNT> mPalette{}; // background + sprite palettes
    std::array<std::array<uint8_t, 16>, AREA_COUNT> mAltPalette{};
    std::vector<WorldTile> mTiles;
    std::vector<Door> mDoors;
    std::vector<Item> mItems;
    std::vector<Elevator> mElevators;
    std::vector<EnemySpawn> mEnemies;
    std::vector<SpriteFrame> mObjectFrames;
    std::array<std::vector<SpriteFrame>, AREA_COUNT> mEnemyFrames;
    int mStartX = 3, mStartY = 14;
};

} // namespace Metroid
