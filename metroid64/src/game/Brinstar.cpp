// Bank 1 (Brinstar): the area's own code ($99A1-$9D35). Its area routine ($95C3 -> $9D35) is an RTS.

#include "game/Area.h"
#include "game/AreaCommon.h"
#include "game/EnemyRam.h"
#include "game/Enemies.h"
#include "game/Engine.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

namespace {

constexpr uint16_t kCrawlerAnimTbl = 0x9AA0;
constexpr uint16_t kRipperSpeedTbl = 0x9BA0;
constexpr uint16_t kPieceYTbl = 0x9CB7;
constexpr uint16_t kPieceXTbl = 0x9CBB;
constexpr uint16_t kPieceTypeTbl = 0x9CC6;

// $99A1: animate and draw the enemy, by its status ($81): $00 / $01 are the animation delays.
void CommonEnemyJump() {
    uint8_t s = Mem(0x81);
    if (s == 0x01) {
        EnemyAnimDrawOnly(Mem(0x01));
    } else if (s == 0x03) {
        EnemyDraw();
    } else {
        EnemyAnimDraw(Mem(0x00));
    }
}

// $99C8
void Delays6And8() {
    Mem(0x00) = 0x06;
    Mem(0x01) = 0x08; // $99CC
    CommonEnemyJump();
}

// $99B8 / $99D3: sidehopper-like enemies (turn toward Samus).
void HopperAI(uint8_t x, uint8_t anims) {
    Mem(0x85) = anims;
    Mem(0x86) = anims;
    if (Enstatus[x] != 0x03) {
        EnemyTurnAnim();
    }
    Delays6And8();
}

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

// $9A8F: the crawler's animation for its direction.
void CrawlerAnim(uint8_t x) {
    uint8_t y = (uint8_t)(((En040A[x] & 0x03) << 1) | (En0405[x] & 0x01));
    SetEnemyAnim(x, Read((uint16_t)(kCrawlerAnimTbl + y)));
}

// $9ABD
void CrawlerFlip(uint8_t x) {
    En0405[x] ^= 0x01;
}

// $9AE2: move one pixel in direction a (with bit 0 of $0405). Returns the carry.
bool CrawlerMove(uint8_t x, uint8_t a) {
    uint8_t d = En0405[x];
    Mem(0x00) = (uint8_t)(d >> 1);
    uint8_t index = (uint8_t)((a << 1) | (d & 0x01));
    return EnemyMoveByTable8048(index);
}

// $9AA8
void CrawlerBlocked(bool carry) {
    uint8_t x = PageIndex;
    if (carry) {
        return;
    }
    if (Mem(0x00) != 0) {
        CrawlerFlip(x);
        return;
    }
    En040A[x] = (uint8_t)((En040A[x] - 1) & 0x03);
    CrawlerAnim(x);
}

// $9AC6: around a corner?
void CrawlerCorner(uint8_t x) {
    uint8_t a = (uint8_t)((En040A[x] + 1) & 0x03); // $9ADA
    bool c = CrawlerMove(x, a);
    x = PageIndex;
    if (!c) {
        return;
    }
    En040A[x] = (uint8_t)((En040A[x] + 1) & 0x03);
    CrawlerAnim(x);
}

// $9A4C: crawlers (zoomers).
void CrawlerAI(uint8_t x) {
    bool c;
    if (EnemyFrameCarry(x, c) & 0x03) {
        uint8_t s = Mem(0x81);
        if (s == 0x01) {
            EnemyAnimDrawOnly(0x08);
            return;
        }
        if (s == 0x03) {
            EnemyDraw();
            return;
        }
        if (Enstatus[x] != 0x03) {
            uint8_t a = En040A[x] & 0x03;
            if (a == 0x01 && EnYRoomPos[x] == 0xE4) {
                CrawlerFlip(x);
                En040A[x] = 0x03;
            } else {
                CrawlerBlocked(CrawlerMove(x, a));
            }
            CrawlerCorner(PageIndex);
        }
    }
    UpdateEnemyAnimE094(0x03);
    EnemyDraw();
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

// $9CA8
void EnemyPosTo08(uint8_t x) {
    Mem(0x08) = EnYRoomPos[x];
    Mem(0x09) = EnXRoomPos[x];
    Mem(0x0B) = EnNameTable[x];
}

// $9C96
void EnemyPosFrom08(uint8_t x) {
    EnYRoomPos[x] = Mem(0x08);
    EnXRoomPos[x] = Mem(0x09);
    EnNameTable[x] = Mem(0x0B) & 0x01;
}

// $9B32: rippers.
void RipperAI(uint8_t x) {
    if (Enstatus[x] == 0x02 && En0403[x] == 0) {
        if (En6AFE[x] == 0) {
            if ((uint8_t)(ObjectY - EnYRoomPos[x]) < 0x40) {
                En6AFE[x] = 0x7F;
            }
        } else if (!(En0402[x] & 0x80)) {
            En0402[x] = 0;
            EnCounter[x] = 0;
            En6AFE[x] = 0;
            En0403[x] = Read((uint16_t)(kRipperSpeedTbl + (En0405[x] & 0x01)));
        }
    }
    if (!((En0405[x] << 1) & 0x80) && Enstatus[x] == 0x02) {
        EnemyVertAccel833F(x);
        uint8_t dy = Mem(0x00);
        EnemyHorzAccel8395(x);
        Mem(0x05) = Mem(0x00);
        Mem(0x04) = dy;
        EnemyPosTo08(x);
        if (!MoveObjectByDeltas()) {
            Enstatus[x] = 0;
            return;
        }
        EnemyPosFrom08(x);
    }
    EnemyAnimDrawOnly(0x03);
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
    Mem(0x04) = Read((uint16_t)(kPieceYTbl + y));
    EnDataIndex[x] = Read((uint16_t)(kPieceTypeTbl + y));
    y = (uint8_t)((y << 1) | (c ? 1 : 0));
    Mem(0x05) = Read((uint16_t)(kPieceXTbl + y));
    EnemyPosTo08(0);
    bool moved = MoveObjectByDeltas();
    x = PageIndex;
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
    UpdateEnemyAnimE094(0x01);
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
            Enemy2AI(x);
            break;
        case 0x03:
            Enemy3AI(x);
            break;
        case 0x04:
            Enemy4AI(x);
            break;
        case 0x05:
            CrawlerAI(x);
            break;
        case 0x06:
            Enemy6AI(x);
            break;
        case 0x07:
            RipperAI(x);
            break;
        case 0x08:
            Enemy8AI(x);
            break;
        case 0x09:
        case 0x0A:
            Enemy9AI(x);
            break;
        default:
            Original(0x95E5, { 0, x, 0 }); // no routine (the table holds $0000)
            break;
    }
}

} // namespace

bool BrinstarHook(uint16_t addr, const Regs& in, Regs& out) {
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

} // namespace Game
