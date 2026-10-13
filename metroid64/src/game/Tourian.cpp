// Bank 3 (Tourian): Metroids, Rinkas, the cannons, Mother Brain, the Zebetites and the escape timer
// ($97DC-$A32A), and the hooks at $95A8-$95C2 the engine calls.

#include "game/Area.h"
#include "game/AreaCommon.h"
#include "game/EnemyRam.h"
#include "game/Enemies.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Ram.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;

namespace {

// Tourian's variables.
inline constexpr Var CannonIndex{ 0x0097 }; // Cannon being updated (offset into $6BF4).
inline constexpr Var MBrainNameTable{ 0x009D };
inline constexpr Var MBrainHitFlag{ 0x009E }; // Set by a missile hitting Mother Brain ($95C0 hook).
inline constexpr Var MBrainTimer{ 0x009F };
inline constexpr Var MBrainEyeDelay{ 0x009A };
inline constexpr Var MBrainBrainDelay{ 0x009B };
inline constexpr Var MBrainEye{ 0x009C };
inline constexpr Var RinkaSpawn{ 0x008B };   // Two Rinka spawners at $8B and $8E: status, name table, index.
inline constexpr Var MetroidState{ 0x77F8 }; // One byte per Metroid (slot / 16): latched on Samus etc.
inline constexpr Var MetroidMaxSpeed{ 0x77F6 };
inline constexpr Var MetroidAccel{ 0x77F2 };
inline constexpr Var MetroidBounce{ 0x77F0 };
inline constexpr Var CannonStatus{ 0x6BF4 }; // 16 cannons, 8 bytes each.
inline constexpr Var CannonY{ 0x6BF5 };
inline constexpr Var CannonX{ 0x6BF6 };
inline constexpr Var CannonHi{ 0x6BF7 };
inline constexpr Var CannonType{ 0x6BF8 };
inline constexpr Var CannonFrame{ 0x6BF9 };
inline constexpr Var CannonDelay{ 0x6BFA };
inline constexpr Var CannonStep{ 0x6BFB };
inline constexpr Var EscapeShown{ 0x010D };
inline constexpr Var EscapeHi{ 0x010C };
inline constexpr Var ZebStatus{ 0x0758 }; // Five Zebetites, 8 bytes each.
inline constexpr Var ZebWRAMLo{ 0x0759 };
inline constexpr Var ZebWRAMHi{ 0x075A };
inline constexpr Var ZebDamage{ 0x075B };
inline constexpr Var ZebTimer{ 0x075C };
inline constexpr Var ZebHit{ 0x075D };

constexpr uint16_t kMetroidStepTbl = 0x99D7;
constexpr uint16_t kCannonDelayTbl = 0x9D8F;
constexpr uint16_t kCannonSeqStart = 0x9D94;
constexpr uint16_t kCannonSeqTbl = 0x9D99;
constexpr uint16_t kCannonFrameTbl = 0x9DC6;
constexpr uint16_t kCannonShotAnim = 0x9C26;
constexpr uint16_t kCannonShotX = 0x9DCC;
constexpr uint16_t kCannonShotY = 0x9DCF;
constexpr uint16_t kMBrainPalTbl = 0x9E41;
constexpr uint16_t kMBrainDeathPal = 0x9F00;
constexpr uint16_t kMBrainBlastPos = 0x9F39;
constexpr uint16_t kMBrainBlastFrame = 0x9F41;
constexpr uint16_t kDoorSlotTbl = 0x9F65;
constexpr uint16_t kEyeFrameTbl = 0xA06D;
constexpr uint16_t kBrainFrame = 0xA071;
constexpr uint16_t kMBrainHitTiles = 0xA0A3;
constexpr uint16_t kMBrainHitStart = 0xA0C0;
constexpr uint16_t kRinkaPosTbl = 0xA1DB;
constexpr uint16_t kZebFrameTbl = 0xA310;
constexpr uint16_t kPieceTileFrame = 0x9D3C;

uint8_t Lsr4(uint8_t a) {
    return a >> 4; // $9B1B
}
uint8_t Asl4(uint8_t a) {
    return (uint8_t)(a << 4); // $9B20
}

// $9D88: the name table the room's right half is on.
uint8_t OtherNameTable() {
    return (PPUCNT0ZP ^ ScrollDir) & 0x01;
}

// $97E2
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

// $99F4
void EnemyPosFrom08(uint8_t x) {
    EnXRoomPos[x] = Mem(0x09);
    EnYRoomPos[x] = Mem(0x08);
    EnNameTable[x] = Mem(0x0B) & 0x01;
}

// $99C3
void ClearSpeeds(uint8_t x) {
    En0402[x] = 0;
    En0403[x] = 0;
    En0407[x] = 0;
    EnCounter[x] = 0;
    En6AFF[x] = 0;
    En6AFE[x] = 0;
}

// $9A06: acceleration from $77F2 by bit 0 of a and the Metroid's pattern.
uint8_t MetroidAccelFor(uint8_t x, uint8_t a) {
    uint8_t y = (uint8_t)((En0408[x] << 1) | (a & 0x01));
    return MetroidAccel[y];
}

// $9A10: bounce speed from $77F0 by the sign of a.
uint8_t MetroidBounceFor(uint8_t a) {
    return MetroidBounce[(a >> 7) & 0x01];
}

// $9822-$986C: cap a speed at the Metroid's maximum.
void CapSpeed(Var speed, Var frac, uint8_t x, uint8_t y) {
    uint8_t max = MetroidMaxSpeed[y];
    uint8_t keep = max;
    uint8_t a = speed[x];
    if (a & 0x80) {
        keep = TwosCompliment(max);
        bool c = 0 >= frac[x]; // CMP: carry when the fraction is 0
        a = Sbc(0x00, speed[x], c);
    }
    if (a >= max) {
        speed[x] = keep;
        frac[x] = 0;
    }
}

// $97F9: Metroids.
void MetroidAI(uint8_t x) {
    if ((uint8_t)(EndTimerHi + 1) != 0) {
        Enstatus[x] = 0; // gone during the escape
    }
    Mem(0x00) = 0x0F;
    Mem(0x01) = 0x0F;
    if (((En0405[x] << 1) & 0x80) || Enstatus[x] == 0x03) {
        CommonEnemyJump();
        return;
    }
    uint8_t m = x >> 4;
    if (MetroidState[m] == 0) {
        uint8_t y = En0408[x];
        CapSpeed(En0402, EnCounter, x, y);
        CapSpeed(En0403, En0407, x, y);
        uint8_t d = En0405[x];
        En6AFF[x] = MetroidAccelFor(x, d);
        En6AFE[x] = MetroidAccelFor(x, (uint8_t)(d >> 2));
        if (Enstatus[x] == 0x04) {
            if (EnHitPoints[x] == 0xFF) {
                EnHitPoints[x] = 0x05;
            }
        } else {
            EnHitPoints[x] = 0xFF;
        }
    }
    // $9899
    if (Mem(0x81) == 0x06 && Enstatus[x] != 0x06) {
        Enstatus[x] = 0x04;
    }
    uint8_t y = 0;
    bool latched = false; // $9932 reached with y = the Metroid's index
    if (En0404[x] & 0x20) {
        y = x >> 4;
        bool release = true;
        if (MetroidState[y] != 0) {
            uint8_t w = En040E[x];
            release = false;
            latched = true;
            if ((w == 0x07 || w == 0x0A) && !(FrameCount & 0x02)) {
                // Bombed while on Samus: the fifth bomb shakes it off.
                MetroidState[y] = (uint8_t)(MetroidState[y] + 0x10);
                if ((MetroidState[y] & 0x70) == 0x50) {
                    En040C[x] = 0x02 | EnSpecialAttribs[x];
                    Enstatus[x] = 0x06;
                    EnSpecialAttribs[x] = 0x20;
                    En040D[x] = 0x01;
                    release = true;
                    latched = false;
                }
            }
        }
        if (release) {
            // $98EF
            En0404[x] = 0;
            MetroidState[y] = 0;
            EnCounter[x] = 0;
            En0407[x] = 0;
            En0402[x] = MetroidBounceFor(En6AFE[x]);
            En0403[x] = MetroidBounceFor(En6AFF[x]);
        }
    }
    if (!latched) {
        // $990F
        y = x >> 4;
        if (MetroidState[y] != 0) {
            latched = true;
        } else if (En0404[x] & 0x04) {
            // It caught Samus.
            y = (uint8_t)((En0403[x] & 0x80) | 0x01);
            ClearSpeeds(x);
            uint8_t i = x >> 4;
            MetroidState[i] = y;
            y = i;
            latched = true;
        } else {
            MetroidState[x >> 4] = 0; // $9964
        }
    }
    if (latched) {
        // $9932: stick to Samus, closing in.
        uint8_t i = y;
        uint8_t st = MetroidState[i];
        bool neg = st & 0x80;
        uint8_t a = st & 0x0F;
        if (a != 0x0C) {
            MetroidState[i]++;
        }
        Mem(0x04) = Read((uint16_t)(kMetroidStepTbl + a));
        Mem(0x05) = a;
        a = (uint8_t)(0x0C - Mem(0x05));
        x = PageIndex;
        if (!neg) {
            a = TwosCompliment(a);
        }
        Mem(0x05) = a;
        Mem(0x09) = ObjectX;
        Mem(0x08) = ObjectY;
        Mem(0x0B) = ObjectHi;
        MoveObjectByDeltas();
        EnemyPosFrom08(x);
    }
    // $9967
    if (Enstatus[x] == 0x03) {
        MetroidState[x >> 4] = 0;
    }
    uint8_t all =
        MetroidState[0] | MetroidState[1] | MetroidState[2] | MetroidState[3] | MetroidState[4] | MetroidState[5];
    y = 0;
    if ((all & 0x0C) == 0x0C && (HealthLo | HealthHi) != 0) {
        HealthHiChange = 0;
        HealthLoChange = 0x04;
        SubtractHealth();
        y = 0x01;
    }
    MetroidOnSamus = y;
    if (!(ObjectCntrl & 0x80)) {
        ObjectCntrl = EnDataIndex[x] | 0xA2;
    }
    CommonEnemyJump();
}

// $9AF9: speeds from the distances in $00/$01 (4.4 fixed point).
void SpeedsFromDistance(uint8_t x) {
    En0402[x] = Lsr4(Mem(0x00));
    EnCounter[x] = Asl4(Mem(0x00));
    En0403[x] = Lsr4(Mem(0x01));
    En0407[x] = Asl4(Mem(0x01));
}

// $9A2C: Rinkas fly straight at Samus.
void RinkaAI(uint8_t x) {
    if (Enstatus[x] == 0x02 && Mem(0x81) == 0x01) {
        // Just started: aim.
        En6AFF[x] = 0; // $99D1
        En6AFE[x] = 0;
        En6AFC[x] = 0;
        En6AFD[x] = 0;
        bool c = true;
        Mem(0x01) = Sbc(ObjectX, EnXRoomPos[x], c);
        uint8_t d = En0405[x];
        if (d & 0x01) {
            c = true;
            Mem(0x01) = Sbc(0, Mem(0x01), c);
        }
        c = true;
        Mem(0x00) = Sbc(ObjectY, EnYRoomPos[x], c);
        if (d & 0x04) {
            c = true;
            Mem(0x00) = Sbc(0, Mem(0x00), c);
        }
        uint8_t a = Mem(0x00) | Mem(0x01);
        uint8_t y = 0x03;
        for (;;) {
            bool top = a & 0x80;
            a = (uint8_t)(a << 1);
            if (top) {
                break;
            }
            if (--y == 0) {
                break;
            }
        }
        for (;;) {
            y--;
            if (y & 0x80) {
                break;
            }
            Mem(0x00) >>= 1;
            Mem(0x01) >>= 1;
        }
        SpeedsFromDistance(x);
        if (d & 0x01) {
            c = true;
            En0407[x] = Sbc(0, En0407[x], c);
            En0403[x] = Sbc(0, En0403[x], c);
        }
        if (d & 0x04) {
            c = true;
            EnCounter[x] = Sbc(0, EnCounter[x], c);
            En0402[x] = Sbc(0, En0402[x], c);
        }
    }
    // $9AB0
    if (!((En0405[x] << 1) & 0x80)) {
        bool c = false;
        En6AFC[x] = Adc(EnCounter[x], En6AFC[x], c);
        Mem(0x04) = Adc(En0402[x], 0x00, c);
        c = false;
        En6AFD[x] = Adc(En0407[x], En6AFD[x], c);
        Mem(0x05) = Adc(En0403[x], 0x00, c);
        Mem(0x08) = EnYRoomPos[x];
        Mem(0x09) = EnXRoomPos[x];
        Mem(0x0B) = EnNameTable[x];
        if (!MoveObjectByDeltas()) {
            Enstatus[x] = 0;
        }
        EnemyPosFrom08(x);
    }
    EnemyAnimDrawOnly(0x08);
}

void EnemyAI(uint8_t x) {
    uint8_t d = EnDataIndex[x];
    ChooseRoutineEffects(0x95EB, d);
    switch (d) {
        case 0x00:
        case 0x01:
            MetroidAI(x);
            break;
        case 0x02:
            EnemyAnimDrawOnly(0x01); // $9A27
            break;
        case 0x04:
            RinkaAI(x);
            break;
        default:
            Enstatus[x] = 0; // $97DC
            break;
    }
}

// --- Cannons ($9B37) -----------------------------------------------------------------------------------

// $9C4D: Y = 1 when the cannon is on screen.
uint8_t CannonVisible(uint8_t x) {
    bool c = CannonX[x] >= ScrollX;
    if (!(ScrollDir & 0x02)) {
        c = CannonY[x] >= ScrollY;
    }
    bool same = ((CannonHi[x] ^ PPUCNT0ZP) & 0x01) == 0;
    return (same ? !c : c) ? 0x01 : 0x00;
}

// $9BAF: fires a shot (direction a) from cannon $97.
void CannonFire(uint8_t a) {
    uint8_t x = CannonIndex;
    if (MthrBrainStatus >= 0x04) {
        return;
    }
    uint8_t y = 0x60;
    while (Enstatus[y] != 0) {
        y = (uint8_t)(y + 0x10);
        if (y == 0xA0) {
            return;
        }
    }
    PageIndex = y;
    EnYRoomPos[y] = CannonY[x];
    EnXRoomPos[y] = CannonX[x];
    EnNameTable[y] = CannonHi[x];
    Enstatus[y] = 0x02;
    EnDelay[y] = 0;
    EnAnimDelay[y] = 0;
    En0408[y] = 0;
    a = TwosCompliment(a);
    x = a;
    En040A[y] = a;
    En0405[y] = a | 0x02;
    a = Read((uint16_t)(kCannonShotAnim + x));
    EnResetAnimIndex[y] = a;
    EnAnimIndex[y] = a;
    Mem(0x05) = Read((uint16_t)(kCannonShotX + x));
    Mem(0x04) = Read((uint16_t)(kCannonShotY + x));
    x = CannonIndex;
    Mem(0x08) = CannonY[x];
    Mem(0x09) = CannonX[x];
    Mem(0x0B) = CannonHi[x];
    MoveObjectByDeltas();
    EnemyPosFrom08(y);
}

// $9B70: the cannon's animation sequence.
void CannonAnimate(uint8_t x) {
    uint8_t y = CannonType[x];
    if (CannonDelay[x] == 0) {
        CannonDelay[x] = Read((uint16_t)(kCannonDelayTbl + y));
        CannonStep[x]++;
    }
    CannonDelay[x]--;
    for (;;) {
        y = (uint8_t)(Read((uint16_t)(kCannonSeqStart + y)) + CannonStep[x]);
        uint8_t a = Read((uint16_t)(kCannonSeqTbl + y));
        if (!(a & 0x80)) {
            CannonFrame[x] = a;
            return;
        }
        if (a == 0xFF) {
            y = CannonType[x];
            CannonStep[x] = 0;
            continue;
        }
        CannonStep[x]++;
        CannonFire(a);
        x = CannonIndex;
        y = CannonType[x];
    }
}

// $9C31: draws the cannon with frame a (as the enemy in slot $E0). Returns true when DrawEnemy pops.
bool CannonDraw(uint8_t x, uint8_t frame) {
    EnAnimFrame[0xE0] = frame;
    EnYRoomPos[0xE0] = CannonY[x];
    EnXRoomPos[0xE0] = CannonX[x];
    EnNameTable[0xE0] = CannonHi[x];
    PageIndex = 0xE0;
    return DrawEnemy();
}

// $9B44. Returns true when DrawEnemy popped the return address.
bool DoOneCannon(uint8_t x) {
    CannonIndex = x;
    if (CannonStatus[x] == 0) {
        return false;
    }
    if (CannonVisible(x) != 0) {
        return false;
    }
    if ((uint8_t)(EndTimerHi + 1) != 0) {
        if (FrameCount & 0x02) {
            return false;
        }
        return CannonDraw(x, 0x19); // $9B65: during the escape
    }
    if (CannonType[x] == 0x05) {
        return false;
    }
    CannonAnimate(x);
    x = CannonIndex;
    return CannonDraw(x, Read((uint16_t)(kCannonFrameTbl + CannonFrame[x]))); // $9C2B
}

// $9B37. Returns true when the original leaves the area routine (the pop in the last, fallen-into slot).
bool UpdateCannons() {
    uint8_t x = 0x78;
    for (;;) {
        if (DoOneCannon(x)) {
            return false;
        }
        x = (uint8_t)(CannonIndex - 0x08);
        if (x == 0) {
            break;
        }
    }
    return DoOneCannon(0x00);
}

// --- Mother Brain ($9DD4) ----------------------------------------------------------------------------

// $9DF2: touching Mother Brain hurts.
void MBrainTouch() {
    if ((ObjectHi ^ MBrainNameTable) != 0) {
        return;
    }
    if ((uint8_t)(ObjectX - 0x48) >= 0x2F) {
        return;
    }
    uint8_t a = (uint8_t)(ObjectY - 0x80);
    if (a & 0x80) {
        a = TwosCompliment(a);
    }
    if (a >= 0x20) {
        return;
    }
    HealthLoChange = 0x00;
    HealthHiChange = 0x02;
    SamusHit = 0x38;
    SubtractHealth();
}

// $9E43
uint8_t MBrainCountdown() {
    if (--MBrainTimer == 0) {
        MthrBrainStatus = 0x01;
    }
    return (MBrainTimer & 0x02) >> 1;
}

// $9FED: hit by a missile.
void MBrainHit() {
    if (MBrainHitFlag == 0) {
        return;
    }
    MultiSFXFlag |= 0x02;
    MotherBrainHits++;
    bool dead = MotherBrainHits >= 0x20;
    uint8_t y = 0x02;
    uint8_t a = 0x10;
    if (dead) {
        for (uint8_t x = 0; x != 0xD0; x = (uint8_t)(x + 0x10)) {
            TileRoutine[x] = 0;
        }
        y++;
        a = 0x80;
    }
    MthrBrainStatus = y;
    MBrainTimer = a;
}

// $A01B
void MBrainEyeTimer() {
    if (--MBrainEyeDelay != 0) {
        return;
    }
    MBrainEye = RandomNumber1 & 0x03;
    MBrainEyeDelay = (uint8_t)((0x20 - MotherBrainHits) >> 1);
}

// $A02E
void MBrainBrainTimer() {
    MBrainBrainDelay--;
    if ((uint8_t)(MBrainBrainDelay << 1) != 0) {
        return;
    }
    MBrainBrainDelay = (uint8_t)(((0x20 - MotherBrainHits) | 0x80) ^ MBrainBrainDelay);
}

// $A041: the eyes and brain (drawn as the enemy in slot $E0).
void MBrainDraw() {
    PageIndex = 0xE0;
    EnNameTable[0xE0] = MBrainNameTable;
    EnYRoomPos[0xE0] = 0x70;
    EnXRoomPos[0xE0] = 0x48;
    EnAnimFrame[0xE0] = Read((uint16_t)(kEyeFrameTbl + MBrainEye));
    if (DrawEnemy()) {
        return;
    }
    if (MBrainBrainDelay & 0x80) {
        return;
    }
    EnAnimFrame[0xE0] = Read(kBrainFrame);
    DrawEnemy();
}

// $9E31
void MBrainClearHit() {
    MBrainHitFlag = 0;
}

// $A072: blast the tile Mother Brain was hit at.
void MBrainHitTiles() {
    uint8_t y = MotherBrainHits;
    if (y == 0) {
        return;
    }
    y = (uint8_t)(Read((uint16_t)(kMBrainHitStart + y)) + MBrainEyeDelay);
    uint8_t a = Read((uint16_t)(kMBrainHitTiles + y));
    if (a == 0xFF) {
        MBrainEyeDelay--;
        return;
    }
    bool c = false;
    TileWRAMLo = Adc(a, 0x44, c);
    uint8_t hi = (uint8_t)((MBrainNameTable << 2) | 0x61);
    TileWRAMHi = Adc(hi, 0x00, c);
    TileAnimFrame = 0;
    PageIndex = 0;
    DrawTileBlast();
}

// $9F69: opens the room's door (slot $90 or $B0). Returns DrawTileBlast's carry.
bool MBrainOpenDoor() {
    bool c = false;
    uint8_t a = Adc(MapPosX, MapPosY, c);
    a = (uint8_t)((a << 1) | 0x01);
    uint8_t x = Read((uint16_t)(kDoorSlotTbl + (a & 0x03)));
    SamusJmpDsplcmnt[x] = 0x01;
    Mem((uint16_t)(0x0307 + x)) = 0x01;
    ObjAction[x] = 0x03;
    ObjectHi[x] = MBrainNameTable;
    ObjectX[x] = 0x10;
    ObjectY[x] = 0x68;
    AnimResetIndex[x] = 0x55;
    AnimIndex[x] = 0x55;
    AnimDelay[x] = 0;
    AnimFrame[x] = 0xF7;
    TileAnimFrame = 0x10;
    TileWRAMLo = 0x40;
    TileWRAMHi = (uint8_t)((MBrainNameTable << 2) | 0x61);
    PageIndex = 0;
    return DrawTileBlast();
}

void UpdateMotherBrain() {
    uint8_t s = MthrBrainStatus;
    if (s == 0) {
        return;
    }
    ChooseRoutineEffects(0x9DDB, s);
    switch (s) {
        case 0x01: // $9E22: alive
            MBrainTouch();
            MBrainHit();
            MBrainEyeTimer();
            MBrainBrainTimer();
            MBrainDraw();
            MBrainClearHit();
            break;
        case 0x02: { // $9E36: flashing after a hit
            uint8_t y = MBrainCountdown();
            PalDataPending = Read((uint16_t)(kMBrainPalTbl + y));
            MBrainClearHit();
            break;
        }
        case 0x03: { // $9E52: dying, shaking
            uint8_t y = MBrainCountdown();
            PalDataPending = Read((uint16_t)(kMBrainPalTbl + y));
            ScrollY = (uint8_t)(y << 2);
            if ((uint8_t)(MthrBrainStatus - 1) == 0) {
                MotherBrainHits = 0;
                for (uint8_t x = 0; x != 0xC0; x = (uint8_t)(x + 0x10)) {
                    Enstatus[x] = 0;
                }
                MthrBrainStatus = 0x04;
                MBrainTimer = 0x28;
                NoiseSFXFlag |= 0x01;
            }
            MBrainDraw(); // $9E2E
            MBrainClearHit();
            break;
        }
        case 0x04: { // $9E86: disintegrating
            NoiseSFXFlag |= 0x10;
            MBrainHitTiles();
            MBrainEyeDelay++;
            uint8_t y = MBrainCountdown();
            for (uint8_t x = 0; x != 0x40; x = (uint8_t)(x + 0x10)) {
                if (Enstatus[x] == 0x05) {
                    Enstatus[x] = 0;
                }
            }
            if (PPUStrIndex == 0) {
                PalDataPending = Read((uint16_t)(kMBrainDeathPal + y));
            }
            if ((uint8_t)(MthrBrainStatus - 1) != 0) {
                break;
            }
            MBrainEyeDelay = 0;
            MthrBrainStatus = 0x04;
            MBrainTimer = 0x1C;
            uint8_t hits = MotherBrainHits++;
            if (hits == 0x04) {
                MBrainTimer >>= 1;
            } else if (hits > 0x04) {
                MusicInitFlag |= 0x04; // $9ED6: the escape
                MthrBrainStatus = 0x05;
                MotherBrainHits = 0x80;
            }
            break;
        }
        case 0x05:
        case 0x08: { // $9F02: erase Mother Brain, tile by tile
            uint8_t a = MotherBrainHits;
            if (a & 0x80) {
                MotherBrainHits++;
                break;
            }
            if (a == 0x08) {
                MthrBrainStatus++;
                break;
            }
            uint8_t y = a;
            TileAnimFrame = Read((uint16_t)(kMBrainBlastFrame + y));
            bool c = false;
            TileWRAMLo = Adc(Read((uint16_t)(kMBrainBlastPos + y)), 0x42, c);
            TileWRAMHi = Adc((uint8_t)(MBrainNameTable << 2), 0x61, c);
            PageIndex = 0;
            if (PPUStrIndex != 0) {
                break;
            }
            if (DrawTileBlast()) {
                break;
            }
            MotherBrainHits++;
            break;
        }
        case 0x06: // $9F49: the escape starts
            if (MBrainOpenDoor()) {
                break;
            }
            MthrBrainStatus = 0;
            EndTimerLo = 0x99;
            EndTimerHi = 0x99;
            EscapeShown = 0x01;
            EscapeHi = MBrainNameTable;
            break;
        case 0x07: // $9FC0: time ran out
            NoiseSFXFlag |= 0x10;
            if (Timer3 == 0) {
                ObjAction = 0x08;
                MthrBrainStatus = 0x0A;
                PalDataPending = 0x01;
            }
            break;
        case 0x09: // $9FDA: back in the room after Mother Brain is dead
            if (MBrainOpenDoor()) {
                break;
            }
            EscapeHi = MBrainNameTable;
            EscapeShown = 0x01;
            MthrBrainStatus = 0;
            break;
        default:
            break;
    }
}

// --- Escape timer ($A1E7, $A238) ---------------------------------------------------------------------

void EscapeTimer() {
    if ((uint8_t)(EndTimerHi + 1) == 0) {
        return;
    }
    Mem(0x03) = EndTimerLo;
    bool c = true;
    EndTimerLo = Base10Subtract(0x01, c);
    Mem(0x03) = EndTimerHi;
    EndTimerHi = Base10Subtract(0x00, c);
    if ((FrameCount & 0x1F) == 0) {
        SQ1SFXFlag |= 0x08;
    }
    if ((EndTimerLo | EndTimerHi) != 0) {
        return;
    }
    EndTimerHi--;
    MotherBrainHits = 0;
    MthrBrainStatus = 0x07;
    NoiseSFXFlag |= 0x01;
    Timer3 = 0x0C;
    PalDataPending = 0x0B;
}

// $A238: the timer display (a sprite drawn as enemy $E0, its digits patched in).
void EscapeTimerDraw() {
    if (EscapeShown == 0) {
        return;
    }
    EnNameTable[0xE0] = EscapeHi;
    EnYRoomPos[0xE0] = 0x84;
    EnXRoomPos[0xE0] = 0x64;
    EnAnimFrame[0xE0] = 0x1A;
    PageIndex = 0xE0;
    uint8_t p = SpritePagePos;
    if (DrawEnemy()) {
        return;
    }
    if (p == SpritePagePos) {
        return;
    }
    uint8_t x = p;
    SpriteRAM[(uint8_t)(x + 1)] = (uint8_t)((((uint8_t)((EndTimerHi >> 3) >> 1) | 0x80) & 0x0F) | 0xA0);
    SpriteRAM[(uint8_t)(x + 5)] = (uint8_t)((EndTimerHi & 0x0F) | 0xA0);
    SpriteRAM[(uint8_t)(x + 9)] = (uint8_t)((((uint8_t)((EndTimerLo >> 3) >> 1) | 0x80) & 0x0F) | 0xA0);
}

// --- Rinka spawners ($A15E) ----------------------------------------------------------------------------

void RinkaSpawner(uint8_t y) {
    PageIndex = y;
    if (RinkaSpawn[y] & 0x80) {
        return;
    }
    if (!((RinkaSpawn[y + 1] ^ FrameCount) & 0x01)) {
        return;
    }
    if (MthrBrainStatus >= 0x04) {
        return;
    }
    if (FrameCount & 0x06) {
        return;
    }
    int x = 0x20;
    while (Enstatus[x] != 0 && (En0405[x] & 0x02)) {
        x -= 0x10;
        if (x < 0) {
            return;
        }
    }
    Enstatus[x] = 0x01;
    EnDataIndex[x] = 0x04;
    EnSpecialAttribs[x] = 0;
    En0404[x] = 0;
    EnemyRestartEB6E((uint8_t)x);
    EnAnimFrame[x] = 0xF7;
    y = PageIndex;
    EnNameTable[x] = RinkaSpawn[y + 1];
    uint8_t i = (uint8_t)((RinkaSpawn[y + 2] << 1) | RinkaSpawn[y]);
    uint8_t pos = Read((uint16_t)(kRinkaPosTbl + i));
    EnYRoomPos[x] = (uint8_t)((pos & 0xF0) | 0x07); // $9EE7
    EnXRoomPos[x] = (uint8_t)((pos << 4) | 0x07);
    uint8_t s = PageIndex;
    if (++RinkaSpawn[s + 2] == 0x06) {
        RinkaSpawn[s + 2] = 0;
    }
}

// --- Zebetites ($A28B) ------------------------------------------------------------------------------------

void DoOneZebetite(uint8_t x) {
    if ((ZebStatus[x] & 0x0F) != 0x01) {
        return;
    }
    uint8_t y;
    bool draw = false;
    if (ZebHit[x] != 0) {
        uint8_t a = ++ZebDamage[x];
        if (!(a & 0x01)) {
            a >>= 1;
            y = a;
            bool c = false;
            if (Sbc(a, 0x03, c) == 0) {
                ZebStatus[x]++; // destroyed
            }
            draw = true;
        }
    }
    if (!draw) {
        // $A2F2: regrow.
        y = ZebDamage[x];
        if (y == 0 || --ZebTimer[x] != 0) {
            ZebHit[x] = 0;
            return;
        }
        ZebTimer[x] = 0x40;
        y--;
        ZebDamage[x] = y;
        bool odd = y & 0x01;
        y >>= 1;
        if (odd) {
            ZebHit[x] = 0;
            return;
        }
    }
    // $A2BA
    Mem(0x0513) = Read((uint16_t)(kZebFrameTbl + y)); // TileAnimFrame of tile slot $10
    Mem(0x0518) = ZebWRAMLo[x];
    Mem(0x0519) = ZebWRAMHi[x];
    bool full = true;
    if (PPUStrIndex == 0) {
        full = DrawTileBlast();
    }
    if (full) {
        uint8_t a = (uint8_t)((ZebStatus[x] & 0x80) | 0x01);
        ZebStatus[x] = a;
        ZebHit[x] = a;
        ZebDamage[x]--;
        return;
    }
    ZebTimer[x] = 0x40;
    ZebHit[x] = 0;
}

void UpdateZebetites() {
    PageIndex = 0x10;
    for (uint8_t x = 0x20; x != 0; x = (uint8_t)(x - 0x08)) {
        DoOneZebetite(x);
    }
    DoOneZebetite(0x00);
}

// $9B25 AreaRoutine
void TourianAreaRoutine() {
    if (UpdateCannons()) {
        return;
    }
    UpdateMotherBrain();
    EscapeTimer();
    EscapeTimerDraw();
    UpdateZebetites();
    if ((uint8_t)(EndTimerHi + 1) != 0) {
        return; // $A15E: no Rinkas during the escape
    }
    RinkaSpawner(0x03);
    RinkaSpawner(0x00);
}

// --- Hooks ---------------------------------------------------------------------------------------------

// $9CD6
void ForgetRinkaSpawner(uint8_t x) {
    if (RinkaSpawn[x] & 0x80) {
        return;
    }
    if ((RinkaSpawn[x + 1] ^ Mem(0x02)) & 0x01) {
        return;
    }
    RinkaSpawn[x] = 0xFF;
}

// $9D64
uint8_t ZebWRAMHiFor() {
    return (uint8_t)((OtherNameTable() << 2) | 0x61);
}

// $95AE -> $9C6F: forget the objects on name table y (the room left behind).
void ClearNameTableObjects(uint8_t y) {
    Mem(0x02) = y;
    for (uint8_t i = 0; !(i & 0x80); i = (uint8_t)(i + 0x08)) {
        if (!((CannonHi[i] ^ Mem(0x02)) & 0x01)) {
            CannonStatus[i] = 0;
        }
    }
    for (uint8_t x = 0; x != 0x28; x = (uint8_t)(x + 0x08)) {
        if (ZebStatus[x] != 0 && (ZebWRAMHiFor() ^ ZebWRAMHi[x]) == 0) {
            ZebStatus[x] = 0;
        }
    }
    ForgetRinkaSpawner(0x00);
    ForgetRinkaSpawner(0x03);
    uint8_t s = MthrBrainStatus;
    if (s != 0 && s != 0x07 && s != 0x0A && !((MBrainNameTable ^ Mem(0x02)) & 0x01)) {
        MthrBrainStatus = 0;
    }
    if (EscapeShown != 0 && !((EscapeHi ^ Mem(0x02)) & 0x01)) {
        EscapeShown = 0;
    }
}

uint16_t Ptr00() {
    return (uint16_t)(Mem(0x00) | (Mem(0x01) << 8));
}

// $95B1 -> $9CE6: a cannon from the room data at ($00),y. Returns X and Y.
Regs AddCannon(Regs in) {
    Regs out = in;
    uint8_t x = 0;
    while (CannonStatus[x] != 0) {
        x = (uint8_t)(x + 0x08);
        if (x & 0x80) {
            out.x = x;
            return out;
        }
    }
    uint8_t y = in.y;
    CannonType[x] = Lsr4(Read((uint16_t)(Ptr00() + y)));
    CannonStatus[x] = 0x01;
    CannonStep[x] = 0x01;
    y++;
    uint8_t a = Read((uint16_t)(Ptr00() + y));
    CannonY[x] = (uint8_t)((a & 0xF0) | 0x07);
    CannonX[x] = (uint8_t)(Asl4(a) | 0x07);
    CannonHi[x] = OtherNameTable();
    out.x = x;
    out.y = y;
    out.a = CannonHi[x];
    return out;
}

// $95B4 -> $9D21: Mother Brain's room.
Regs AddMotherBrain(Regs in) {
    Regs out = in;
    MthrBrainStatus = 0x01;
    uint8_t a = OtherNameTable();
    MBrainNameTable = a;
    uint8_t x = a ^ 0x01;
    Mem((uint16_t)(0x6C + x)) |= Read(kPieceTileFrame);
    MBrainEyeDelay = 0x20;
    MBrainBrainDelay = 0x20;
    out.x = x;
    out.a = 0x20;
    return out;
}

// $95B7 -> $9D3D: a Zebetite from the room data at ($00),y.
Regs AddZebetite(Regs in) {
    Regs out = in;
    uint8_t a = Read((uint16_t)(Ptr00() + in.y)) & 0xF0;
    uint8_t x = a >> 1;
    a = (uint8_t)((((uint8_t)(x << 1) & 0x10) ^ 0x10) | 0x84);
    ZebWRAMLo[x] = a;
    ZebWRAMHi[x] = ZebWRAMHiFor();
    ZebStatus[x] = 0x01;
    ZebDamage[x] = 0;
    ZebTimer[x] = 0;
    ZebHit[x] = 0;
    out.x = x;
    out.a = 0;
    return out;
}

// $95BA -> $9D6C: a Rinka spawner from the room data at ($00),y.
Regs AddRinkaSpawner(Regs in) {
    Regs out = in;
    for (uint8_t x : { (uint8_t)0x03, (uint8_t)0x00 }) {
        out.x = x;
        if (!(RinkaSpawn[x] & 0x80)) {
            out.a = RinkaSpawn[x];
            if (x == 0x03) {
                continue;
            }
            return out;
        }
        RinkaSpawn[x] = Lsr4(Read((uint16_t)(Ptr00() + in.y)));
        RinkaSpawn[x + 1] = OtherNameTable();
        out.a = 0xFF;
        return out;
    }
    return out;
}

// $95BD -> $A0C6: a missile hit a blastable tile (A = tile, Y = tile). Tourian's tiles never blast the
// usual way: the routine always pops BlastTile's return address and returns carry clear to its caller.
// Signalled here with y = $FF (see Area.h).
Regs BlastTileHook(Regs in) {
    Regs out = in;
    out.c = false;
    out.y = 0xFF;
    if (UpdtngPrjctl == 0) {
        return out;
    }
    uint8_t x = PageIndex;
    out.x = x;
    if (ObjAction[x] != 0x0B) {
        return out;
    }
    if (in.y == 0x98) {
        // A missile hitting Mother Brain's glass.
        x = 0;
        while (TileRoutine[x] != 0) {
            x = (uint8_t)(x + 0x10);
            if (x == 0xD0) {
                out.x = x;
                return out;
            }
        }
        TileWRAMLo[x] = 0x8C;
        TileWRAMHi[x] = Mem(0x05);
        TileAnimFrame[x] = 0x01;
        uint8_t saved = PageIndex;
        PageIndex = x;
        DrawTileBlast();
        PageIndex = saved;
        if (saved != 0) {
            return out;
        }
    }
    // $A103: a Zebetite?
    if (Mem(0x04) & 0x01) {
        Mem(0x04)--;
    }
    uint8_t a = Read((uint16_t)(Mem(0x04) | (Mem(0x05) << 8)));
    if (a & 0x01) {
        return out;
    }
    a >>= 1;
    if (a < 0x48 || a >= 0x4C) {
        return out;
    }
    for (uint8_t y = 0; y != 0x28; y = (uint8_t)(y + 0x08)) {
        if (ZebStatus[y] != 0 && (Mem(0x04) & 0x9E) == ZebWRAMLo[y] && Mem(0x05) == ZebWRAMHi[y]) {
            ZebHit[y] = 0x01;
            break;
        }
    }
    return out;
}

// $95C0 -> $A142: a missile hitting Mother Brain (tiles $5E-$71). A = tile.
Regs MBrainTileHook(Regs in) {
    Regs out = in;
    uint8_t y = in.a;
    out.y = y;
    if (UpdtngPrjctl != 0) {
        uint8_t x = PageIndex;
        out.x = x;
        if (ObjAction[x] == 0x0B && y >= 0x5E && y < 0x72) {
            MBrainHitFlag = 0x01;
        }
    }
    out.a = y;
    return out;
}

} // namespace

