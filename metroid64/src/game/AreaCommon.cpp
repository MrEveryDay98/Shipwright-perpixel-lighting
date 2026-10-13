// Code shared by the five area banks ($8058-$8571 enemy movement, $8B13-$8D39 doors). The bytes are
// identical in banks 1-5; the tables they read differ per bank (Game::Read reads the mapped bank).

#include "game/AreaCommon.h"

#include "game/Area.h"
#include "game/EnemyRam.h"
#include "game/Enemies.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Original.h"
#include "game/Ram.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;

namespace {
constexpr uint16_t kEnemyMoveFlagsTbl = 0x977B;
constexpr uint16_t kEnemyDataTbl = 0x968B;
constexpr uint16_t kEnemyMovePtrTbl = 0x96DB;
constexpr uint16_t kDoorDelayTbl = 0x8BD1;
constexpr uint16_t kDoorXTbl = 0x8D3A;

uint16_t Ptr(uint8_t zp) {
    return (uint16_t)(Mem(zp) | (Mem((uint8_t)(zp + 1)) << 8));
}

// $E770 / $E77B / $E8F1 / $E8FC (bank 7, not translated here): is the background free above / below /
// left of / right of the enemy? Return the carry (set: free).
bool BgCheck(uint16_t addr) {
    return Original(addr).c;
}
} // namespace

// --- $8058 -------------------------------------------------------------------------------------------

void EnemyMove8058() {
    uint8_t x = PageIndex;
    if ((En0405[x] << 1) & 0x80) {
        return;
    }
    if (Enstatus[x] != 0x02) {
        return;
    }
    if (EnemyVertSpeed8244(x)) {
        return;
    }
    uint8_t a = Mem(0x00);
    if (a & 0x80) {
        Mem(0x66) = TwosCompliment(a);
        do {
            EnemyBlockedUp80B8(EnemyMoveUp83F5());
        } while (--Mem(0x66) != 0);
    } else if (a != 0) {
        Mem(0x66) = a;
        do {
            EnemyBlockedDown80FB(EnemyMoveDown844B());
        } while (--Mem(0x66) != 0);
    }
    EnemyHorzSpeed8318(PageIndex);
    a = Mem(0x00);
    if (a & 0x80) {
        Mem(0x66) = TwosCompliment(a);
        do {
            EnemyBlockedLeft816E(EnemyMoveLeft84A7());
        } while (--Mem(0x66) != 0);
    } else if (a != 0) {
        Mem(0x66) = a;
        do {
            EnemyBlockedRight8134(EnemyMoveRight84FE());
        } while (--Mem(0x66) != 0);
    }
}

uint8_t EnemyType80B0(uint8_t x, bool& carry) {
    uint8_t a = Read((uint16_t)(kEnemyMoveFlagsTbl + EnDataIndex[x]));
    carry = a & 0x80;
    return (uint8_t)(a << 1);
}

// --- $80B8-$81B0: what happens when the enemy can't move further -----------------------------------

void EnemyBlockedUp80B8(bool carry) {
    uint8_t x = PageIndex;
    if (carry) {
        return;
    }
    bool c;
    if (En0405[x] & 0x80) {
        Enemy81FC(x);
    } else if (EnemyType80B0(x, c) & 0x80) {
        uint8_t a = En6B03[x];
        if (a == 0) {
            Enemy81FC(x);
        } else if (!(a & 0x80)) {
            c = true;
            En0402[x] = Ror(En0402[x], c);
            EnCounter[x] = Ror(EnCounter[x], c);
        } else {
            a = Enemy81B1(x);
            En0402[x] = a;
            EnCounter[x] = a;
        }
    } else {
        uint8_t a = Read((uint16_t)(kEnemyMoveFlagsTbl + EnDataIndex[x]));
        if (a & 0x02) {
            ToggleEnemyBits856B(x, 0x04);
        }
    }
    Mem(0x66) = 0x01;
}

