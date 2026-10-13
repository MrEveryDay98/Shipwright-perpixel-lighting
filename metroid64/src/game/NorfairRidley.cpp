// Banks 2 (Norfair, $9833-$9B9D) and 5 (Ridley's hideout, $982A-$9B48): the areas' own code. Several
// enemy routines are shared by the two banks at different addresses. Both area routines ($95C3) are an
// RTS.

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
    uint8_t animUp, animDown; // $98F4 / $98F8: resetting animation when rising / falling
    uint16_t crawlerAnim;     // $99ED / $99BE
    uint16_t ripperSpeed;     // $98BE / $98D5
    bool ridley;
};
constexpr BankTables kNorfair{ 0x25, 0x22, 0x99ED, 0x98BE, false };
constexpr BankTables kRidley{ 0x20, 0x1D, 0x99BE, 0x98D5, true };
const BankTables* sBank = &kNorfair;

constexpr uint16_t kNorfairBubbleSpeed = 0x9AD2;
constexpr uint16_t kNorfairShotAnim = 0x9B62;
constexpr uint16_t kRidleyShotX = 0x9ADF;

// $9833 / $982A
void KillEnemy(uint8_t x) {
    Enstatus[x] = 0;
}

// $98F4 / $98F8
void RiseFallAnim(uint8_t x) {
    if (Enstatus[x] == 0x02) {
        EnResetAnimIndex[x] = (En0402[x] & 0x80) ? sBank->animDown : sBank->animUp;
    }
}

// $9954 / $9958: becomes enemy type a (keeping its hit points).
void BecomeType(uint8_t x, uint8_t a) {
    EnDataIndex[x] = a;
    uint8_t hp = EnHitPoints[x];
    EnemyRestartEB6E(x);
    EnHitPoints[x] = hp;
}

// $98D3 / $98D7
void Type0AI(uint8_t x) {
    Mem(0x00) = 0x03;
    Mem(0x01) = 0x08;
    if (Enstatus[x] == 0x01 && (En0405[x] & 0x10)) {
        BecomeType(x, 0x01);
    }
    RiseFallAnim(x);
    CommonEnemyJump();
}

