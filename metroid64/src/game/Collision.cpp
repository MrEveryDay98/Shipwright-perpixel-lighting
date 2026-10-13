// Bank 7: room lookup on the world map, collisions with the background and blastable tiles
// ($E720-$EA04), and the name table helpers they share ($EB85, $EC93).
//
// The crash checks work on temporaries: $08/$09/$0B the object's y, x and name table, $02/$03 the
// offset (radius) to the edge being checked, then the position of the first tile to test, and $04
// the number of tiles along that edge.

#include "game/Collision.h"

#include "game/Area.h"
#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Memory.h"
#include "game/Projectiles.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;
using namespace Const;

// --- $E720 GetRoomNum ----------------------------------------------------------------------------

// Puts the room number at the current map position in RoomNumber. Returns the carry: set when there is
// no room there, or (scrolling horizontally) a door is in the way.
bool GetRoomNum() {
    if ((ScrollDir >> 1) != 0) {
        uint8_t side = (uint8_t)(ScrollDir - 1); // 1 scrolling left, 2 right
        if (side & DoorOnNameTable3[OnNameTable0()]) {
            return true;
        }
    }
    uint16_t ptr = (uint16_t)(0x7000 + ((MapPosY & 0x1F) << 5) + MapPosX);
    Mem(0x00) = (uint8_t)ptr;
    Mem(0x01) = (uint8_t)(ptr >> 8);
    uint8_t room = Read(ptr);
    if (room == 0xFF) {
        return true;
    }
    RoomNumber = room;
    static const uint16_t kSpecialRooms = 0x95D0; // item rooms etc., per area bank
    for (uint8_t y = 0; y != 0x07; y++) {
        if (room == Read((uint16_t)(kSpecialRooms + y))) {
            ItemRmMusicSts = 0x01; // start the item room music on the next music start
            return false;
        }
    }
    if (ItemRmMusicSts != 0) {
        ItemRmMusicSts = 0x80; // stop it after the next music start
    }
    return false;
}

// --- $E770 Enemy crash checks ----------------------------------------------------------------------------

// $E770/$E77B: above/below the enemy at PageIndex. Return the carry: clear when blocked.
bool EnemyCheckMoveUp() {
    Mem(0x02) = (uint8_t)(EnRadY[PageIndex] + 0x08);
    return EnemyCheckMoveVert();
}
bool EnemyCheckMoveDown() {
    Mem(0x02) = (uint8_t)(0x00 - EnRadY[PageIndex]);
    return EnemyCheckMoveVert();
}
bool EnemyCheckMoveVert() {
    uint8_t x = PageIndex;
    Mem(0x04) = 0x08;
    EnemyCoordsToTemp(x);
    return CheckVertCrash(EnRadX[x]);
}

// $E792
void EnemyCoordsToTemp(uint8_t x) {
    Mem(0x09) = EnXRoomPos[x];
    Mem(0x08) = EnYRoomPos[x];
    Mem(0x0B) = EnNameTable[x];
}

// --- $E7A2 Object crash checks -----------------------------------------------------------------------------
// For the object at PageIndex (Samus, a projectile). Return the carry: clear when blocked.

bool CheckMoveUp() {
    uint8_t x = PageIndex;
    Mem(0x02) = (uint8_t)(ObjRadY[x] + 0x08);
    ObjCoordsToTemp(x);
    return CheckVertCrash(ObjRadX[x]);
}

bool CheckMoveDown() {
    uint8_t x = PageIndex;
    Mem(0x02) = (uint8_t)(0x00 - ObjRadY[x]);
    ObjCoordsToTemp(x);
    return CheckVertCrash(ObjRadX[x]);
}

// $E7BD: checks the row of tiles above or below, across the object's width.
bool CheckVertCrash(uint8_t radX) {
    if (radX == 0) {
        return true;
    }
    Mem(0x03) = radX;
    uint8_t a = (uint8_t)(Mem(0x09) - Mem(0x03)) & 0x07;
    Mem(0x04) = TilesToCheck(a, a != 0 ? 1 : 0, radX);
    CrashCoords();
    return CheckCrashLoop(Mem(0x00), 0x00, 0x08);
}