void EnemyBlockedDown80FB(bool carry) {
    uint8_t x = PageIndex;
    if (carry) {
        return;
    }
    bool c;
    if (En0405[x] & 0x80) {
        Enemy81FC(x);
    } else if (EnemyType80B0(x, c) & 0x80) {
        uint8_t a = En6B03[x];
        if (a == 0) {
            Enemy81FC(x);
        } else if (a & 0x80) {
            c = false;
            En0402[x] = Ror(En0402[x], c);
            EnCounter[x] = Ror(EnCounter[x], c);
        } else {
            Enemy81B1(x);
            if (Read((uint16_t)(kEnemyMoveFlagsTbl + EnDataIndex[x])) & 0x02) {
                ToggleEnemyBits856B(x, 0x04);
            }
        }
    } else {
        if (Read((uint16_t)(kEnemyMoveFlagsTbl + EnDataIndex[x])) & 0x02) {
            ToggleEnemyBits856B(x, 0x04);
        }
    }
    Mem(0x66) = 0x01;
}

void EnemyBlockedRight8134(bool carry) {
    uint8_t x = PageIndex;
    if (carry) {
        return;
    }
    bool c;
    if (EnemyType80B0(x, c) & 0x80) {
        if (!(En0405[x] & 0x80)) {
            Enemy81C7(x);
        } else {
            uint8_t a = En6B03[x];
            if (a == 0) {
                Enemy81C7(x);
            } else if (a & 0x80) {
                c = false;
                En0403[x] = Ror(En0403[x], c);
                En0407[x] = Ror(En0407[x], c);
            } else {
                Enemy81C0(x);
            }
        }
    } else {
        if (Read((uint16_t)(kEnemyMoveFlagsTbl + EnDataIndex[x])) & 0x01) {
            ToggleEnemyBits856B(x, 0x01);
        }
    }
    Mem(0x66) = 0x01;
}

void EnemyBlockedLeft816E(bool carry) {
    uint8_t x = PageIndex;
    if (carry) {
        return;
    }
    bool c;
    if (EnemyType80B0(x, c) & 0x80) {
        if (!(En0405[x] & 0x80)) {
            Enemy81C7(x);
        } else {
            uint8_t a = En6B03[x];
            if (a == 0) {
                Enemy81C7(x);
            } else if (a & 0x80) {
                a = Enemy81C0(x);
                En0403[x] = a;
                En0407[x] = a;
            } else {
                c = true;
                En0403[x] = Ror(En0403[x], c);
                En0407[x] = Ror(En0407[x], c);
            }
        }
    } else {
        uint8_t a = EnemyType80B0(x, c);
        if (a & 0x02) {
            ToggleEnemyBits856B(x, 0x01);
        }
    }
    Mem(0x66) = 0x01;
}

// $81B1 / $81B8 / $81C0: stop accelerating; return A (0).
uint8_t Enemy81B1(uint8_t x) {
    SetEnemyBits(x, 0x20);
    En6AFE[x] = 0;
    return 0;
}

uint8_t Enemy81C0(uint8_t x) {
    SetEnemyBits(x, 0x20);
    En6AFF[x] = 0;
    return 0;
}

// $81F6
static bool Enemy81F6(uint8_t x) {
    return (EnemyDataF74B(x) & 0x20) != 0;
}

// $81C7: turn around horizontally.
void Enemy81C7(uint8_t x) {
    if (Enemy81F6(x)) {
        return;
    }
    ToggleEnemyBits856B(x, 0x01);
    Enemy81D1(x);
}

void Enemy81D1(uint8_t x) {
    En6AFF[x] = TwosCompliment(En6AFF[x]);
    Enemy81DA(x);
}

void Enemy81DA(uint8_t x) {
    if (Enemy81F6(x)) {
        return;
    }
    bool c;
    uint8_t a = EnemyType80B0(x, c);
    c = true;
    if (a & 0x80) {
        En0407[x] = Sbc(0, En0407[x], c);
    }
    En0403[x] = Sbc(0, En0403[x], c);
}

// $81FC: turn around vertically.
void Enemy81FC(uint8_t x) {
    if (Enemy81F6(x)) {
        return;
    }
    ToggleEnemyBits856B(x, 0x04);
    Enemy8206(x);
}