// $9908 / $990C
void Type1AI(uint8_t x) {
    uint8_t s = Mem(0x81);
    if (s == 0x01) {
        EnemyAnimDrawOnly(0x08);
        return;
    }
    if (s == 0x03) {
        EnemyDraw();
        return;
    }
    if (Enstatus[x] == 0x01) {
        BecomeType(x, 0x00);
        EnemyAnimDrawOnly(0x08);
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
    RiseFallAnim(x);
    EnemyAnimDraw(0x03);
}

// $98C0 (Norfair type 2)
void NorfairType2AI(uint8_t x) {
    if (Enstatus[x] != 0x03) {
        EnemyTurnAnim2();
    }
    Mem(0x00) = 0x03;
    Mem(0x01) = 0x03;
    CommonEnemyJump();
}

// $9A64 (Norfair type B): lava bubbles that jump out.
void NorfairTypeBAI(uint8_t x) {
    if (Mem(0x81) == 0x01) {
        uint8_t s = Enstatus[x];
        if (s == 0x03) {
            EnemyAnimDrawOnly(0x02);
            return;
        }
        if (s == 0x02) {
            En0402[x] = Read((uint16_t)(kNorfairBubbleSpeed + En0408[x]));
            En6AFE[x] = 0x40;
            EnCounter[x] = 0;
        }
    }
    if (Enstatus[x] == 0x03 || Mem(0x81) == 0x01) {
        EnemyAnimDrawOnly(0x02);
        return;
    }
    if (Mem(0x81) == 0x03) {
        EnemyDraw();
        return;
    }
    EnemyVertAccel833F(x);
    x = PageIndex;
    Mem(0x05) = 0;
    uint8_t y = Mem(0x00);
    Mem(0x04) = y;
    EnResetAnimIndex[x] = (y & 0x80) ? 0x1D : 0x20;
    EnemyPosTo08(x);
    bool moved = MoveObjectByDeltas();
    if (!moved || 0xE8 < Mem(0x08)) {
        Mem(0x08) = 0xE8;
        En0405[x] |= 0x20;
    }
    EnemyPosFrom08(x);
    EnemyAnimDrawOnly(0x02);
}

// $9AD6 / $9B03 (type C)
void TypeCAI(uint8_t x) {
    if (Enstatus[x] == 0x02) {
        EnemyTurnAnim2();
    }
    Mem(0x00) = 0x02;
    Mem(0x01) = 0x02;
    CommonEnemyJump();
}

// $9AE9 (Norfair type D): the dragon in the lava, spitting fireballs.
void NorfairTypeDAI(uint8_t x) {
    uint8_t a = Enstatus[x];
    if (a == 0x01) {
        EnYRoomPos[x] = 0xE8;
        a = 0xE8;
    }
    if (a == 0x02 && EnCounter[x] != 0 && En6B01[x] == 0) {
        a = FrameCount & 0x1F;
        if (a == 0) {
            if ((RandomNumber1 & 0x03) != 0) {
                Mem(0x87) = 0x02;
                Mem(0x88) = 0x00;
                Mem(0x83) = 0x43;
                Mem(0x84) = 0x47;
                Mem(0x85) = 0x03;
                EnemyFireProjectile(x);
                NoiseSFXFlag |= 0x04;
                SetEnemyAnim(x, Mem((uint16_t)(0x0083 + (En0405[x] & 0x01))));
            }
        } else if (a >= 0x0F) {
            SetEnemyAnim(x, Read((uint16_t)(kNorfairShotAnim + (En0405[x] & 0x01))));
        }
    } else if (Enstatus[x] != 0x03) {
        EnemyTurnAnim2();
    }
    Mem(0x00) = 0x01;
    Mem(0x01) = 0x01;
    CommonEnemyJump();
}

// $9B64 (Norfair type E) / $9B16 (Ridley type E): invisible spawners of lava bubbles.
void BubbleSpawnerAI(uint8_t x) {
    EnRadY[x] = 0;
    EnRadX[x] = 0;
    En0405[x] = 0x10;
    bool c = false;
    uint8_t a;
    if (!sBank->ridley) {
        uint8_t t = (uint8_t)(x << 1);
        c = t & 0x80;
        Mem(0x00) = (uint8_t)(t << 1);
        c = (x >> 3) & 0x01; // the last LSR's carry
        a = Adc((uint8_t)(x >> 4), FrameCount, c);
        a = Adc(a, Mem(0x00), c);
        if (a & 0x47) {
            return;
        }
    } else {
        c = (x >> 3) & 0x01;
        a = Adc((uint8_t)(x >> 4), FrameCount, c);
        if (a & 0x07) {
            return;
        }
    }
    En0405[x] >>= 1;
    Mem(0x87) = 0x03;
    a = RandomNumber1;
    c = a & 0x01;
    a >>= 1;
    En0405[x] = Rol(En0405[x], c);
    a &= 0x03;
    if (a == 0) {
        return;
    }
    Mem(0x88) = a;
    Mem(0x85) = 0x02;
    EnemyFireProjectile(x);
}

// $9A79 (Ridley): now and then, a fireball from Ridley's mouth.
void RidleyFireball() {
    uint8_t y = Mem(0x80);
    if (y == 0) {
        y = 0x60;
    }
    if (FrameCount & 0x02) {
        return;
    }
    y--;
    Mem(0x80) = y;
    uint8_t a = (uint8_t)(y << 1);
    if (a & 0x80) {
        return;
    }
    if ((a & 0x0F) != 0x0A) {
        return;
    }
    uint8_t x = 0x50;
    while (Enstatus[x] != 0 && (En0405[x] & 0x02)) {
        x = (uint8_t)(x - 0x10);
        if (x == 0) {
            Mem(0x7E)++; // (the original's counter is $80; it bumps $7E here)
            return;
        }
    }
    EnemyPosTo08(0);
    uint8_t d = En0405[0];
    En0405[x] = d;
    Mem(0x05) = Read((uint16_t)(kRidleyShotX + (d & 0x01)));
    Mem(0x04) = 0xF8;
    if (!MoveObjectByDeltas()) {
        return;
    }
    EnSpecialAttribs[x] = 0;
    EnDataIndex[x] = 0x0A;
    Enstatus[x] = 0x01;
    EnemyPosFrom08(x);
    EnemyRestartEB6E(x);
}

// $9A13 (Ridley type 9): Ridley.
void RidleyAI(uint8_t x) {
    uint8_t s = Enstatus[x];
    if (s < 0x03) {
        Mem(0x85) = 0x0B;
        Mem(0x86) = 0x0E;
        EnemyTurnAnim();
        RidleyFireball();
    } else if (s == 0x03 || s == 0x05) {
        Enstatus[0x10] = 0;
        Enstatus[0x20] = 0;
        Enstatus[0x30] = 0;
        Enstatus[0x40] = 0;
        Enstatus[0x50] = 0;
    }
    Mem(0x00) = 0x03;
    Mem(0x01) = 0x03;
    CommonEnemyJump();
}

// $9A4A (Ridley type A): Ridley's fireballs.
void RidleyFireballAI(uint8_t x) {
    uint8_t d = En0405[x];
    Mem(0x00) = 0x02;
    Mem(0x01) = 0x02;
    CommonEnemyJump();
    x = PageIndex;
    if ((d ^ En0405[x]) & 0x01) {
        Enstatus[x] = 0; // it bounced off something
        return;
    }
    if (En0405[x] & 0x01) {
        return;
    }
    bool c = true;
    uint8_t a = Sbc(EnXRoomPos[x], ObjectX, c);
    if (!c || a < 0x20) {
        return;
    }
    Enstatus[x] = 0;
}

void EnemyAI(uint8_t x) {
    uint8_t d = EnDataIndex[x];
    ChooseRoutineEffects(0x95EB, d);
    if (!sBank->ridley) {
        switch (d) {
            case 0x00:
                Type0AI(x);
                break;
            case 0x01:
                Type1AI(x);
                break;
            case 0x02:
                NorfairType2AI(x);
                break;
            case 0x06:
                CrawlerAI(x, sBank->crawlerAnim, 0xEB, 0x01);
                break;
            case 0x07:
                RipperAI(x, sBank->ripperSpeed);
                break;
            case 0x0B:
                NorfairTypeBAI(x);
                break;
            case 0x0C:
                TypeCAI(x);
                break;
            case 0x0D:
                NorfairTypeDAI(x);
                break;
            case 0x0E:
                BubbleSpawnerAI(x);
                break;
            default:
                KillEnemy(x);
                break;
        }
        return;
    }
    switch (d) {
        case 0x00:
            Type0AI(x);
            break;
        case 0x01:
            Type1AI(x);
            break;
        case 0x02:
            HopperAI(x, 0x42);
            break;
        case 0x03:
            HopperAI(x, 0x48);
            break;
        case 0x06:
            CrawlerAI(x, sBank->crawlerAnim, 0xEB, 0x01);
            break;
        case 0x07:
            RipperAI(x, sBank->ripperSpeed);
            break;
        case 0x09:
            RidleyAI(x);
            break;
        case 0x0A:
            RidleyFireballAI(x);
            break;
        case 0x0C:
            TypeCAI(x);
            break;
        case 0x0E:
            BubbleSpawnerAI(x);
            break;
        default:
            KillEnemy(x);
            break;
    }
}

bool NorfairRidleyHook(uint16_t addr, const Regs& in, Regs& out) {
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

bool NorfairHook(uint16_t addr, const Regs& in, Regs& out) {
    sBank = &kNorfair;
    return NorfairRidleyHook(addr, in, out);
}

bool RidleyHook(uint16_t addr, const Regs& in, Regs& out) {
    sBank = &kRidley;
    return NorfairRidleyHook(addr, in, out);
}

} // namespace Game