// $E7DE: only on a tile boundary (a = 0); then $04 tiles stepping by $06 down / $07 right.
bool CheckCrashLoop(uint8_t a, uint8_t stepY, uint8_t stepX) {
    if (a != 0) {
        return true;
    }
    Mem(0x06) = stepY;
    Mem(0x07) = stepX;
    uint8_t count = Mem(0x04);
    for (;;) {
        MakeCartRAMPtr();
        uint8_t tile = Read((uint16_t)(Mem(0x04) | (Mem(0x05) << 8)));
        if (tile == 0x4E) {
            return DoorTileHit();
        }
        Regs r = AreaHook(0x95C0, { tile, count, 0x00 });
        tile = r.a;
        count = r.x;
        if (!IsBlastableRange(tile)) {
            return false; // solid
        }
        if (tile < 0xA0) {
            return IsBlastTile(tile, count, false);
        }
        if (IsSamus != 0) {
            // Door tiles: $A0 toggles the scrolling, $A1 keeps it horizontal.
            SamusDoorData = (uint8_t)(IsSamus - 1);
            if (tile == 0xA0) {
                SamusDoorData++;
            } else if (tile == 0xA1) {
                SamusDoorData += 2;
            }
        }
        if (--count == 0) {
            return true;
        }
        NextCrashTile();
    }
}

// $E81E: tile $4E, the edge of a door. A projectile hitting a door opens it (missiles open red doors);
// anything else counts the hit. Always returns the carry clear (blocked).
bool DoorTileHit() {
    if (UpdtngPrjctl == 0) {
        return false;
    }
    for (int8_t x = 6; x >= 0; x -= 2) {
        if (((Mem(0x05) ^ Mem((uint16_t)(0x5D + x))) & 0x04) != 0 ||
            ((Mem(0x04) ^ Mem((uint16_t)(0x5C + x))) & 0x1F) != 0) {
            continue;
        }
        uint8_t y = (uint8_t)((x << 3) | 0x80); // the door object
        if (ObjAction[y] == 0) {
            continue;
        }
        if (!(Mem((uint16_t)(0x0307 + y)) & 0x01)) {
            // A red door: only missiles open it.
            uint8_t p = PageIndex;
            bool missile = ObjAction[p] == 0x0B || (ObjAction[p] == 0x04 && AnimResetIndex[p] == 0x91);
            if (!missile) {
                SFXMetal();
                return false;
            }
            Mem(0x0683) |= 0x02;
        }
        Mem((uint16_t)(0x030A + y)) = 0x04; // hit
        return false;
    }
    Mem((uint16_t)(0x0366 + ((Mem(0x04) >> 3) & 0x01)))++;
    return false;
}

bool CheckMoveLeft() {
    uint8_t x = PageIndex;
    Mem(0x03) = (uint8_t)(ObjRadX[x] + 0x08);
    ObjCoordsToTemp(x);
    return CheckHorzCrash(ObjRadY[x]);
}

bool CheckMoveRight() {
    uint8_t x = PageIndex;
    Mem(0x03) = (uint8_t)(0x00 - ObjRadX[x]);
    ObjCoordsToTemp(x);
    return CheckHorzCrash(ObjRadY[x]);
}

// $E89B: checks the column of tiles left or right, across the object's height.
bool CheckHorzCrash(uint8_t radY) {
    if (radY == 0) {
        return true;
    }
    Mem(0x02) = radY;
    uint8_t a = (uint8_t)(Mem(0x08) - Mem(0x02)) & 0x07;
    Mem(0x04) = TilesToCheck(a, a != 0 ? 1 : 0, radY);
    CrashCoords();
    return CheckCrashLoop(Mem(0x01), 0x08, 0x00);
}

// $E8BE: the object's position into $08/$09/$0B. Returns A (its x).
uint8_t ObjCoordsToTemp(uint8_t x) {
    Mem(0x0B) = ObjectHi[x];
    Mem(0x08) = ObjectY[x];
    Mem(0x09) = ObjectX[x];
    return Mem(0x09);
}

// $E8CE: how many tiles an edge 2*y pixels long, starting a pixels into a tile, covers.
uint8_t TilesToCheck(uint8_t a, uint8_t x, uint8_t y) {
    Mem(0x04) = (uint8_t)(0x00 - a) & 0x07;
    uint8_t len = (uint8_t)(y << 1);
    bool carry = len >= Mem(0x04);
    len = (uint8_t)(len - Mem(0x04));
    if (!carry) {
        len = (uint8_t)(len + 0x08);
    }
    Mem(0x04) = len >> 3;
    if (len & 0x07) {
        x++;
    }
    return (uint8_t)(x + Mem(0x04));
}

// $E8F1/$E8FC: left/right of the enemy at PageIndex.
bool EnemyCheckMoveLeft() {
    uint8_t x = PageIndex;
    Mem(0x03) = (uint8_t)(EnRadX[x] + 0x08);
    EnemyCoordsToTemp(x);
    return CheckHorzCrash(EnRadY[x]);
}
bool EnemyCheckMoveRight() {
    uint8_t x = PageIndex;
    Mem(0x03) = (uint8_t)(0x00 - EnRadX[x]);
    EnemyCoordsToTemp(x);
    return CheckHorzCrash(EnRadY[x]);
}