void Enemy8206(uint8_t x) {
    En6AFE[x] = TwosCompliment(En6AFE[x]);
    Enemy820F(x);
}

void Enemy820F(uint8_t x) {
    if (Enemy81F6(x)) {
        return;
    }
    bool c;
    uint8_t a = EnemyType80B0(x, c);
    c = true;
    if (a & 0x80) {
        EnCounter[x] = Sbc(0, EnCounter[x], c);
    }
    En0402[x] = Sbc(0, En0402[x], c);
}

// --- $822B Table-driven movement ----------------------------------------------------------------------

void Enemy822B(uint8_t x) {
    uint8_t a = En0405[x];
    bool c = (a & 0x80) ? (a & 0x04) : (a & 0x01);
    uint8_t y = (uint8_t)((((En0408[x] << 1) | (c ? 1 : 0)) << 1));
    Mem(0x81) = Read((uint16_t)(kEnemyMovePtrTbl + y));
    Mem(0x82) = Read((uint16_t)(kEnemyMovePtrTbl + 1 + y));
}

uint8_t SpeedFromNibble8296(uint8_t a) {
    bool neg = a & 0x80;
    a = (uint8_t)(((uint8_t)(a << 1)) >> 5);
    if (neg) {
        a = (uint8_t)((a ^ 0xFF) + 1);
    }
    Mem(0x00) = a;
    return a;
}

// $8312: end of the movement: stop.
static void Enemy8312(uint8_t x) {
    Mem(0x00) = Enemy81B1(x);
}

bool EnemyVertSpeed8244(uint8_t x) {
    bool c;
    if (EnemyType80B0(x, c) & 0x80) {
        EnemyVertAccel833F(x);
        return false;
    }
    if (((En0405[x] & 0x20) ^ 0x20) == 0) {
        Mem(0x00) = 0;
        return false;
    }
    Enemy822B(x);
    uint8_t y = EnCounter[x];
    for (;;) {
        uint8_t a = Read((uint16_t)(Ptr(0x81) + y));
        if (a < 0xF0) {
            if ((uint8_t)(a - EnDelay[x]) == 0) {
                EnDelay[x] = 0;
                y = (uint8_t)(y + 2);
                EnCounter[x] = y;
                if (y != 0) {
                    continue;
                }
            }
            // $8290
            EnDelay[x]++;
            y++;
            SpeedFromNibble8296(Read((uint16_t)(Ptr(0x81) + y)));
            return false;
        }
        switch (a) {
            case 0xFA:
                Enemy8312(x);
                return false;
            case 0xFB:
                return true; // PLA PLA: leaves the caller
            case 0xFC: {
                // Wait until the background below/above is free.
                uint8_t s = En6B03[x];
                bool free;
                if (s & 0x80) {
                    free = BgCheck(0xE770);
                } else if (s == 0) {
                    y = (uint8_t)(EnCounter[x] - 2);
                    EnCounter[x] = y;
                    continue;
                } else {
                    free = BgCheck(0xE77B);
                }
                x = PageIndex;
                if (free) {
                    y = (uint8_t)(EnCounter[x] - 2);
                } else {
                    y = (uint8_t)(EnCounter[x] + 1);
                    En6B03[x] = 0;
                }
                EnCounter[x] = y;
                continue;
            }
            case 0xFD:
                EnCounter[x]++;
                y++;
                En6B01[x] = 0;
                continue;
            case 0xFE: {
                y = (uint8_t)(y - 2);
                EnCounter[x] = y;
                uint8_t s = En6B03[x];
                bool free = false;
                bool skip = false;
                if (s & 0x80) {
                    free = BgCheck(0xE770);
                } else if (s == 0) {
                    skip = true;
                } else {
                    free = BgCheck(0xE77B);
                }
                if (!skip) {
                    x = PageIndex;
                    if (free) {
                        y = EnCounter[x];
                        continue; // $8258
                    }
                }
                // $82FB
                uint8_t d = Read((uint16_t)(kEnemyDataTbl + EnDataIndex[x]));
                if (d & 0x20) {
                    En0405[x] = (uint8_t)(((En0405[x] ^ 0x05) | d) & 0x1F);
                }
                Enemy8312(x);
                return false;
            }
            default:
                EnCounter[x] = 0;
                y = 0;
                continue;
        }
    }
}

