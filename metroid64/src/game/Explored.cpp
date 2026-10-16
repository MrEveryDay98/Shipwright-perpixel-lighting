#include "game/Explored.h"

#include "game/Memory.h"
#include "game/Present.h"
#include "game/Ram.h"

#include <cstring>

namespace Game::Explored {

namespace {
uint8_t sVisited[kBytes / 2];
uint8_t sHorizontal[kBytes / 2];

bool Bit(const uint8_t* bits, int x, int y) {
    if (x < 0 || y < 0 || x > 31 || y > 31) {
        return false;
    }
    return bits[(y * 32 + x) >> 3] & (1 << (x & 7));
}
void SetBit(uint8_t* bits, int x, int y, bool on) {
    uint8_t m = (uint8_t)(1 << (x & 7));
    bits[(y * 32 + x) >> 3] = on ? (uint8_t)(bits[(y * 32 + x) >> 3] | m) : (uint8_t)(bits[(y * 32 + x) >> 3] & ~m);
}
} // namespace

void Reset() {
    memset(sVisited, 0, sizeof(sVisited));
    memset(sHorizontal, 0, sizeof(sHorizontal));
}

void Update() {
    using namespace Vars;
    if (GameMode != 0 || MainRoutine != 0x03 || DoorStatus != 0) {
        return; // not playing, or going through a door (the scrolling changes on the way)
    }
    int x = 0, y = 0;
    if (SamusScreen(x, y)) {
        SetBit(sVisited, x, y, true);
        SetBit(sHorizontal, x, y, (ScrollDir & 0x02) != 0);
    }
}

bool Visited(int mapX, int mapY) {
    return Bit(sVisited, mapX, mapY);
}

bool Horizontal(int mapX, int mapY) {
    return Bit(sHorizontal, mapX, mapY);
}

bool SamusScreen(int& mapX, int& mapY) {
    const Present::RoomTag& t = Present::gRooms[Vars::ObjectHi & 0x01];
    if (!t.valid) {
        return false;
    }
    mapX = t.mapX & 0x1F;
    mapY = t.mapY & 0x1F;
    return true;
}

bool ItemTaken(uint8_t type, uint8_t mapX, uint8_t mapY) {
    // $EE41 SetItemID / $DC67 CreateItemID.
    uint8_t lo = (uint8_t)((mapX << 5) | mapY);
    uint8_t hi = (uint8_t)((type << 2) | (mapX >> 3));
    int count = Vars::NumUniqueItems;
    for (int y = count; y >= 2; y -= 2) {
        if (Mem((uint16_t)(Vars::UnqItmHist.addr + y - 2)) == lo &&
            Mem((uint16_t)(Vars::UnqItmHist.addr + y - 1)) == hi) {
            return true;
        }
    }
    return false;
}

void Get(uint8_t* out) {
    memcpy(out, sVisited, sizeof(sVisited));
    memcpy(out + sizeof(sVisited), sHorizontal, sizeof(sHorizontal));
}

void Set(const uint8_t* in) {
    memcpy(sVisited, in, sizeof(sVisited));
    memcpy(sHorizontal, in + sizeof(sVisited), sizeof(sHorizontal));
}

} // namespace Game::Explored