bool TourianHook(uint16_t addr, const Regs& in, Regs& out) {
    out = in;
    switch (addr) {
        case 0x95A8: { // $A320: a Metroid is gone
            uint8_t y = Lsr4(in.x);
            MetroidState[y] = 0;
            MetroidOnSamus = 0;
            out.a = 0;
            out.y = y;
            return true;
        }
        case 0x95AB: // $A315: all Metroids let go
            for (int y = 5; y >= 0; y--) {
                MetroidState[y] = 0;
            }
            MetroidOnSamus = 0;
            out.a = 0;
            out.y = 0xFF;
            return true;
        case 0x95AE:
            ClearNameTableObjects(in.y);
            out.a = 0;
            out.x = 0x03;
            out.y = 0x80;
            return true;
        case 0x95B1:
            out = AddCannon(in);
            return true;
        case 0x95B4:
            out = AddMotherBrain(in);
            return true;
        case 0x95B7:
            out = AddZebetite(in);
            return true;
        case 0x95BA:
            out = AddRinkaSpawner(in);
            return true;
        case 0x95BD:
            out = BlastTileHook(in);
            return true;
        case 0x95C0:
            out = MBrainTileHook(in);
            return true;
        case 0x95C3:
            TourianAreaRoutine();
            return true;
        case 0x95C6:
            out.a = TwosCompliment(in.a);
            return true;
        case 0x95E5:
            EnemyAI(in.x);
            return true;
        default:
            return false;
    }
}

} // namespace Game