uint8_t SpeedFromByte832F(uint8_t a) {
    uint8_t v = a & 0x07;
    if (a & 0x08) {
        v = TwosCompliment(v);
    }
    Mem(0x00) = v;
    return v;
}

void EnemyHorzSpeed8318(uint8_t x) {
    bool c;
    if (EnemyType80B0(x, c) & 0x80) {
        EnemyHorzAccel8395(x);
        return;
    }
    if (((En0405[x] & 0x20) ^ 0x20) == 0) {
        Mem(0x00) = 0;
        return;
    }
    uint8_t y = (uint8_t)(EnCounter[x] + 1);
    SpeedFromByte832F(Read((uint16_t)(Ptr(0x81) + y)));
}

void EnemyVertAccel833F(uint8_t x) {
    uint8_t y = 0x0E;
    uint8_t a = En6AFE[x];
    bool c;
    if (!(a & 0x80)) {
        c = false;
        EnCounter[x] = Adc(a, EnCounter[x], c);
        a = Adc(En0402[x], 0x00, c);
        En0402[x] = a;
        if (a & 0x80) {
            a = TwosCompliment(a);
            y = 0xF2;
        }
    } else {
        a = TwosCompliment(a);
        c = true;
        Mem(0x00) = a;
        EnCounter[x] = Sbc(EnCounter[x], Mem(0x00), c);
        a = Sbc(En0402[x], 0x00, c);
        En0402[x] = a;
        if (a & 0x80) {
            a = TwosCompliment(a);
            y = 0xF2;
        }
    }
    if (a >= 0x0E) {
        EnCounter[x] = 0;
        En0402[x] = y;
    }
    c = false;
    En6AFC[x] = Adc(En6AFC[x], EnCounter[x], c);
    Mem(0x00) = Adc(0x00, En0402[x], c);
}

void EnemyHorzAccel8395(uint8_t x) {
    Mem(0x00) = 0;
    Mem(0x02) = 0;
    Mem(0x01) = 0x0E;
    Mem(0x03) = 0x0E;
    bool c = false;
    uint8_t a = Adc(En0407[x], En6AFF[x], c);
    En0407[x] = a;
    Mem(0x04) = a;
    a = (En6AFF[x] & 0x80) ? 0xFF : 0x00;
    a = Adc(a, En0403[x], c);
    En0403[x] = a;
    uint8_t y = a;
    if (a & 0x80) {
        c = true;
        Mem(0x04) = Sbc(0, En0407[x], c);
        y = Sbc(0, En0403[x], c);
        // $E449 (bank 7): negate $00/$01.
        bool c2 = true;
        Mem(0x00) = Sbc(0, Mem(0x00), c2);
        Mem(0x01) = Sbc(0, Mem(0x01), c2);
    }
    c = Mem(0x04) >= Mem(0x02);
    Sbc(y, Mem(0x03), c);
    if (c) {
        En0407[x] = Mem(0x00);
        En0403[x] = Mem(0x01);
    }
    c = false;
    En6AFD[x] = Adc(En6AFD[x], En0407[x], c);
    Mem(0x00) = Adc(0x00, En0403[x], c);
}

// --- $83F5-$8571: moving one pixel, with the room's edges and name tables ---------------------------

