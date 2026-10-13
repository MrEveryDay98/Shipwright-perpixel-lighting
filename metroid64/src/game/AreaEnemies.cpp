// Enemy routines that several area banks contain, each at its own address (the per-bank differences
// are parameters).

#include "game/AreaEnemies.h"

#include "game/AreaCommon.h"
#include "game/EnemyRam.h"
#include "game/Enemies.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

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
void RipperAI(uint8_t x, uint16_t speedTbl) {
    if (Enstatus[x] == 0x02 && En0403[x] == 0) {
        if (En6AFE[x] == 0) {
            if ((uint8_t)(ObjectY - EnYRoomPos[x]) < 0x40) {
                En6AFE[x] = 0x7F;
            }
        } else if (!(En0402[x] & 0x80)) {
            En0402[x] = 0;
            EnCounter[x] = 0;
            En6AFE[x] = 0;
            En0403[x] = Read((uint16_t)(speedTbl + (En0405[x] & 0x01)));
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

namespace {

// $9A8F: the crawler's animation for its direction.
void CrawlerAnim(uint8_t x, uint16_t animTbl) {
    uint8_t y = (uint8_t)(((En040A[x] & 0x03) << 1) | (En0405[x] & 0x01));
    SetEnemyAnim(x, Read((uint16_t)(animTbl + y)));
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
void CrawlerBlocked(bool carry, uint16_t animTbl) {
    uint8_t x = PageIndex;
    if (carry) {
        return;
    }
    if (Mem(0x00) != 0) {
        CrawlerFlip(x);
        return;
    }
    En040A[x] = (uint8_t)((En040A[x] - 1) & 0x03);
    CrawlerAnim(x, animTbl);
}

// $9AC6: around a corner?
void CrawlerCorner(uint8_t x, uint16_t animTbl) {
    uint8_t a = (uint8_t)((En040A[x] + 1) & 0x03); // $9ADA
    bool c = CrawlerMove(x, a);
    x = PageIndex;
    if (!c) {
        return;
    }
    En040A[x] = (uint8_t)((En040A[x] + 1) & 0x03);
    CrawlerAnim(x, animTbl);
}

} // namespace

// $9A4C (Brinstar): crawlers (zoomers).
void CrawlerAI(uint8_t x, uint16_t animTbl, uint8_t bottom, uint8_t onlyDelay) {
    bool c;
    if (EnemyFrameCarry(x, c) & 0x03) {
        uint8_t s = Mem(0x81);
        if (s == 0x01) {
            EnemyAnimDrawOnly(onlyDelay);
            return;
        }
        if (s == 0x03) {
            EnemyDraw();
            return;
        }
        if (Enstatus[x] != 0x03) {
            uint8_t a = En040A[x] & 0x03;
            if (a == 0x01 && EnYRoomPos[x] == bottom) {
                CrawlerFlip(x);
                En040A[x] = 0x03;
            } else {
                CrawlerBlocked(CrawlerMove(x, a), animTbl);
            }
            CrawlerCorner(PageIndex, animTbl);
        }
    }
    UpdateEnemyAnim(0x03);
    EnemyDraw();
}

} // namespace Game
