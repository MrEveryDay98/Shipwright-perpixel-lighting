// Banks 1 (Brinstar, $99A1-$9D35) and 4 (Kraid's hideout, $9905-$9C49): the areas' own code. The two
// banks share their enemy routines at different addresses; the differences are the table addresses,
// the enemy types without a routine, and one bug Kraid's bank fixes ($9B94: the pieces around the boss
// keep their own slot). Both area routines ($95C3) are an RTS.

#include "game/Area.h"
#include "game/AreaCommon.h"
#include "game/AreaEnemies.h"
#include "game/EnemyRam.h"
#include "game/Enemies.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

namespace {

struct BankTables {
    uint16_t crawlerAnim; // $9AA0 / $99EF
    uint16_t ripperSpeed; // $9BA0 / $9AB2
    uint16_t pieceY;      // $9CB7 / $9BCB
    uint16_t pieceX;      // $9CBB / $9BCF
    uint16_t pieceType;   // $9CC6 / $9BDA
    bool kraid;
};
constexpr BankTables kBrinstar{ 0x9AA0, 0x9BA0, 0x9CB7, 0x9CBB, 0x9CC6, false };
constexpr BankTables kKraid{ 0x99EF, 0x9AB2, 0x9BCB, 0x9BCF, 0x9BDA, true };
const BankTables* sBank = &kBrinstar;

// $99D8
void Enemy3AI(uint8_t x) {
    if (Enstatus[x] != 0x03) {
        EnemyTurnAnim2();
    }
    Delays6And8();
}

// $99E5
void Enemy2AI(uint8_t x) {
    Mem(0x85) = 0x21;
    Mem(0x86) = 0x1E;
    if (Enstatus[x] != 0x03) {
        EnemyTurnAnim();
    }
    Delays6And8();
}

// $99FA: the enemy that bursts into four pieces ($A0-$AF).
void Enemy4AI(uint8_t x) {
    uint8_t s = Mem(0x81);
    if (s == 0x01) {
        EnemyAnimDrawOnly(0x08); // $9A44
        return;
    }
    if (s == 0x03) {
        EnemyDraw(); // $9A49
        return;
    }
    uint8_t c = EnCounter[x];
    if (c >= 0x0F) {
        if (c < 0x11) {
            En6B01[x] = 0x3A;
        } else if (--En6B01[x] == 0) {
            Enstatus[x] = 0;
            for (int y = 0x0C; y >= 0; y -= 4) {
                PieceTimer[y] = 0x0A;
                PieceY[y] = EnYRoomPos[x];
                PieceX[y] = EnXRoomPos[x];
                PieceHi[y] = EnNameTable[x];
            }
        }
    }
    EnemyAnimDraw(0x02);
}

// $9AF5
void Enemy6AI(uint8_t x) {
    uint8_t s = Mem(0x81);
    if (s == 0x01) {
        EnemyAnimDrawOnly(0x08);
        return;
    }
    if (s == 0x03) {
        EnemyDraw();
        return;
    }
    En6AFE[x] = 0x80;
    if (!(En0402[x] & 0x80) && (En0405[x] & 0x10)) {
        uint8_t a = (uint8_t)(EnYRoomPos[x] - ObjectY);
        if (a & 0x80) {
            a = TwosCompliment(a);
        }
        if (a < 0x10) {
            En6AFE[x] = 0x00;
        }
    }
    EnemyAnimDraw(0x03);
}

// $9C2A: one of the pieces around the enemy in slot 0. Returns X as the original leaves it.
uint8_t PieceCheck(uint8_t x) {
    uint8_t y = Enstatus[x];
    bool reset = false;
    if (y == 0) {
        reset = true;
    } else {
        uint8_t d = EnDataIndex[x];
        if (d != 0x0A && d != 0x09) {
            return x;
        }
        if (!(En0405[x] & 0x02)) {
            reset = true;
        } else {
            y--;
            if (y == 0) {
                // $9C60
            } else if (y == 0x02) {
                reset = true;
            } else if (y != 0x03 || En040C[x] != 0x01) {
                return x;
            }
        }
    }
    if (reset) {
        Enstatus[x] = 0;
        EnSpecialAttribs[x] = 0;
        EnemyRestartEB6E(x);
    }
    // $9C60
    uint8_t a = En0405[0];
    En0405[x] = a;
    bool c = a & 0x01;
    y = (uint8_t)(x >> 4);
    Mem(0x04) = Read((uint16_t)(sBank->pieceY + y));
    EnDataIndex[x] = Read((uint16_t)(sBank->pieceType + y));
    y = (uint8_t)((y << 1) | (c ? 1 : 0));
    Mem(0x05) = Read((uint16_t)(sBank->pieceX + y));
    EnemyPosTo08(0);
    bool moved = MoveObjectByDeltas();
    if (!sBank->kraid) {
        x = PageIndex; // Brinstar's bank goes on with the boss's slot
    }
    if (!moved) {
        return x;
    }
    if (Enstatus[x] == 0) {
        Enstatus[x]++;
    }
    EnemyPosFrom08(x);
    return x;
}

// $9CCC / $9D05: wake a resting piece now and then.
void WakePieces(uint8_t zp, uint8_t start, uint8_t first, uint8_t count, uint8_t mask) {
    uint8_t y = Mem(zp);
    if (y == 0) {
        y = start;
    }
    if (FrameCount & 0x02) {
        return;
    }
    y--;
    Mem(zp) = y;
    uint8_t a = (uint8_t)(y << 1);
    if (a & 0x80) {
        return;
    }
    if ((a & 0x0F) != mask) {
        return;
    }
    for (uint8_t i = 0; i < count; i++) {
        uint8_t x = (uint8_t)(first + i * 0x10);
        if (Enstatus[x] == 0x01) {
            EnDelay[x] = 0x08;
            return;
        }
    }
    Mem(zp)++;
}

// $9BA2
void Enemy8AI(uint8_t x) {
    uint8_t s = Enstatus[x];
    if (s < 0x03) {
        uint8_t i = 0x50;
        do {
            i = PieceCheck(i);
            i = (uint8_t)(i - 0x10);
        } while (i != 0);
        WakePieces(0x7E, 0x80, 0x10, 3, 0x0A);
        WakePieces(0x7F, 0x60, 0x40, 2, 0x00);
    } else if (s == 0x03 || s == 0x05) {
        Enstatus[0x10] = 0;
        Enstatus[0x20] = 0;
        Enstatus[0x30] = 0;
        Enstatus[0x40] = 0;
        Enstatus[0x50] = 0;
    }
    Mem(0x00) = 0x0A;
    Mem(0x01) = 0x08; // $99CC
    CommonEnemyJump();
}

// $9BD2
void Enemy9AI(uint8_t x) {
    bool alive = (En0405[x] & 0x02) && Enstatus[x] != 0x03;
    if (!alive) {
        Enstatus[x] = 0;
    } else if (!((En0405[x] << 1) & 0x80) && Enstatus[x] == 0x02) {
        if (EnemyVertSpeed8244(x)) {
            return; // the movement data ended: the original leaves this routine
        }
        x = PageIndex;
        En0402[x] = Mem(0x00);
        EnemyHorzSpeed8318(x);
        x = PageIndex;
        En0403[x] = Mem(0x00);
        if (!MoveEnemyObject(x)) {
            Enstatus[x] = 0x03;
        }
    }
    UpdateEnemyAnim(0x01);
    EnemyDraw();
}

void EnemyAI(uint8_t x) {
    uint8_t d = EnDataIndex[x];
    ChooseRoutineEffects(0x95EB, d);
    switch (d) {
        case 0x00:
            HopperAI(x, 0x09);
            break;
        case 0x01:
            HopperAI(x, 0x0F);
            break;
        case 0x02:
            if (!sBank->kraid) {
                Enemy2AI(x);
            }
            break;
        case 0x03:
            Enemy3AI(x);
            break;
        case 0x04:
            Enemy4AI(x);
            break;
        case 0x05:
            CrawlerAI(x, sBank->crawlerAnim, 0xE4, 0x08);
            break;
        case 0x06:
            if (!sBank->kraid) {
                Enemy6AI(x);
            }
            break;
        case 0x07:
            RipperAI(x, sBank->ripperSpeed);
            break;
        case 0x08:
            Enemy8AI(x);
            break;
        case 0x09:
        case 0x0A:
            Enemy9AI(x);
            break;
        default:
            if (!sBank->kraid) {
                Original(0x95E5, { 0, x, 0 }); // no routine (Brinstar's table holds $0000)
            }
            break; // Kraid's bank: $95CB, an RTS
    }
}

bool BrinstarKraidHook(uint16_t addr, const Regs& in, Regs& out);

} // namespace

bool BrinstarHook(uint16_t addr, const Regs& in, Regs& out) {
    sBank = &kBrinstar;
    return BrinstarKraidHook(addr, in, out);
}

bool KraidHook(uint16_t addr, const Regs& in, Regs& out) {
    sBank = &kKraid;
    return BrinstarKraidHook(addr, in, out);
}

namespace {

bool BrinstarKraidHook(uint16_t addr, const Regs& in, Regs& out) {
    out = in;
    switch (addr) {
        case 0x95C3: // AreaRoutine: RTS
            return true;
        case 0x95C6:
            out.a = TwosCompliment(in.a);
            return true;
        case 0x95E5:
            EnemyAI(in.x);
            return true;
        default:
            return NoAreaHook(addr, in, out);
    }
}

} // namespace

} // namespace Game