bool EnemyMoveUp83F5() {
    uint8_t x = PageIndex;
    bool c = true;
    if (((uint8_t)(EnYRoomPos[x] - EnRadY[x]) & 0x07) == 0) {
        c = BgCheck(0xE770);
    }
    Mem(0x00) = 0;
    x = PageIndex;
    if (!c) {
        return false;
    }
    Mem(0x00)++;
    uint8_t y = EnYRoomPos[x];
    if (y == 0) {
        y = 0xF0;
        if (ScrollDir < 0x02) {
            if (ScrollY == 0) {
                return false;
            }
            if (EnemyOtherNameTable8563(x) == 0) {
                return false;
            }
            ToggleEnemyNameTable855A(x);
        }
    }
    y--;
    EnYRoomPos[x] = y;
    if (y == EnRadY[x]) {
        if (ScrollY == 0 || EnemyOtherNameTable8563(x) == 0) {
            EnYRoomPos[x]++;
            return false;
        }
    }
    if (!(En0405[x] & 0x80)) {
        En6B01[x]++;
    }
    return true;
}

bool EnemyMoveDown844B() {
    uint8_t x = PageIndex;
    bool c = true;
    if (((uint8_t)(EnYRoomPos[x] + EnRadY[x]) & 0x07) == 0) {
        c = BgCheck(0xE77B);
    }
    Mem(0x00) = 0;
    x = PageIndex;
    if (!c) {
        return false;
    }
    Mem(0x00)++;
    uint8_t y = EnYRoomPos[x];
    if (y == 0xEF) {
        y = 0xFF;
        if (ScrollDir < 0x02) {
            if (ScrollY == 0) {
                return false;
            }
            if (EnemyOtherNameTable8563(x) != 0) {
                return false;
            }
            ToggleEnemyNameTable855A(x);
        }
    }
    y++;
    EnYRoomPos[x] = y;
    if ((uint8_t)(y + EnRadY[x]) == 0xEF) {
        if (ScrollY == 0 || EnemyOtherNameTable8563(x) != 0) {
            EnYRoomPos[x]--;
            return false;
        }
    }
    if (!(En0405[x] & 0x80)) {
        En6B01[x]--;
    }
    return true;
}

bool EnemyMoveLeft84A7() {
    uint8_t x = PageIndex;
    bool c = true;
    if (((uint8_t)(EnXRoomPos[x] - EnRadX[x]) & 0x07) == 0) {
        c = BgCheck(0xE8F1);
    }
    Mem(0x00) = 0;
    x = PageIndex;
    if (!c) {
        return false;
    }
    Mem(0x00)++;
    if (EnXRoomPos[x] == 0 && ScrollDir >= 0x02) {
        if (ScrollX == 0 || EnemyOtherNameTable8563(x) == 0) {
            return false;
        }
        ToggleEnemyNameTable855A(x);
    }
    EnXRoomPos[x]--;
    if (EnXRoomPos[x] == EnRadX[x]) {
        if (ScrollX == 0 || EnemyOtherNameTable8563(x) == 0) {
            EnXRoomPos[x]++;
            return false;
        }
    }
    if (En0405[x] & 0x80) {
        En6B01[x]++;
    }
    return true;
}

bool EnemyMoveRight84FE() {
    uint8_t x = PageIndex;
    bool c = true;
    if (((uint8_t)(EnXRoomPos[x] + EnRadX[x]) & 0x07) == 0) {
        c = BgCheck(0xE8FC);
    }
    Mem(0x00) = 0;
    x = PageIndex;
    if (!c) {
        return false;
    }
    Mem(0x00)++;
    if (++EnXRoomPos[x] == 0 && ScrollDir >= 0x02) {
        if (ScrollX == 0 || EnemyOtherNameTable8563(x) != 0) {
            EnXRoomPos[x]--;
            return false;
        }
        ToggleEnemyNameTable855A(x);
    }
    if ((uint8_t)(EnXRoomPos[x] + EnRadX[x]) == 0xFF) {
        if (ScrollX == 0 || EnemyOtherNameTable8563(x) != 0) {
            EnXRoomPos[x]--;
            return false;
        }
    }
    if (En0405[x] & 0x80) {
        En6B01[x]--;
    }
    return true;
}

bool EnemyMoveByTable8048(uint8_t index) {
    switch (index & 0x07) {
        case 0:
        case 5:
            return EnemyMoveRight84FE();
        case 1:
        case 4:
            return EnemyMoveLeft84A7();
        case 2:
        case 3:
            return EnemyMoveDown844B();
        default:
            return EnemyMoveUp83F5();
    }
}

