#include "Rom.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace Metroid {

namespace {

constexpr uint32_t kPrgSize = 0x20000;
constexpr uint32_t kUsaPrgCrc32 = 0x70080810;

// Per-area-bank data pointers (identical layout in banks 1-5).
constexpr uint16_t kSpecItemsPtr = 0x9598;
constexpr uint16_t kRoomPtrTblPtr = 0x959A;
constexpr uint16_t kStructPtrTblPtr = 0x959C;
constexpr uint16_t kMacroDefsPtr = 0x959E;
constexpr uint16_t kStartMapX = 0x95D7;
constexpr uint16_t kStartMapY = 0x95D8;
// Samus/object frame tables (identical in every area bank).
constexpr uint16_t kObjFramePtrTbl = 0x860B;
constexpr uint16_t kObjPlacePtrTbl = 0x86DF;
constexpr int kObjFrameCount = (kObjPlacePtrTbl - kObjFramePtrTbl) / 2;
constexpr uint16_t kObjAnimTbl = 0x8572;
// Enemy tables (per area bank).
constexpr uint16_t kEnFramePtrTblPtr = 0x95A0;
constexpr uint16_t kEnPlacePtrTblPtr = 0x95A4;
constexpr uint16_t kEnAnimTblPtr = 0x95A6;
constexpr uint16_t kEnAnimIndexTbl = 0x963B; // [type * 2 + facing] -> offset into the enemy animation table
constexpr int kSpriteCanvas = 64;
// World map (bank 0) and the CHR loading table (bank 7).
constexpr uint32_t kWorldMapOffset = 0x253E;
constexpr uint16_t kGfxInfoTbl = 0xC6E0;

// Banks are numbered like the areas: bank 1 = Brinstar ... bank 5 = Ridley.
// PPU palette strings ($3F00 + length header skipped).
constexpr uint16_t kAreaPalette[AREA_COUNT] = { 0, 0xA274, 0xA17B, 0xA71B, 0xA158, 0xA0EE };
constexpr uint16_t kAreaAltPalette[AREA_COUNT] = { 0, 0xA2B0, 0xA1B7, 0, 0, 0xA12A };

// GFXInfo entries the game loads when entering each area (from the area init routines in bank 7).
const std::vector<int> kAreaGfx[AREA_COUNT] = {
    {},
    { 0x03, 0x04, 0x05, 0x06, 0x19, 0x16 },
    { 0x04, 0x05, 0x07, 0x08, 0x09, 0x19, 0x16 },
    { 0x05, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x1A, 0x1C, 0x19, 0x16 },
    { 0x04, 0x05, 0x0A, 0x0F, 0x10, 0x11, 0x19, 0x16 },
    { 0x04, 0x05, 0x0A, 0x12, 0x13, 0x19, 0x16 },
};

// The ROM does not say which area a screen belongs to (the game only knows the current area),
// so this was derived from map connectivity: B/N/T/K/R = Brinstar, Norfair, Tourian, Kraid, Ridley.
// Lower case marks screens that use the area's alternate background palette.
const char* const kAreaLayout[kMapH] = {
    "................................", "...........B.b......b...........", "...BBBBBBBBB.bbbbbbbb.........b.",
    ".T.T.......B.bbbbbbbbbbbbbbbbbb.", ".T.T.......B.b................b.", ".T.T..TBBBBB.b..........bbbbbbb.",
    ".T.T.......B.b....b...b.......b.", ".T.TTTTTTTTBBbbbbbbbbbbbbbbbbbb.", ".T........TB.b....b...b.......b.",
    ".T........TB.b....bbbbb.N.....N.", ".T........TB.b....b...b.NNNNNNN.", ".TTTTTTTTTTB.bbbbbbbbbb.NNNNNNN.",
    "...........B.n........N.NNNNNNN.", "...........B.N........N.N.....N.", ".BBBBBBBBBBB.NNNNNNNNNNNNNNNNNN.",
    "......B....K.NNNNNNNNNNNNNNNNNN.", "......B.K..K.nnNNNn...........N.", "......B.KKKK.nnnnnnnnnNNNNNNNNN.",
    "......BBKKKK.nnnnnnnnnnnnnnnnnN.", "......KKKKKK.n...nn......nnnnnN.", ".K.....KK..K.nnnnnnnnnnnnnnnNnN.",
    ".KKKKKKKKKKK.n...nNNNn..NNNNNNN.", ".KKKKKKKKKKK.N..NnnNNn..NN....N.", ".K..K..K....K....R.....RRR....R.",
    ".K..KKKKKKKKK.R..RRRRRRRRR....R.", ".K..K..K.KKKK.RRRRRRRRRRRRRRRRR.", ".K..KKKKKK.KK.R..R....R.......R.",
    ".KKKKKKKKKKKK.RRRRRRRRRRRRRRRRR.", ".K.KKKKKKKKKK.R.......R.......R.", "........KKKKK.RRRRRRRRRRRRRRRRR.",
    "............K.RRRRRRRRRRRRRRRRR.", "................................",
};

// 2C02 master palette.
constexpr uint32_t kNesPalette[64] = {
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00, 0x333500, 0x0B4800, 0x005200,
    0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000, 0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B,
    0xB53120, 0x994E00, 0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000, 0xFFFEFF,
    0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22, 0xBCBE00, 0x88D800, 0x5CE430, 0x45E082,
    0x48CDDE, 0x4F4F4F, 0x000000, 0x000000, 0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5,
    0xF7D8A5, 0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

uint32_t Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

Area AreaFromChar(char c) {
    switch (c) {
        case 'B':
        case 'b':
            return AREA_BRINSTAR;
        case 'N':
        case 'n':
            return AREA_NORFAIR;
        case 'T':
            return AREA_TOURIAN;
        case 'K':
            return AREA_KRAID;
        case 'R':
        case 'r':
            return AREA_RIDLEY;
        default:
            return AREA_NONE;
    }
}

} // namespace

uint32_t Rom::NesColor(uint8_t idx) {
    return (kNesPalette[idx & 0x3F] << 8) | 0xFF;
}

uint8_t Rom::Rd(int bank, uint16_t addr) const {
    uint32_t off = addr >= 0xC000 ? 7 * 0x4000 + (addr - 0xC000) : bank * 0x4000 + (addr - 0x8000);
    return off < mPrg.size() ? mPrg[off] : 0xFF;
}

uint16_t Rom::Rw(int bank, uint16_t addr) const {
    return Rd(bank, addr) | (Rd(bank, addr + 1) << 8);
}

Area Rom::ScreenArea(int sx, int sy) const {
    if (sx < 0 || sy < 0 || sx >= kMapW || sy >= kMapH || mWorldMap[sy * kMapW + sx] == 0xFF) {
        return AREA_NONE;
    }
    return AreaFromChar(kAreaLayout[sy][sx]);
}

bool Rom::ScreenAltPalette(int sx, int sy) const {
    char c = kAreaLayout[sy][sx];
    return c == 'b' || c == 'n' || c == 'r';
}

bool Rom::Load(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "Could not open " + path;
        return false;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t start = 0;
    if (file.size() >= 16 && memcmp(file.data(), "NES\x1A", 4) == 0) {
        start = 16;
    }
    if (file.size() - start < kPrgSize) {
        error = "File is too small to be Metroid (USA)";
        return false;
    }
    mPrg.assign(file.begin() + start, file.begin() + start + kPrgSize);
    if (Crc32(mPrg.data(), mPrg.size()) != kUsaPrgCrc32) {
        error = "This is not the Metroid (USA) ROM this build understands (PRG CRC32 mismatch)";
        return false;
    }

    memcpy(mWorldMap.data(), &mPrg[kWorldMapOffset], mWorldMap.size());

    for (int a = AREA_BRINSTAR; a < AREA_COUNT; a++) {
        BuildChr((Area)a);
        for (int i = 0; i < 32; i++) {
            mPalette[a][i] = Rd(a, kAreaPalette[a] + i);
        }
        for (int i = 0; i < 16; i++) {
            mAltPalette[a][i] = kAreaAltPalette[a] ? Rd(a, kAreaAltPalette[a] + i) : mPalette[a][i];
        }
    }
    mStartX = Rd(AREA_BRINSTAR, kStartMapX);
    mStartY = Rd(AREA_BRINSTAR, kStartMapY);

    mTiles.assign(kWorldTilesW * kWorldTilesH, WorldTile{});
    mDoors.clear();
    mItems.clear();
    mElevators.clear();
    mEnemies.clear();
    for (int sy = 0; sy < kMapH; sy++) {
        for (int sx = 0; sx < kMapW; sx++) {
            DecodeScreen(sx, sy);
        }
    }
    for (int a = AREA_BRINSTAR; a < AREA_COUNT; a++) {
        DecodeItems((Area)a);
    }
    DecodeSpriteFrames();
    mLoaded = true;
    return true;
}

void Rom::BuildChr(Area area) {
    auto& chr = mChr[area];
    chr.fill(0);
    std::vector<int> groups = { 0 }; // Samus and power-up sprites
    groups.insert(groups.end(), kAreaGfx[area].begin(), kAreaGfx[area].end());
    for (int g : groups) {
        uint16_t e = kGfxInfoTbl + g * 7;
        int bank = Rd(7, e);
        uint16_t src = Rw(7, e + 1);
        uint16_t dst = Rw(7, e + 3);
        uint16_t len = Rw(7, e + 5);
        for (int i = 0; i < len && dst + i < 0x2000; i++) {
            chr[dst + i] = Rd(bank, src + i);
        }
    }
}

uint8_t Rom::ChrPixel(Area area, uint16_t base, uint8_t chr, int x, int y) const {
    const auto& c = mChr[area];
    uint16_t a = base + chr * 16 + y;
    return ((c[a] >> (7 - x)) & 1) | (((c[a + 8] >> (7 - x)) & 1) << 1);
}

TileKind Rom::Classify(Area area, uint8_t chr) const {
    // Mirrors the game's background collision checks in bank 7 ($D651, $E7F9, $E9C6).
    if (chr >= 0xA0) {
        return TILE_BG;
    }
    uint8_t blastStart = area == AREA_BRINSTAR ? 0x80 : 0x70;
    if (chr < blastStart || chr >= 0x98) {
        return TILE_SOLID;
    }
    return TILE_BLAST;
}

void Rom::DecodeScreen(int sx, int sy) {
    Area area = ScreenArea(sx, sy);
    if (area == AREA_NONE) {
        return;
    }
    int bank = area;
    uint8_t room = mWorldMap[sy * kMapW + sx];
    uint16_t roomCount = (Rw(bank, kStructPtrTblPtr) - Rw(bank, kRoomPtrTblPtr)) / 2;
    if (room >= roomCount) {
        return;
    }
    uint16_t a = Rw(bank, Rw(bank, kRoomPtrTblPtr) + 2 * room);
    uint16_t structTbl = Rw(bank, kStructPtrTblPtr);
    uint16_t macroDefs = Rw(bank, kMacroDefsPtr);
    a++; // default attribute byte

    // Room objects: [position yx][structure][palette] ... $FD (objects end) / $FF (room end).
    bool hasEntities = false;
    for (int guard = 0; guard < 256; guard++) {
        uint8_t v = Rd(bank, a);
        if (v == 0xFF) {
            break;
        }
        if (v == 0xFD) {
            a++;
            hasEntities = true;
            break;
        }
        if (v == 0xFE) {
            a++;
            continue;
        }
        int x0 = v & 0x0F, y = v >> 4;
        uint8_t structIdx = Rd(bank, a + 1);
        uint8_t pal = Rd(bank, a + 2) & 3;
        a += 3;
        // Structure: rows of [x offset << 4 | count] followed by macro numbers, $FF terminated.
        uint16_t s = Rw(bank, structTbl + 2 * structIdx);
        for (int row = 0; row < 16; row++, y++) {
            uint8_t hdr = Rd(bank, s++);
            if (hdr == 0xFF) {
                break;
            }
            int count = (hdr & 0x0F) ? (hdr & 0x0F) : 16;
            int xo = hdr >> 4;
            for (int i = 0; i < count; i++) {
                uint8_t macro = Rd(bank, s++);
                int mx = x0 + xo + i;
                if (mx < 0 || mx >= 16 || y < 0 || y >= 15) {
                    continue;
                }
                for (int q = 0; q < 4; q++) {
                    uint8_t chr = Rd(bank, macroDefs + macro * 4 + q);
                    int tx = sx * kScreenTilesW + mx * 2 + (q & 1);
                    int ty = sy * kScreenTilesH + y * 2 + (q >> 1);
                    WorldTile& t = mTiles[ty * kWorldTilesW + tx];
                    t.chr = chr;
                    t.palette = pal;
                    t.kind = Classify(area, chr);
                }
            }
        }
    }

    // Room entities: enemies (type 1), doors (type 2), elevators (type 4), statues (6), pipes (7).
    for (int guard = 0; hasEntities && guard < 64; guard++) {
        uint8_t v = Rd(bank, a);
        if (v == 0xFF) {
            break;
        }
        switch (v & 0x0F) {
            case 1: {
                uint8_t type = Rd(bank, a + 1);
                uint8_t pos = Rd(bank, a + 2);
                mEnemies.push_back({ sx * kScreenPxW + (pos & 0x0F) * 16 + 8, sy * kScreenPxH + (pos >> 4) * 16 + 8,
                                     type, area, false });
                a += 3;
                break;
            }
            case 2: {
                uint8_t info = Rd(bank, a + 1);
                mDoors.push_back({ sx, sy, (info >> 4) == 0xA, (uint8_t)(info & 0x03) });
                a += 2;
                break;
            }
            case 4: {
                uint8_t info = Rd(bank, a + 1);
                mElevators.push_back({ sx, sy, (info & 0x80) != 0, (uint8_t)(info & 0x0F) });
                a += 2;
                break;
            }
            case 6:
                a += 1;
                break;
            case 7: { // pipe that keeps releasing enemies
                uint8_t type = Rd(bank, a + 1);
                uint8_t pos = Rd(bank, a + 2);
                mEnemies.push_back({ sx * kScreenPxW + (pos & 0x0F) * 16 + 8, sy * kScreenPxH + (pos >> 4) * 16 + 8,
                                     type, area, true });
                a += 3;
                break;
            }
            default:
                guard = 64; // unknown entry, stop parsing this room
                break;
        }
    }
}

void Rom::DecodeItems(Area area) {
    // Special item table: linked list of map rows, each a linked list of map columns whose data
    // is a sequence of typed entries terminated by $00.
    int bank = area;
    uint16_t a = Rw(bank, kSpecItemsPtr);
    for (int rows = 0; rows < 32; rows++) {
        uint8_t my = Rd(bank, a);
        uint16_t nextRow = Rw(bank, a + 1);
        uint16_t p = a + 3;
        for (int cols = 0; cols < 32; cols++) {
            uint8_t mx = Rd(bank, p);
            uint8_t nextCol = Rd(bank, p + 1);
            uint16_t q = p + 2;
            bool ours = ScreenArea(mx, my) == area;
            for (int guard = 0; guard < 16; guard++) {
                uint8_t t = Rd(bank, q);
                if (t == 0) {
                    break;
                }
                uint8_t kind = t & 0x0F;
                if (kind == 2) {
                    uint8_t item = Rd(bank, q + 1);
                    uint8_t pos = Rd(bank, q + 2);
                    if (ours && item < PU_COUNT) {
                        mItems.push_back(
                            { mx * kScreenPxW + (pos & 0x0F) * 16 + 8, my * kScreenPxH + (pos >> 4) * 16 + 8, item });
                    }
                    q += 3;
                } else if (kind == 4) {
                    uint8_t info = Rd(bank, q + 1);
                    if (ours) {
                        mElevators.push_back({ mx, my, (info & 0x80) != 0, (uint8_t)(info & 0x0F) });
                    }
                    q += 2;
                } else if (kind == 9) {
                    uint8_t info = Rd(bank, q + 1);
                    if (ours) {
                        mDoors.push_back({ mx, my, (info >> 4) == 0xA, (uint8_t)(info & 0x03) });
                    }
                    q += 2;
                } else {
                    break; // enemies/Mother Brain/Zebetites etc: not handled yet
                }
            }
            if (nextCol == 0xFF) {
                break;
            }
            p += nextCol;
        }
        if (nextRow == 0xFFFF) {
            break;
        }
        a = nextRow;
    }
}

SpriteFrame Rom::DecodeFrame(Area area, uint16_t framePtrTbl, uint16_t placeTbl, int idx) const {
    // Frame: [flags | placement index][2 size bytes] then tiles. $FD n sets sprite attributes,
    // $FC dy dx shifts the placement, $FE skips a placement slot, $FF ends. Placement tables are
    // signed (y, x) pairs. Flag $40 mirrors the frame horizontally and $80 vertically.
    const int bank = area;
    const auto& pal = mPalette[area];
    SpriteFrame fr;
    fr.w = fr.h = kSpriteCanvas;
    fr.originX = fr.originY = kSpriteCanvas / 2;
    fr.rgba.assign(kSpriteCanvas * kSpriteCanvas, 0);
    fr.minX = fr.minY = 1000;
    fr.maxX = fr.maxY = -1000;

    uint16_t a = Rw(bank, framePtrTbl + 2 * idx);
    uint8_t hdr = Rd(bank, a);
    uint16_t place = Rw(bank, placeTbl + 2 * (hdr & 0x0F));
    uint8_t attr = hdr & 0xF0;
    int shiftY = 0, shiftX = 0;
    a += 3;
    for (int guard = 0; guard < 64; guard++) {
        uint8_t t = Rd(bank, a++);
        if (t == 0xFF) {
            break;
        }
        if (t == 0xFD) {
            attr = Rd(bank, a++);
            continue;
        }
        if (t == 0xFC) {
            shiftY = (int8_t)Rd(bank, a);
            shiftX = (int8_t)Rd(bank, a + 1);
            a += 2;
            continue;
        }
        int y = (int8_t)Rd(bank, place) + shiftY;
        int x = (int8_t)Rd(bank, place + 1) + shiftX;
        place += 2;
        if (t == 0xFE) {
            continue;
        }
        if (hdr & 0x40) {
            x = -x - 8;
        }
        if (hdr & 0x80) {
            y = -y - 8;
        }
        bool hflip = attr & 0x40, vflip = attr & 0x80;
        for (int py = 0; py < 8; py++) {
            for (int px = 0; px < 8; px++) {
                uint8_t ci = ChrPixel(area, 0x0000, t, hflip ? 7 - px : px, vflip ? 7 - py : py);
                int ox = fr.originX + x + px, oy = fr.originY + y + py;
                if (ci && ox >= 0 && oy >= 0 && ox < kSpriteCanvas && oy < kSpriteCanvas) {
                    fr.rgba[oy * kSpriteCanvas + ox] = NesColor(pal[16 + (attr & 3) * 4 + ci]);
                    fr.minX = std::min(fr.minX, x + px);
                    fr.maxX = std::max(fr.maxX, x + px);
                    fr.minY = std::min(fr.minY, y + py);
                    fr.maxY = std::max(fr.maxY, y + py);
                }
            }
        }
    }
    if (fr.minX > fr.maxX) {
        fr.minX = fr.minY = 1;
        fr.maxX = fr.maxY = 0;
    }
    return fr;
}

void Rom::DecodeSpriteFrames() {
    mObjectFrames.clear();
    for (int i = 0; i < kObjFrameCount; i++) {
        mObjectFrames.push_back(DecodeFrame(AREA_BRINSTAR, kObjFramePtrTbl, kObjPlacePtrTbl, i));
    }
    for (int a = AREA_BRINSTAR; a < AREA_COUNT; a++) {
        uint16_t frames = Rw(a, kEnFramePtrTblPtr), place = Rw(a, kEnPlacePtrTblPtr);
        int count = (place - frames) / 2;
        mEnemyFrames[a].clear();
        for (int i = 0; i < count; i++) {
            mEnemyFrames[a].push_back(DecodeFrame((Area)a, frames, place, i));
        }
    }
}

std::vector<uint8_t> Rom::ObjectAnim(uint8_t offset) const {
    std::vector<uint8_t> seq;
    for (int i = 0; i < 16; i++) {
        uint8_t f = Rd(AREA_BRINSTAR, kObjAnimTbl + offset + i);
        if (f == 0xFF) {
            break;
        }
        seq.push_back(f == 0xF7 ? 0xFF : f);
    }
    return seq;
}

std::vector<uint8_t> Rom::EnemyAnim(Area area, uint8_t type, bool facingRight) const {
    std::vector<uint8_t> seq;
    uint16_t table = Rw(area, kEnAnimTblPtr);
    uint8_t offset = Rd(area, kEnAnimIndexTbl + (type & 0x0F) * 2 + (facingRight ? 0 : 1));
    for (int i = 0; i < 16; i++) {
        uint8_t f = Rd(area, table + offset + i);
        if (f == 0xFF) {
            break;
        }
        if (f != 0xF7 && f < mEnemyFrames[area].size()) {
            seq.push_back(f);
        }
    }
    return seq;
}

void Rom::TilePixels(Area area, bool alt, uint8_t chr, uint8_t palette, uint32_t out[64], bool transparentZero) const {
    const uint8_t* pal = alt ? mAltPalette[area].data() : mPalette[area].data();
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            uint8_t ci = ChrPixel(area, 0x1000, chr, x, y);
            out[y * 8 + x] = (ci == 0 && transparentZero) ? 0 : NesColor(pal[(palette & 3) * 4 + ci]);
        }
    }
}

bool Rom::TileBlank(Area area, uint8_t chr) const {
    const auto& c = mChr[area];
    for (int i = 0; i < 16; i++) {
        if (c[0x1000 + chr * 16 + i]) {
            return false;
        }
    }
    return true;
}

} // namespace Metroid
