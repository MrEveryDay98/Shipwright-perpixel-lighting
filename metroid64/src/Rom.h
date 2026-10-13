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
    uint8_t type;
    Area area;
};

// One decoded sprite frame: RGBA8 (0xRRGGBBAA) image with the object's origin at (originX, originY).
struct SpriteFrame {
    int w = 0, h = 0, originX = 0, originY = 0;
    std::vector<uint32_t> rgba;
};

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

    const SpriteFrame& SamusFrame(int idx) const {
        return mSamusFrames[idx];
    }
    int SamusFrameCount() const {
        return (int)mSamusFrames.size();
    }

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
    void DecodeSamusFrames();
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
    std::vector<SpriteFrame> mSamusFrames;
    int mStartX = 3, mStartY = 14;
};

} // namespace Metroid