void ToggleEnemyNameTable855A(uint8_t x) {
    EnNameTable[x] ^= 0x01;
}

uint8_t EnemyOtherNameTable8563(uint8_t x) {
    return (EnNameTable[x] ^ PPUCNT0ZP) & 0x01;
}

void ToggleEnemyBits856B(uint8_t x, uint8_t a) {
    En0405[x] ^= a;
}

// --- $8B13 SamusEnterDoor -----------------------------------------------------------------------------

namespace {
// $8B74 SamusInDoor
void SamusInDoor(uint8_t a) {
    DoorStatus = a | 0x80;
}

// $8B53 SetDoorEntryInfo
void SetDoorEntryInfo(uint8_t x) {
    DoorScrollStatus = x;
    SamusInDoor(x);
    DoorDelay = 0x12;
    SamusDoorData = (uint8_t)((SamusDoorData << 4) | ObjAction);
    ObjAction = 0x05;
}
} // namespace

void SamusEnterDoor8B13() {
    if (DoorStatus != 0 || SamusDoorData == 0) {
        return;
    }
    CrntMslePickups = 0;
    CrntEnrgyPickups = 0;
    uint8_t a = RandomNumber1 & 0x0F;
    MaxMissilePickup = a;
    MaxEnergyPickup = (uint8_t)((a << 1) | 0x40);
    uint8_t y = (PPUCNT0ZP ^ 0x01) & 0x01;
    Mem((uint16_t)(0x006C + y)) = 0;
    uint8_t x;
    if (ScrollDir & 0x02) {
        x = 0x02;
        if (ObjectX & 0x80) {
            x--;
        }
    } else {
        x = 0x04;
        if (ScrollY == 0) {
            // $8B6D: the room is already centred.
            SetDoorEntryInfo(x);
            Regs r = Original(0xE21B); // VertRoomCentered (bank 7)
            SamusInDoor(r.x);
            return;
        }
        if ((PPUCNT0ZP ^ ObjectHi) & 0x01) {
            x--;
        }
    }
    SetDoorEntryInfo(x);
}

// --- $8B79 DrawDoors ----------------------------------------------------------------------------------