// $E90F: the room position of the first tile to check: $02 = y, $03 = x (with $00/$01 their offsets in
// the tile), crossing into the other name table when the edge is past the room's border. Rows $F0-$FF
// don't exist: y wraps by 16.
void CrashCoords() {
    uint8_t x;
    bool carry = Mem(0x08) >= Mem(0x02);
    x = (uint8_t)(Mem(0x08) - Mem(0x02));
    Mem(0x00) = x & 0x07;
    bool other = false;
    if (Mem(0x02) & 0x80) {
        // Below the object.
        if (carry || x >= 0xF0) {
            x = (uint8_t)(x + 0x10);
            other = true;
        }
    } else if (!carry) {
        // Above the object, past the top.
        x = (uint8_t)(x - 0x10);
        other = true;
    }
    if (other && !(ScrollDir & 0x02)) {
        Mem(0x0B)++;
    }
    Mem(0x02) = x;
    uint8_t hi = (Mem(0x03) & 0x80) ? 0x00 : 0xFF;
    carry = Mem(0x09) >= Mem(0x03);
    Mem(0x03) = (uint8_t)(Mem(0x09) - Mem(0x03));
    Mem(0x01) = Mem(0x03) & 0x07;
    if ((uint8_t)(hi + (carry ? 1 : 0)) != 0 && (ScrollDir & 0x02)) {
        Mem(0x0B)++;
    }
}

// --- $E96A MakeCartRAMPtr ----------------------------------------------------------------------------------

// ($04) = the room RAM address of the tile at y $02, x $03 on name table $0B: 01100HYY YYYXXXXX.
void MakeCartRAMPtr() {
    uint8_t hi = 0x18;
    bool carry = false;
    uint8_t a = Asl(Mem(0x02) & 0xF8, carry);
    hi = Rol(hi, carry);
    a = Asl(a, carry);
    hi = Rol(hi, carry);
    Mem(0x04) = (uint8_t)((Mem(0x03) >> 3) | a);
    Mem(0x05) = (uint8_t)(((Mem(0x0B) << 2) & 0x04) | hi);
}

// $E98E: moves the check position to the next tile along the edge.
void NextCrashTile() {
    uint8_t y = (uint8_t)(Mem(0x02) + Mem(0x06));
    Mem(0x02) = y;
    if (y >= 0xF0) {
        Mem(0x02) = (uint8_t)(y + 0x10);
        if (!(ScrollDir & 0x02)) {
            Mem(0x0B)++;
        }
    }
    bool carry = false;
    Mem(0x03) = Adc(Mem(0x03), Mem(0x07), carry);
    if (carry && (ScrollDir & 0x02)) {
        Mem(0x0B)++;
    }
}

// $E9B7: flips to the other name table.
void SwitchNameTable() {
    PPUCNT0ZP ^= 0x03;
}

// --- $E9BE IsBlastTile --------------------------------------------------------------------------------------

// A projectile hit a blastable tile ($04: its room RAM address): starts its blast in a free tile slot.
// Returns the carry, which is clear on every path (callers enter with it clear: the tile blocks).
bool IsBlastTile(uint8_t tile, uint8_t x, bool carry) {
    if (UpdtngPrjctl == 0) {
        return carry;
    }
    return BlastTile(tile, x);
}

// $E9C2: bombs come in here, whatever UpdtngPrjctl says. Returns the carry (clear).
bool BlastTile(uint8_t tile, uint8_t x) {
    uint8_t y = AreaHook(0x95BD, { tile, x, tile }).y;
    if (y >= 0x98) {
        return false; // not blastable after all
    }
    x = 0xC0;
    while (TileRoutine[x] != 0) {
        x = (uint8_t)(x - 0x10);
        if (x == 0) {
            if (TileRoutine[x] != 0) {
                return false; // no free slot
            }
            break;
        }
    }
    TileRoutine[x]++;
    TileWRAMLo[x] = Mem(0x04) & 0xDE;
    TileWRAMHi[x] = Mem(0x05);
    uint8_t type;
    if (InArea == 0x11 && y == 0x76) {
        type = 0x04 >> 1;
    } else {
        type = (uint8_t)(((y + 0x10) & 0x3C) >> 2);
    }
    TileType[x] = type;
    return false;
}

// --- $EB85 Name tables --------------------------------------------------------------------------------------

// $EB85: 1 when new objects go on name table 3, 0 for name table 0.
uint8_t GetNameTable() {
    return (PPUCNT0ZP ^ ScrollDir) & 0x01;
}

// $EC93: 1 when the screen shows name table 0, 0 for name table 3.
uint8_t OnNameTable0() {
    return (PPUCNT0ZP ^ 0x01) & 0x01;
}

} // namespace Game