namespace {

// $8CF7 / $8CFB: writes the door's column of tiles into room RAM.
void DoorTiles(uint8_t x, uint8_t tile) {
    Mem(0x02) = 0x50;
    Mem(0x03) = Read((uint16_t)(kDoorXTbl + ((x >> 4) & 0x01)));
    Mem(0x0B) = ObjectHi[x];
    Original(0xE96A); // MakeCartRAMPtr (bank 7)
    for (uint8_t y = 0; y != 0xC0; y += 0x20) {
        Write((uint16_t)(Ptr(0x04) + y), tile);
    }
    x = PageIndex;
    uint8_t y = (x >> 3) & 0x06;
    Mem((uint16_t)(0x005C + y)) = Mem(0x04);
    Mem((uint16_t)(0x005D + y)) = Mem(0x05);
}

// $8C7E: animation index, then the door sound.
void DoorAnimSound(uint8_t x, uint8_t a) {
    AnimIndex[x] = a; // $D2FD
    AnimDelay[x] = 0;
    SFXDoor();
}

// $8C76
void DoorOpenAnim(uint8_t x) {
    AnimResetIndex[x] = 0x30;
    DoorAnimSound(x, 0x2E);
}

// $8BB1: draw the door.
void DrawDoor(uint8_t x) {
    uint8_t a = Mem((uint16_t)(0x0307 + x));
    if (a == 0x03) {
        a = 0x01;
    }
    ObjectCntrl = a | 0xA0;
    SamusHit[x] = 0;
    ObjectCntrl = (uint8_t)(((x & 0x10) ^ 0x10) | ObjectCntrl);
    AnimDrawObject(0x06);
}

// $8C61
void DoorClose(uint8_t x) {
    SamusJmpDsplcmnt[x] = 0x01;
    DoorTiles(x, 0x4E);
    ObjAction[x] = 0x02;
    DoorOpenAnim(x);
}

void DoOneDoor(uint8_t x) {
    PageIndex = x;
    uint8_t action = ObjAction[x];
    ChooseRoutineEffects(0x8B8F, action);
    switch (action) {
        case 0x01: // $8B9D: new door
            ObjAction[x]++;
            SetProjectileAnim(x, 0x30);
            DoorTiles(x, 0x4E);
            SamusJmpDsplcmnt[x] = Read((uint16_t)(kDoorDelayTbl + Mem((uint16_t)(0x0307 + x))));
            DrawDoor(x);
            break;
        case 0x02: // $8BD5: closed; shot?
            if ((SamusHit[x] & 0x04) && --SamusJmpDsplcmnt[x] == 0) {
                if (Mem((uint16_t)(0x0307 + x)) != 0x03 || (uint8_t)(EndTimerHi + 1) == 0) {
                    ObjAction[x] = 0x03;
                    SamusJmpDsplcmnt[x] = 0x50;
                    AnimResetIndex[x] = 0x2C;
                    DoorAnimSound(x, 0x29);
                    break; // (JMP $8C7E: not drawn this frame)
                }
            }
            DrawDoor(x);
            break;
        case 0x03: // $8C01: open
            if (DoorStatus != 0 && !((ObjectHi ^ ObjectHi[x]) & 0x01) && !((ObjectX ^ ObjectX[x]) & 0x80)) {
                ObjAction[x] = 0x04;
            } else if (AnimIndex[x] >= AnimResetIndex[x]) {
                if (SamusJmpDsplcmnt[x] == 0x50) {
                    DoorTiles(x, 0xFF);
                    uint8_t t = Mem((uint16_t)(0x0307 + x));
                    if (t != 0x01 && t != 0x03) {
                        // A one-time door: noted in the unique item history.
                        Mem(0x09) = 0x0A;
                        Mem(0x08) = ObjectHi[x];
                        uint8_t y = MapPosX;
                        if ((uint8_t)(x << 3) & 0x80) {
                            y--;
                        }
                        Original(0xDC1E, { y, x, y });
                        ObjAction[x] = 0x00;
                        DrawDoor(x);
                        break;
                    }
                }
                if (!(FrameCount & 0x01) && --SamusJmpDsplcmnt[x] == 0) {
                    DoorClose(x);
                    x = PageIndex;
                }
            }
            DrawDoor(x);
            break;
        case 0x04: // $8C84: Samus is going through
            if (DoorStatus < 0x05) {
                DoorTiles(x, 0x4E);
                DoorOpenAnim(x);
                x = PageIndex;
                if (Mem(0x91) != 0 && (((x >> 4) ^ Mem(0x91)) & 0x01)) {
                    PalToggle ^= 0x07;
                    PalDataPending = PalToggle;
                }
                ObjAction[x]++;
                Mem(0x91) = 0;
                if (Mem((uint16_t)(0x0307 + x)) == 0x03) {
                    if ((uint8_t)(x << 3) & 0x80) {
                        MotherBrainMusic();
                    } else {
                        TourianMusic();
                    }
                }
            }
            x = PageIndex;
            DrawDoor(x);
            break;
        case 0x05: // $8CC6
            if (DoorStatus == 0x05) {
                uint8_t o = x ^ 0x10;
                ObjAction[o] = 0x06;
                AnimResetIndex[o] = 0x2C;
                DoorAnimSound(o, 0x29);
                SelectSamusPal();
                x = PageIndex;
                ObjAction[x] = 0x02;
            }
            DrawDoor(x);
            break;
        case 0x06: // $8CF0
            if (DoorStatus == 0) {
                DoorClose(x);
                x = PageIndex;
            }
            DrawDoor(x);
            break;
        default:
            break;
    }
}

} // namespace

void DrawDoors8B79() {
    uint8_t x = 0xB0;
    do {
        DoOneDoor(x);
        x = (uint8_t)(PageIndex - 0x10);
    } while (x & 0x80);
}

} // namespace Game
