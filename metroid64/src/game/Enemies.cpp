// Bank 7 from $F1A7: enemies, enemy/object collisions, enemy-spawned objects and tile regrowth.

#include "game/Enemies.h"

#include "game/AllActive.h"
#include "game/Area.h"
#include "game/Collision.h"
#include "game/Constants.h"
#include "game/EnemyRam.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Original.h"
#include "game/Ram.h"
#include "game/Room.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;
using namespace Const;

namespace {

// Per-area data the enemy code reads from the bank mapped at $8000.
constexpr uint16_t kAreaDamageLo = 0x95CE;
constexpr uint16_t kAreaDamageHi = 0x95CF;
constexpr uint16_t kExplodeAnim = 0x95DC;
constexpr uint16_t kPieceAnim = 0x95DD;
constexpr uint16_t kFireballAnims = 0x95E0;
constexpr uint16_t kFireballEndAnims = 0x95E2;
constexpr uint16_t kMellowEnemy = 0x95E4;
constexpr uint16_t kEnemyExplodeAnimTbl = 0x960B;
constexpr uint16_t kEnemyHitPointTbl = 0x962B;
constexpr uint16_t kEnemyRunAnimTbl = 0x963B;
constexpr uint16_t kEnemyTurnAnimTbl = 0x965B;
constexpr uint16_t kEnemyTurnDelayTbl = 0x967B;
constexpr uint16_t kEnemyDataTbl = 0x968B;
constexpr uint16_t kEnemyTimerTbl = 0x969B;
constexpr uint16_t kEnemyDistTbl = 0x96AB;
constexpr uint16_t kEnemyInitDelayTbl = 0x96BB;
constexpr uint16_t kEnemyMoveSetTbl = 0x96CB;
constexpr uint16_t kEnemyAccelYTbl = 0x972B;
constexpr uint16_t kEnemyAccelXTbl = 0x973F;
constexpr uint16_t kEnemySpeedYTbl = 0x9753;
constexpr uint16_t kEnemySpeedXTbl = 0x9767;
constexpr uint16_t kProjAnimTbl = 0x978B;
constexpr uint16_t kProjXTbl = 0x979B;
constexpr uint16_t kProjYTbl = 0x97A3;
constexpr uint16_t kFireballPathTbl = 0x97A7;
constexpr uint16_t kTileBlastFrameTbl = 0x97AF;

// Bank 7 tables.
constexpr uint16_t kTable15 = 0xF92A;
constexpr uint16_t kTable16 = 0xFAEA;
constexpr uint16_t kTable17 = 0xFC5D;
constexpr uint16_t kTable18 = 0xFD5A;
constexpr uint16_t kTable19 = 0xFE79;
constexpr uint16_t kTileBlastAnim = 0xFF86;

// $0728-$072F: room enemies waiting to appear (8 bytes each).
inline constexpr Var SpawnType{ 0x0728 };
inline constexpr Var SpawnSlot{ 0x0729 };
inline constexpr Var SpawnY{ 0x072A };
inline constexpr Var SpawnX{ 0x072B };
inline constexpr Var SpawnHi{ 0x072C };

uint16_t Ptr(uint8_t zp) {
    return (uint16_t)(Mem(zp) | (Mem((uint8_t)(zp + 1)) << 8));
}

// $E449 (bank 7): $00/$01 = -$00/$01.
void Negate00() {
    bool c = true;
    Mem(0x00) = Sbc(0, Mem(0x00), c);
    Mem(0x01) = Sbc(0, Mem(0x01), c);
}

// $E792 (bank 7, Collision.cpp): enemy x's position into $08 (y), $09 (x), $0B (name table).
void GetEnemyPosition(uint8_t x) {
    EnemyCoordsToTemp(x);
}

// $80B0 (area banks): the enemy's entry in the table at $977B, times 2. Returns A; Y = EnDataIndex.
uint8_t EnemyType80B0(uint8_t x) {
    return AreaHook(0x80B0, { 0, x, 0 }).a;
}

} // namespace

// --- $F270-$F344 Crash detection pieces (SamusEnemyCollisions $F034 is in Room.cpp)
// ---------------------------------------------------------------------------

namespace {

uint8_t F340() {
    return Mem(0x10) ^ 0x03;
}

// $F332
uint8_t F332() {
    return (uint8_t)(F340() << 3);
}

// $F270
void F270(uint8_t x, uint8_t a) {
    Mem((uint16_t)(0x030A + x)) |= a;
}

// $F27B
void F27B(uint8_t y, uint8_t a) {
    Mem((uint16_t)(0x030A + y)) |= a;
}

// $F279
void F279(uint8_t y) {
    F27B(y, Mem(0x10));
}

// $F338
void F338(uint8_t y) {
    F27B(y, (uint8_t)(Mem(0x10) << 3));
}

// $F2D8
void F2D8(uint8_t x, uint8_t a) {
    En0404[x] |= a;
}

// $F2DF
void F2DF(uint8_t y) {
    En0404[y] |= Mem(0x10);
}

// $F306: Samus takes the area's base damage.
void TakeAreaDamage() {
    HealthLoChange = Read(kAreaDamageLo);
    HealthHiChange = Read(kAreaDamageHi);
}

// $F2BF: a Memu was hit.
void F2BF(uint8_t x) {
    MemuHit[x] = (uint8_t)(((MemuHit[x] & 0xF8) | Mem(0x10)) ^ 0x03);
}

// $F2A5 / $F298 tail: Samus is hit.
void SamusHitF2A5() {
    F338(0);
    TakeAreaDamage();
}

// $F298: A = what hit Samus.
void SamusHitF298(uint8_t a) {
    ObjHitBy = a;
    uint8_t y = a;
    if (!(a & 0x80)) {
        if (Read((uint16_t)(kEnemyDataTbl + y)) & 0x10) {
            return;
        }
    }
    SamusHitF2A5();
}

// $F2B4: Memu x against Samus.
void CrashMemuSamus(uint8_t x, bool carry) {
    if (carry) {
        return;
    }
    if (IsScrewAttackActive()) {
        SamusHitF298(0xC0);
        return;
    }
    F2BF(x);
}

// $F32A: Memu x against projectile y.
void CrashMemuBullet(uint8_t x, uint8_t y, bool carry) {
    if (carry) {
        return;
    }
    F279(y);
    F2BF(x);
}

// $F2CA: enemy x against projectile y.
void CrashEnemyBullet(uint8_t x, uint8_t y, bool carry) {
    if (carry) {
        return;
    }
    En040E[x] = ObjAction[y];
    F279(y);
    F2D8(x, F332());
}

// $F2ED: Samus (x = 0) against enemy projectile y.
void CrashSamusEnemyObject(uint8_t x, uint8_t y, bool carry) {
    if (carry) {
        return;
    }
    F2DF(y);
    if (!IsScrewAttackActive()) {
        return;
    }
    ObjHitBy = 0x80;
    F270(x, F332());
    TakeAreaDamage();
}

// $F282: enemy x against Samus.
void CrashEnemySamus(uint8_t x, uint8_t y, bool carry) {
    if (carry) {
        return;
    }
    // $F2E8
    uint8_t a = F340();
    if (a != 0) {
        F2D8(x, a);
    } else {
        CrashSamusEnemyObject(x, y, false);
    }
    if (!IsScrewAttackActive()) {
        En040E[x] = 0x81; // screw attack
        F2D8(x, F332());
        return;
    }
    if (Enstatus[x] >= 0x04) {
        return;
    }
    SamusHitF298(EnDataIndex[x]);
}

} // namespace

// $F311: Samus touched something that hurts (blast, fire). Clears the pending health change.
static void CrashF311(uint8_t y, bool carry) {
    if (carry) {
        return;
    }
    ObjHitBy = 0xE0;
    F338(y);
    Mem(0x73) = Mem(0x0F) != 0 ? 0x01 : 0x00;
    ClearHealthChangeF323();
}

void CrashDoorF277(uint8_t y, bool carry) {
    if (!carry) {
        F279(y);
    }
}
void CrashEnemySamusF282(uint8_t x, uint8_t y, bool carry) {
    CrashEnemySamus(x, y, carry);
}
void CrashMemuSamusF2B4(uint8_t x, bool carry) {
    CrashMemuSamus(x, carry);
}
void CrashEnemyBulletF2CA(uint8_t x, uint8_t y, bool carry) {
    CrashEnemyBullet(x, y, carry);
}
void CrashSamusObjectF2ED(uint8_t x, uint8_t y, bool carry) {
    CrashSamusEnemyObject(x, y, carry);
}
void CrashBlastF311(uint8_t y, bool carry) {
    CrashF311(y, carry);
}
void CrashMemuBulletF32A(uint8_t x, uint8_t y, bool carry) {
    CrashMemuBullet(x, y, carry);
}

void ClearHealthChangeF323() {
    HealthLoChange = 0;
    HealthHiChange = 0;
}

// --- $F1A7-$F1FA Distances -------------------------------------------------------------

void DistFromObj0ToObj1(uint8_t x, uint8_t y) {
    AddObject1YRadius(ObjRadY[x], y);
    AddObject1XRadius(ObjRadX[x], y);
}

void DistFromObj0ToEn1(uint8_t x, uint8_t y) {
    AddEnemy1YRadius(ObjRadY[x], y);
    AddEnemy1XRadius(ObjRadX[x], y);
}

void DistFromEn0ToObj1(uint8_t x, uint8_t y) {
    AddObject1YRadius(EnRadY[x], y);
    AddObject1XRadius(EnRadX[x], y);
}

void AddEnemy1XRadius(uint8_t a, uint8_t y) {
    Mem(0x05) = (uint8_t)(a + EnRadX[y]);
}

void AddObject1XRadius(uint8_t a, uint8_t y) {
    Mem(0x05) = (uint8_t)(a + ObjRadX[y]);
}

void AddObject1YRadius(uint8_t a, uint8_t y) {
    Mem(0x04) = (uint8_t)(a + ObjRadY[y]);
}

void AddEnemy1YRadius(uint8_t a, uint8_t y) {
    Mem(0x04) = (uint8_t)(a + EnRadY[y]);
}

namespace {
// $F266: $01 = the high byte of the distance; a negative distance is made positive and noted in $10.
void F266(uint8_t a) {
    Mem(0x01) = a;
    if (a & 0x80) {
        Negate00();
        Mem(0x10)++;
    }
}
} // namespace

bool CheckObjectDistance() {
    Mem(0x10) = 0x02;
    Mem(0x03) = 0x02 & ScrollDir;
    bool c = true;
    Mem(0x00) = Sbc(Mem(0x07), Mem(0x06), c);
    if (Mem(0x03) == 0 && (Mem(0x0B) ^ Mem(0x0A)) != 0) {
        // Vertical room, the objects on different name tables.
        F266(Sbc(Mem(0x0B), Mem(0x0A), c)); // $F262
        bool c2 = true;
        Mem(0x00) = Sbc(Mem(0x00), 0x10, c2);
        if (!c2) {
            Mem(0x01)--;
        }
    } else {
        F266(Sbc(0x00, 0x00, c)); // $F224
    }
    // $F22B
    if (Mem(0x01) != 0) {
        return true;
    }
    Mem(0x11) = Mem(0x00);
    if (Mem(0x00) >= Mem(0x04)) {
        return true;
    }
    Mem(0x10) = (uint8_t)(Mem(0x10) << 1);
    c = true;
    Mem(0x00) = Sbc(Mem(0x09), Mem(0x08), c);
    if (Mem(0x03) != 0 && (Mem(0x0B) ^ Mem(0x0A)) != 0) {
        F266(Sbc(Mem(0x0B), Mem(0x0A), c)); // $F262
    } else {
        F266(Sbc(0x00, 0x00, c)); // $F251
    }
    // $F256
    if (Mem(0x01) != 0) {
        return true;
    }
    Mem(0x0F) = Mem(0x00);
    return Mem(0x00) >= Mem(0x05);
}

bool TouchCheckDC7F(uint8_t x, uint8_t y) {
    GetObject1CoordData(y);
    return TouchCheckDC82(x, y);
}

bool TouchCheckDC82(uint8_t x, uint8_t y) {
    GetObject0CoordData(x);
    DistFromObj0ToObj1(x, y);
    return CheckObjectDistance();
}

// --- $F345 UpdateEnemies -----------------------------------------------------------------------------

void UpdateEnemies() {
    uint8_t x = 0x50;
    for (;;) {
        DoOneEnemy(x);
        x = (uint8_t)(PageIndex - 0x10);
        if (x == 0) {
            break;
        }
    }
    DoOneEnemy(0x00); // reached by falling through
}

namespace {

// $F37F: an enemy not marked on screen (bit 1 of $0405) is only updated once it is fully visible. Returns
// false when the original pops DoOneEnemy's return address (the enemy is skipped).
bool EnemyStillVisible(uint8_t x) {
    if (En0405[x] & 0x02) {
        return true;
    }
    Mem(0x0A) = EnYRoomPos[x];
    Mem(0x0B) = EnXRoomPos[x];
    Mem(0x06) = EnNameTable[x];
    Mem(0x08) = EnRadY[x];
    Mem(0x09) = EnRadX[x];
    return IsObjectVisible() != 0;
}

// $F3AA: bit 6 of $0405 = whether the enemy acts this frame (alternating, staggered by slot).
void EnemyActBitF3AA(uint8_t x) {
    bool c = false;
    uint8_t a = En0405[x];
    a = Asl(a, c);
    a = Rol(a, c);
    uint8_t y = a;
    uint8_t t = (uint8_t)(x >> 4);
    c = (x >> 3) & 1;
    t ^= FrameCount;
    t = Lsr(t, c);
    a = Ror(y, c);
    a = Ror(a, c);
    En0405[x] = a;
}

void EnemyHitCheckF536(uint8_t x);
void F6B9(uint8_t x);
void F75B(uint8_t x);
void F682(uint8_t x);
void F676(uint8_t x);
void F7BA(uint8_t x);
void F51E();
void F699(uint8_t x);
void EnemyFrozen(uint8_t x);
void EnemyDropped(uint8_t x);
void EnemyHitStun(uint8_t x);

// $F40A: hit check, then the area's enemy routine ($95E5).
void EnemyAI(uint8_t x) {
    EnemyHitCheckF536(x);
    AreaHook(0x95E5, { 0, PageIndex, 0 });
}

} // namespace

void DoOneEnemy(uint8_t x) {
    PageIndex = x;
    uint8_t y = Enstatus[x];
    if (y != 0 && y < 0x03) {
        if (!EnemyStillVisible(x)) {
            return;
        }
        x = PageIndex;
    }
    EnemyActBitF3AA(x);
    uint8_t a = Enstatus[x];
    Mem(0x81) = a;
    if (a >= 0x07) {
        KillObject(x);
        return;
    }
    ChooseRoutineEffects(0xF36E, a);
    switch (a) {
        case 0x01: // $F3BE: resting
            if (!((En0405[x] << 1) & 0x80)) {
                En6B01[x] = 0;
                EnCounter[x] = 0;
                En040A[x] = 0;
                F6B9(x);
                F75B(x);
                F682(x);
                F676(x);
                if (EnDelay[x] != 0) {
                    F7BA(x);
                }
            }
            EnemyAI(x);
            break;
        case 0x02: // $F3E6: active
            if (!((En0405[x] << 1) & 0x80)) {
                bool skip = false;
                if (En0405[x] & 0x20) {
                    EnDelay[x] = Read((uint16_t)(kEnemyInitDelayTbl + EnDataIndex[x]));
                    if (--Enstatus[x] != 0) {
                        skip = true;
                    }
                }
                if (!skip) {
                    F6B9(x);
                    F75B(x);
                    F51E();
                    x = PageIndex;
                }
            }
            EnemyAI(x);
            break;
        case 0x03: // $F40D: exploding
            AreaHook(0x95E5, { 0, x, 0 });
            break;
        case 0x04:
            EnemyFrozen(x);
            break;
        case 0x05:
            EnemyDropped(x);
            break;
        case 0x06:
            EnemyHitStun(x);
            break;
        default:
            break;
    }
}

// $F410. a: the animation delay UpdateEnemyAnim stores (whatever A the caller had).
void EnemyAnimDraw(uint8_t a) {
    UpdateEnemyAnim(a);
    AreaHook(0x8058, { 0, PageIndex, 0 });
    EnemyDraw();
}

namespace {
// $F423 / $F425
void EnemyDrawTail() {
    uint8_t x = PageIndex;
    if (Enstatus[x] != 0) {
        if (DrawEnemy()) {
            return; // its explosion ended: the original pops this routine's return address
        }
    }
    x = PageIndex;
    En0404[x] = 0;
    En040E[x] = 0;
}
void EnemyDrawCntrl(uint8_t cntrl) {
    ObjectCntrl = cntrl;
    EnemyDrawTail();
}
} // namespace

// $F416
void EnemyDraw() {
    uint8_t x = PageIndex;
    if ((EnSpecialAttribs[x] & 0x80) && !(ObjectCntrl & 0x80)) {
        ObjectCntrl = 0xA3;
    }
    EnemyDrawTail();
}

// $F438
void EnemyAnimDrawOnly(uint8_t a) {
    UpdateEnemyAnim(a);
    EnemyDraw();
}

namespace {

// $F43E: frozen.
void EnemyFrozen(uint8_t x) {
    EnemyHitCheckF536(x);
    if (Enstatus[x] == 0x03) {
        EnemyAnimDraw(0x03);
        return;
    }
    if (!(ObjectCntrl & 0x80)) {
        ObjectCntrl = 0xA1;
    }
    if ((FrameCount & 0x07) == 0) {
        if (--En040D[x] == 0 && Enstatus[x] != 0x03) {
            Enstatus[x] = En040C[x];
            En040D[x] = Read((uint16_t)(kEnemyTimerTbl + EnDataIndex[x]));
        }
    }
    if (En040D[x] < 0x0B && (FrameCount & 0x02)) {
        ObjectCntrl = (uint8_t)(ObjectCntrl << 1);
    }
    EnemyDraw();
}

// $F4B7 PickupMissile
void PickupMissile(uint8_t x) {
    uint8_t a = EnDataIndex[x] != 0 ? 0x1E : 0x02;
    bool c = false;
    a = Adc(a, MissileCount, c);
    if (c || a >= MaxMissiles) {
        a = MaxMissiles;
    }
    MissileCount = a;
    SFX_MissilePickup();
}

// $F483: a dropped item (energy or missiles).
void EnemyDropped(uint8_t x) {
    if (En0404[x] & 0x24) {
        // Samus touched it.
        KillObject(x);
        uint8_t frame = EnAnimFrame[x];
        if (frame == 0x80) {
            PickupMissile(x);
            return;
        }
        uint8_t lo = 0x00, hi = 0x03;
        if (EnDataIndex[x] == 0) {
            hi = 0x02;
            if (frame == 0x81) {
                hi = 0x00;
                lo = 0x50;
            }
        }
        HealthLoChange = lo;
        HealthHiChange = hi;
        AddHealth();
        SFX_EnergyPickup();
        return;
    }
    if ((FrameCount & 0x03) == 0) {
        if (--En040D[x] == 0) {
            KillObject(x);
        }
    }
    ObjectCntrl = (uint8_t)(((FrameCount & 0x02) >> 1) | 0xA0);
    EnemyDraw();
}

// $F515
void F515(uint8_t x, uint8_t a) {
    En040C[x] = a;
    Enstatus[x] = 0x04;
}

// $F518
void F518(uint8_t x) {
    Enstatus[x] = 0x04;
}

// $F4EE: briefly stunned after a hit.
void EnemyHitStun(uint8_t x) {
    if (--EnSpecialAttribs[x] == 0) {
        uint8_t a = En040C[x];
        EnSpecialAttribs[x] = a & 0xC0;
        Enstatus[x] = a & 0x3F;
        uint8_t s = a & 0x3F;
        if (EnemyType80B0(x) & 0x20) {
            F515(x, s);
        }
    }
    EnemyDrawCntrl(0xA0);
}

// $F51E: in a horizontal room, an enemy that falls to the bottom is gone.
void F51E() {
    uint8_t x = PageIndex;
    if (ScrollDir < 0x02 || EnYRoomPos[x] < 0xEC) {
        return;
    }
    KillObject(x);
}

// $F56C
void F56C(uint8_t x) {
    if (EnemyType80B0(x) & 0x20) {
        SFX_MetroidHit(); // $F530
        return;
    }
    SFXMetal();
    x = PageIndex; // $F42D
    En0404[x] = 0;
    En040E[x] = 0;
}

// $F536: was the enemy hit by a weapon?
void EnemyHitCheckF536(uint8_t x) {
    Mem(0x0A) = EnSpecialAttribs[x];
    if (!(En0404[x] & 0x20)) {
        return;
    }
    bool boss = Mem(0x0A) & 0x40;
    if (En040E[x] == wa_IceBeam && !boss && Enstatus[x] != 0x04) {
        // Frozen.
        F515(x, Enstatus[x]);
        En040D[x] = 0x40;
        uint8_t a = EnemyType80B0(x);
        if (!(a & 0x20)) {
            return;
        }
        EnHitPoints[x] = 0x05;
        AreaHook(0x95A8, { 0x05, x, EnDataIndex[x] });
        return;
    }
    if (EnHitPoints[x] == 0xFF) {
        F56C(x);
        return;
    }
    if (boss) {
        SFX_BossHit();
    } else {
        uint8_t a = EnemyDataF74B(x) & 0x0C;
        if (a == 0x00 || a == 0x04) {
            SFX_EnemyHit();
        } else if (a == 0x08) {
            SFX_BigEnemyHit();
        } else {
            SFX_MetroidHit();
        }
    }
    x = PageIndex;
    if (EnemyType80B0(x) & 0x20) {
        if (En040E[x] != wa_Missile) {
            F56C(x);
            return;
        }
    }
    uint8_t y = EnDataIndex[x];
    uint8_t a = Enstatus[x];
    if (a == 0x04) {
        a = En040C[x];
    }
    a |= Mem(0x0A);
    En040C[x] = a;
    bool kill = false;
    if (!((a << 1) & 0x80)) {
        a = EnemyType80B0(x);
        y = EnDataIndex[x];
        if (!(a & 0x20)) {
            y = En040E[x];
            if (y == wa_Missile || y == 0x81) {
                kill = true; // missiles and the screw attack kill at once
            }
        }
    }
    if (!kill) {
        Enstatus[x] = 0x06;
        EnSpecialAttribs[x] = boss ? 0x03 : 0x0A;
        int hits = 1;
        if (y == 0x02) {
            hits = 2; // ice
        } else if (boss && En040E[x] == wa_Missile) {
            hits = 4;
        }
        for (int i = 0; i < hits; i++) {
            if (--EnHitPoints[x] == 0) {
                kill = true;
                break;
            }
        }
        if (!kill) {
            return;
        }
    }
    // $F611: killed.
    Enstatus[x] = 0x03;
    if (!boss && En040E[x] < 0x02) {
        EnemyItemDrop(x);
        x = PageIndex;
    }
    y = EnemyDirIndex(x);
    SetEnemyAnim(x, Read((uint16_t)(kEnemyExplodeAnimTbl + y)));
    EnCounter[x] = 0x00;
    // An explosion piece slot.
    for (x = 0xC0; x != 0xE0; x += 0x08) {
        if (Enstatus[x] == 0) {
            SetEnemyAnim(x, Read(kPieceAnim));
            EnCounter[x] = 0x0A;
            Enstatus[x]++;
            En0407[x] = boss ? 0x03 : 0x00;
            uint8_t p = PageIndex;
            EnYRoomPos[x] = EnYRoomPos[p];
            EnXRoomPos[x] = EnXRoomPos[p];
            EnNameTable[x] = EnNameTable[p];
            break;
        }
    }
}

// $F676
void F676(uint8_t x) {
    uint8_t a = (uint8_t)(EnemyType80B0(x) << 3);
    En6B03[x] = a & 0xC0;
}

// $F682
void F682(uint8_t x) {
    uint8_t y = EnemyDirIndex(x);
    uint8_t a = Read((uint16_t)(kEnemyRunAnimTbl + y));
    if (a == EnResetAnimIndex[x]) {
        return;
    }
    SetEnemyAnim(x, a);
}

// $F699
void F699(uint8_t x) {
    uint8_t y = EnemyDirIndex(x);
    uint8_t a = Read((uint16_t)(kEnemyTurnAnimTbl + y));
    if (a == EnResetAnimIndex[x]) {
        return;
    }
    SetEnemyAnim(x, a);
    a = Read((uint16_t)(kEnemyTurnDelayTbl + EnDataIndex[x])) & 0x7F;
    for (y = a; y != 0; y--) {
        EnAnimIndex[x]--;
    }
}

// $F7B3
void F7B3(uint8_t x, uint8_t a) {
    En0405[x] &= a;
}

// $F752: carry = whether the enemy and Samus are on different name tables. Returns Y (EnNameTable).
uint8_t F752(uint8_t x, bool& c) {
    uint8_t y = EnNameTable[x];
    c = (y ^ ObjectHi) & 0x01;
    return y;
}

// $F6B9: turn the enemy toward Samus when its timer runs out.
void F6B9(uint8_t x) {
    Mem(0x82) = 0;
    uint8_t attr = EnemyDataF74B(x);
    if (Enstatus[x] == 0x02 && !(attr & 0x02)) {
        return;
    }
    if (--En040D[x] != 0) {
        return;
    }
    En040D[x] = Read((uint16_t)(kEnemyTimerTbl + EnDataIndex[x]));
    if (attr & 0x80) {
        // Horizontal: face Samus.
        F7B3(x, 0xFE);
        uint8_t a;
        bool c;
        bool done = false;
        if (ScrollDir >= 0x02) {
            uint8_t y = F752(x, c);
            if (c) {
                a = y ^ PPUCNT0ZP;
                done = true;
            }
        }
        if (!done) {
            a = EnXRoomPos[x];
            c = a >= ObjectX;
            if (a == ObjectX) {
                Mem(0x82)++;
            }
            a = (uint8_t)((a << 1) | (c ? 1 : 0));
        }
        a &= 0x01;
        SetEnemyBits(x, a);
        a = En0405[x];
        a = (uint8_t)((a >> 1) | (a << 7));
        if ((a ^ En0403[x]) & 0x80) {
            AreaHook(0x81DA, { 0, x, 0 });
        }
    }
    // $F70F: vertical.
    F7B3(x, 0xFB);
    uint8_t a;
    bool c;
    bool done = false;
    if (ScrollDir < 0x02) {
        uint8_t y = F752(x, c);
        if (c) {
            a = y ^ PPUCNT0ZP;
            done = true;
        }
    }
    if (!done) {
        a = EnYRoomPos[x];
        c = a >= ObjectY;
        if (a == ObjectY) {
            Mem(0x82) += 2;
        }
        a = (uint8_t)((a << 1) | (c ? 1 : 0));
    }
    a = (uint8_t)((a & 0x01) << 2);
    SetEnemyBits(x, a);
    a = En0405[x];
    a = (uint8_t)(((a >> 3) & 0x7F) | ((a & 0x04) << 5));
    if ((a ^ En0402[x]) & 0x80) {
        AreaHook(0x820F, { 0, x, 0 });
    }
}

// $F75B: is Samus close enough for the enemy to notice?
void F75B(uint8_t x) {
    Mem(0x06) = 0xE7;
    SetEnemyBits(x, 0x18);
    uint8_t dist = Read((uint16_t)(kEnemyDistTbl + EnDataIndex[x]));
    if (dist == 0) {
        return;
    }
    if (!(En0405[x] & 0x02)) {
        F7B3(x, Mem(0x06));
        return;
    }
    Mem(0x06) = (dist & 0x80) ? 0xF7 : 0xEF;
    Mem(0x02) = dist & 0x7F;
    Mem(0x00) = ObjectY;
    uint8_t y = EnYRoomPos[x];
    if (!(En0405[x] & 0x80)) {
        Mem(0x00) = ObjectX;
        y = EnXRoomPos[x];
    }
    Mem(0x00) = (uint8_t)((Mem(0x00) >> 1) | ((ObjectHi & 0x01) << 7));
    uint8_t a = (uint8_t)((y >> 1) | ((EnNameTable[x] & 0x01) << 7));
    a = (uint8_t)(a - Mem(0x00));
    if (a & 0x80) {
        a = TwosCompliment(a);
    }
    a >>= 3;
    if (a < Mem(0x02)) {
        return;
    }
    F7B3(x, Mem(0x06));
}

// $F7BA: the resting enemy's delay; when it ends, the enemy picks a movement and becomes active.
void F7BA(uint8_t x) {
    if (--EnDelay[x] != 0) {
        return;
    }
    if (!(En0405[x] & 0x08)) {
        EnDelay[x]++;
        return;
    }
    if (EnDataIndex[x] == 0x07) {
        SFXEnemyRegen();
        x = PageIndex;
    }
    Enstatus[x]++;
    F699(x);
    bool c = false;
    Mem(0x00) = Adc(Read((uint16_t)(kEnemyMoveSetTbl + EnDataIndex[x])), 0xD1, c);
    Mem(0x01) = Adc(0x00, 0x97, c);
    uint16_t set = Ptr(0x00);
    uint8_t y = (uint8_t)(((FrameCount ^ RandomNumber1) & Read(set)) + 1);
    En0408[x] = Read((uint16_t)(set + y));
    if (EnemyType80B0(x) & 0x80) {
        EnCounter[x] = 0;
        En0407[x] = 0;
        y = En0408[x];
        En6AFE[x] = Read((uint16_t)(kEnemyAccelYTbl + y));
        En6AFF[x] = Read((uint16_t)(kEnemyAccelXTbl + y));
        En0402[x] = Read((uint16_t)(kEnemySpeedYTbl + y));
        En0403[x] = Read((uint16_t)(kEnemySpeedXTbl + y));
        uint8_t a = En0405[x];
        if (a & 0x80) {
            if (a & 0x04) {
                AreaHook(0x8206, { 0, x, 0 });
            }
        } else if (a & 0x01) {
            AreaHook(0x81D1, { 0, x, 0 });
        }
    }
    F7B3(x, 0xDF);
}

} // namespace

void SetEnemyBits(uint8_t x, uint8_t a) {
    En0405[x] |= a;
}

uint8_t EnemyDataF74B(uint8_t x) {
    return Read((uint16_t)(kEnemyDataTbl + EnDataIndex[x]));
}

uint8_t SetEnemyAnim(uint8_t x, uint8_t a) {
    EnResetAnimIndex[x] = a;
    SetEnemyAnimIndex(x, a);
    return 0;
}

void SetEnemyAnimIndex(uint8_t x, uint8_t a) {
    EnAnimIndex[x] = a;
    ClearEnemyAnimDelay(x);
}

void ClearEnemyAnimDelay(uint8_t x) {
    EnAnimDelay[x] = 0;
}

// $F83E: Y = EnDataIndex * 2 + bit 0 of $0405.
uint8_t EnemyDirIndex2(uint8_t x) {
    return (uint8_t)((EnDataIndex[x] << 1) | (En0405[x] & 0x01));
}

// $F844: Y = EnDataIndex * 2 + the direction bit (bit 0, or bit 2 for vertical movers).
uint8_t EnemyDirIndex(uint8_t x) {
    uint8_t a = En0405[x];
    bool c = (a & 0x80) ? (a & 0x04) : (a & 0x01);
    return (uint8_t)((EnDataIndex[x] << 1) | (c ? 1 : 0));
}

uint8_t EnemyFrameCarry(uint8_t x, bool& carry) {
    uint8_t a = (uint8_t)(x >> 3);
    carry = (x >> 2) & 1;
    a = Adc(a, FrameCount, carry);
    return Lsr(a, carry);
}

void ResetEnemyHitPoints(uint8_t x) {
    uint8_t y = EnDataIndex[x];
    En040D[x] = Read((uint16_t)(kEnemyTimerTbl + y));
    uint8_t a = Read((uint16_t)(kEnemyHitPointTbl + y));
    if (EnSpecialAttribs[x] & 0x80) {
        a = (uint8_t)(a << 1);
    }
    EnHitPoints[x] = a;
}

// --- $F870 An enemy fires a projectile ---------------------------------------------------------------
// $85: projectile type, $87: flags, $88: pattern.

namespace {
// $F8E8: a free enemy projectile slot ($60-$B0). Returns false (carry clear) and the slot in y.
bool FindEnemyObjectSlot(uint8_t& y) {
    for (y = 0x60; y != 0xC0; y += 0x10) {
        if (Enstatus[y] == 0) {
            return true;
        }
    }
    return false;
}

// $F8F8
void F8F8(uint8_t y) {
    if (Mem(0x85) < 0x02) {
        return;
    }
    uint8_t x = PageIndex;
    En040A[y] = (uint8_t)(((Mem(0x88) << 1) | (En0405[x] & 0x01)) & 0x07);
    Enstatus[y] = 0x02;
    EnDelay[y] = 0;
    EnAnimDelay[y] = 0;
    En0408[y] = 0;
}

// $F92C
void F92C(uint8_t y) {
    EnRadY[y] = 0x02;
    EnRadX[y] = 0x02;
    En0405[y] |= 0x02;
}
} // namespace

void EnemyFireProjectile(uint8_t x) {
    if (!(En0405[x] & 0x10)) {
        return;
    }
    if ((Mem(0x87) & Enstatus[x]) == 0) {
        return;
    }
    if ((Mem(0x87) & 0x80) && En6B01[x] != 0) {
        return;
    }
    uint8_t y;
    if (!FindEnemyObjectSlot(y)) {
        return;
    }
    En0404[y] = 0;
    F92C(y);
    bool c = En0405[x] & 0x01;
    uint8_t type = Mem(0x85);
    uint8_t i = (uint8_t)((type << 1) | (c ? 1 : 0));
    SetEnemyAnim(y, Read((uint16_t)(kProjAnimTbl + i)));
    x = PageIndex;
    Enstatus[y] = 0x01;
    En0403[y] = Read((uint16_t)(kTable15 + (En0405[x] & 0x01)));
    En0402[y] = 0;
    x = PageIndex;
    F8F8(y);
    c = En0405[x] & 0x01;
    Mem(0x04) = Read((uint16_t)(kProjYTbl + type));
    i = (uint8_t)((type << 1) | (c ? 1 : 0));
    Mem(0x05) = Read((uint16_t)(kProjXTbl + i));
    // $F91D: place it next to the enemy.
    GetEnemyPosition(PageIndex);
    MoveObjectByDeltas();
    EnemyStorePosition(y);
    x = PageIndex;
    if (!(Mem(0x87) & 0x40)) {
        return;
    }
    SetEnemyAnimIndex(x, Mem((uint16_t)(0x0083 + (En0405[x] & 0x01))));
}

// --- $F93B Enemy projectiles -----------------------------------------------------------------------

namespace {

// $F97E: animate and draw. Returns true when DrawEnemy pops the return address.
bool EnemyObjectDraw(uint8_t delay) {
    UpdateEnemyAnim(delay);
    return DrawEnemy();
}

// $FA5B / $FA60
void FA60(uint8_t x) {
    En0404[x] = 0;
    Enstatus[x] = 0x05;
}
void FA5B(uint8_t x) {
    if (En0404[x] != 0) {
        FA60(x);
    }
}

// $F96A: a projectile flying straight.
bool EnemyObjectFly(uint8_t x) {
    FA5B(x);
    bool c = MoveEnemyObject(x);
    x = PageIndex;
    if (!c) {
        if (Enstatus[x] == 0) {
            return false;
        }
        FA60(x);
    }
    return EnemyObjectDraw(0x01);
}

// $F991: a projectile following a path (fireballs).
bool EnemyObjectPath(uint8_t x) {
    FA5B(x);
    uint8_t y = En040A[x] & 0xFE;
    Mem(0x0A) = Read((uint16_t)(kFireballPathTbl + y));
    Mem(0x0B) = Read((uint16_t)(kFireballPathTbl + y + 1));
    uint8_t a;
    for (;;) {
        y = En0408[x];
        a = Read((uint16_t)(Ptr(0x0A) + y));
        if (a == 0xFF) {
            En0408[x] = 0xFF;
            En0408[x]++; // $F987
            EnDelay[x] = 0;
            continue;
        }
        if (a == EnDelay[x]) {
            En0408[x] += 2; // $F984
            EnDelay[x] = 0;
            continue;
        }
        break;
    }
    EnDelay[x]++;
    y++;
    Regs r = AreaHook(0x8296, { Read((uint16_t)(Ptr(0x0A) + y)), x, y });
    x = PageIndex;
    En0402[x] = r.a;
    y = r.y;
    r = AreaHook(0x832F, { Read((uint16_t)(Ptr(0x0A) + y)), x, y });
    x = PageIndex;
    En0403[x] = r.a;
    y = r.a;
    a = En040A[x];
    bool odd = a & 0x01;
    a >>= 1;
    if (odd) {
        En0403[x] = TwosCompliment(y);
    }
    if (a == 0 && En0402[x] != 0 && !(En0402[x] & 0x80)) {
        EnResetAnimIndex[x] = Read((uint16_t)(kFireballAnims + En040A[x]));
    }
    bool c = MoveEnemyObject(x);
    x = PageIndex;
    if (!c) {
        if (Enstatus[x] == 0) {
            return false;
        }
        y = (En040A[x] >> 1) != 0 ? 1 : 0;
        SetEnemyAnim(x, Read((uint16_t)(kFireballEndAnims + y)));
        F518(x);
        EnDelay[x] = 0x0A;
    }
    return EnemyObjectDraw(0x01);
}

// $FA6B
bool EnemyObjectFade(uint8_t x) {
    if (EnAnimFrame[x] == 0xF7 || --EnDelay[x] == 0) {
        KillObject(x);
    }
    return EnemyObjectDraw(0x01);
}

// $FA91
bool EnemyObjectExplode(uint8_t x) {
    KillObject(x);
    SetEnemyAnim(x, Read(kExplodeAnim));
    return EnemyObjectDraw(0x01);
}


} // namespace

// $F949. Returns true when DrawEnemy pops this routine's return address.
bool DoOneEnemyObject(uint8_t x) {
    PageIndex = x;
    if (!(En0405[x] & 0x02)) {
        KillObject(x);
    }
    uint8_t a = Enstatus[x];
    if (a == 0) {
        return false;
    }
    ChooseRoutineEffects(0xF95D, a);
    switch (a) {
        case 0x01:
            return EnemyObjectFly(x);
        case 0x02:
            return EnemyObjectPath(x);
        case 0x04:
            return EnemyObjectFade(x);
        case 0x05:
            return EnemyObjectExplode(x);
        default:
            return false;
    }
}

bool UpdateObjectsF93B() {
    uint8_t x = 0xB0;
    for (;;) {
        if (DoOneEnemyObject(x)) {
            return false;
        }
        x = (uint8_t)(PageIndex - 0x10);
        if (x == 0x60) {
            break;
        }
    }
    return DoOneEnemyObject(0x60);
}

void KillObject(uint8_t x) {
    Enstatus[x] = 0;
}

// $FA1E: moves the enemy object by its speeds; checks the background first. Returns the carry: clear
// when it was blocked (killed).
bool MoveEnemyObject(uint8_t x) {
    bool check = true;
    if (InArea == 0x11 && !(Enstatus[x] & 0x01)) {
        check = false;
    }
    if (check) {
        // $FA7D
        x = PageIndex;
        Mem(0x02) = EnYRoomPos[x];
        Mem(0x03) = EnXRoomPos[x];
        Mem(0x0B) = EnNameTable[x];
        MakeCartRAMPtr();
        if (Read(Ptr(0x04)) < 0xA0) {
            return false;
        }
        x = PageIndex;
    }
    Mem(0x05) = En0403[x];
    Mem(0x04) = En0402[x];
    return MoveEnemyByDeltas(x);
}

// $FA41: moves enemy x by ($04, $05); kills it when it leaves the room. Returns the carry.
bool MoveEnemyByDeltas(uint8_t x) {
    GetEnemyPosition(x);
    if (!MoveObjectByDeltas()) {
        KillObject(x);
        return false;
    }
    EnemyStorePosition(x);
    return true;
}

// $FA49
void EnemyStorePosition(uint8_t x) {
    EnYRoomPos[x] = Mem(0x08);
    EnXRoomPos[x] = Mem(0x09);
    EnNameTable[x] = Mem(0x0B) & 0x01;
}

// --- $FA9D Explosion pieces -------------------------------------------------------------------------

void DestroyEnemyObjects() {
    uint8_t x = 0xC0;
    do {
        PageIndex = x;
        if (Enstatus[x] != 0) {
            // $FAB4
            if (--EnCounter[x] == 0) {
                EnCounter[x] = 0x0C;
                uint8_t n = --En0407[x];
                if ((n & 0x80) || n == 0) {
                    KillObject(x);
                }
            }
            if (EnCounter[x] == 0x09) {
                uint8_t y = (uint8_t)(En0407[x] << 1);
                Mem(0x04) = Read((uint16_t)(kTable16 + y));
                Mem(0x05) = Read((uint16_t)(kTable16 + y + 1));
                MoveEnemyByDeltas(x);
            }
            ObjectCntrl = 0x80;
            if (EnemyObjectDraw(0x03)) {
                return; // DrawEnemy popped the return address: DestroyEnemyObjects ends here
            }
        }
        x = (uint8_t)(PageIndex + 0x08);
    } while (x != 0xE0);
}

// --- $FAF2 Enemies waiting in the room ---------------------------------------------------------------

namespace {

// $FB7B. Y = EnDataIndex.
void FB7B(uint8_t x, uint8_t y) {
    bool c = AreaHook(0x80B0, { 0, x, 0 }).c; // the ASL's carry
    uint8_t v = En0405[x];
    En0405[x] = (uint8_t)((v >> 1) | (c ? 0x80 : 0));
    EnDelay[x] = Read((uint16_t)(kEnemyInitDelayTbl + y));
}

// $FAFF
void DoOneSpawn(uint8_t y) {
    PageIndex = y;
    if (SpawnType[y] == 0xFF) {
        return;
    }
    uint8_t x = SpawnSlot[y];
    uint8_t a = Enstatus[x];
    int rx = 0, ry = 0;
    if (a != 0 && ((En0405[x] & 0x02) || AllActive::KeepPastEdge(x, rx, ry))) {
        return; // (not part of the original: Everything active keeps it while the 3D view shows it)
    }
    En0404[x] = 0; // A is 0 on both paths here
    if (EnDataIndex[x] != 0xFF) {
        // $FB70
        EnDataIndex[x] = 0xFF;
        EnDelay[x] = 0x01;
        KillObject(x);
        return;
    }
    if (--EnDelay[x] != 0) {
        return;
    }
    if (!GetEnemyType(x, SpawnType[y])) {
        return; // Kraid/Ridley dead: the original pops DoOneSpawn's return address
    }
    y = PageIndex;
    EnYRoomPos[x] = SpawnY[y];
    EnXRoomPos[x] = SpawnX[y];
    EnNameTable[x] = SpawnHi[y];
    EnRadX[x] = 0x18;
    EnRadY[x] = 0x0C;
    GetObject1CoordData(0);
    GetEnemy0CoordData(x);
    DistFromEn0ToObj1(x, 0);
    if (!CheckObjectDistance()) {
        return; // Samus is in the way
    }
    EnDelay[x] = 0x01;
    Enstatus[x] = 0x01;
    En0405[x] = (uint8_t)((0x01 & ScrollDir) << 1);
    FB7B(x, EnDataIndex[x]);
    ResetEnemyHitPoints(x);
}

} // namespace

// $EB6E (bank 7, area $802A): the enemy starts over (direction, delay and hit points).
void EnemyRestartEB6E(uint8_t x) {
    uint8_t y = EnDataIndex[x];
    En0405[x] = (uint8_t)(En0405[x] << 1);
    FB7B(x, y);
    ResetEnemyHitPoints(x);
}

void UpdateObjectsFAF2() {
    uint8_t y = 0x18;
    for (;;) {
        DoOneSpawn(y);
        y = (uint8_t)(PageIndex - 0x08);
        if (y == 0) {
            break;
        }
    }
    DoOneSpawn(0x00);
}

// --- $FB88 Turning animations --------------------------------------------------------------------------

void EnemyTurnAnim() {
    uint8_t x = PageIndex;
    uint8_t y = EnemyDirIndex(x);
    uint8_t a = En6B01[x];
    // The sign tested is $6B03's when it isn't 0 (the flags of INC/DEC $6B03), else $6B01's (PHA/PLA).
    bool neg = En6B03[x] != 0 ? (En6B03[x] & 0x80) != 0 : (a & 0x80) != 0;
    if (neg) {
        a = TwosCompliment(a);
    }
    if (a < 0x08) {
        a = Read((uint16_t)(kEnemyRunAnimTbl + y)); // $FBBF
        if (a == EnResetAnimIndex[x]) {
            return;
        }
        SetEnemyAnim(x, a);
        return;
    }
    if (a >= 0x10) {
        return;
    }
    a = Mem((uint16_t)(0x0085 + (y & 0x01)));
    if (a == EnResetAnimIndex[x]) {
        return;
    }
    EnAnimIndex[x] = a;
    EnAnimIndex[x]--;
    EnemyDirAnim(x, a);
}

// $FBB9
void EnemyDirAnim(uint8_t x, uint8_t a) {
    EnResetAnimIndex[x] = a;
    ClearEnemyAnimDelay(x);
}

void EnemyTurnAnim2() {
    uint8_t x = PageIndex;
    uint8_t y = EnemyDirIndex(x);
    uint8_t a = Read((uint16_t)(kEnemyTurnAnimTbl + y));
    if (a == EnResetAnimIndex[x]) {
        return;
    }
    EnResetAnimIndex[x] = a;
    SetEnemyAnimIndex(x, a);
}

// --- $FBDD Spinning pieces ----------------------------------------------------------------------------

namespace {
void DoOnePiece(uint8_t x) {
    if (PieceTimer[x] == 0) {
        return;
    }
    PieceTimer[x]--;
    uint8_t y = (uint8_t)(x >> 1);
    Mem(0x04) = Read((uint16_t)(kTable17 + y));
    Mem(0x05) = Read((uint16_t)(kTable17 + y + 1));
    Mem(0x08) = PieceY[x];
    Mem(0x09) = PieceX[x];
    Mem(0x0B) = PieceHi[x];
    if (!MoveObjectByDeltas()) {
        PieceTimer[x] = 0;
        return;
    }
    PieceY[x] = Mem(0x08);
    PowerUpY = Mem(0x08);
    PieceX[x] = Mem(0x09);
    PowerUpX = Mem(0x09);
    PieceHi[x] = Mem(0x0B) & 0x01;
    PowerUpHi = PieceHi[x];
    PowerUpHi = PieceHi[x];
    PowerUpAnimFrame = 0x5A;
    DrawFrame();
    if (SamusBlink != 0) {
        return;
    }
    if (TouchCheckDC7F(0x40, 0)) {
        return;
    }
    if (!IsScrewAttackActive()) {
        return;
    }
    CrashF311(0, false);
    HealthLoChange = 0x50;
    SubtractHealth();
}
} // namespace

void UpdateSpinners() {
    PageIndex = 0x40;
    for (uint8_t x = 0x0C; x != 0; x -= 4) {
        DoOnePiece(x);
    }
    DoOnePiece(0x00);
}

// --- $FC65 Mellows / Memus -----------------------------------------------------------------------------

namespace {

void FD5F(uint8_t x) {
    Mem(0x0B) = MemuHi[x];
    Mem(0x08) = MemuY[x];
    Mem(0x09) = MemuX[x];
}

void FD6C(uint8_t x) {
    MemuY[x] = Mem(0x08);
    Mem(0x04F0) = Mem(0x08);
    MemuX[x] = Mem(0x09);
    Mem(0x04F1) = Mem(0x09);
    MemuHi[x] = Mem(0x0B) & 0x01;
    Mem(0x6BEB) = MemuHi[x];
}

void FD84(uint8_t x) {
    if (MemuHit[x] & 0x04) {
        MemuStatus[x] = 0x03;
    }
}

// $FD08: close enough to Samus to dive?
void FD08(uint8_t x) {
    MemuCounter[x] = 0;
    uint8_t y = 0;
    bool c = true;
    uint8_t a = Sbc(ObjectX, MemuX[x], c);
    if (a & 0x80) {
        y++;
        a = TwosCompliment(a);
    }
    if (a < 0x10) {
        MemuDir[x] = y;
        MemuStatus[x] = 0x02;
    }
}

// $FD25: hovering.
void FD25(uint8_t x) {
    bool c = (x >> 2) & 1;
    uint8_t a = Adc((uint8_t)(x >> 3), Mem(0x8A), c);
    Mem(0x8A) = a;
    Mem(0x8A) >>= 1;
    uint8_t y = a & 0x03;
    Mem(0x04) = Read((uint16_t)(kTable18 + y));
    Mem(0x05) = Read((uint16_t)(kTable18 + y + 1));
    FD5F(x);
    uint8_t dy = (uint8_t)(Mem(0x08) - ScrollY);
    a = 0x02;
    if (dy < 0x20) {
        Mem(0x04) = a;
    } else {
        a = TwosCompliment(a);
        if (dy >= 0x80) {
            Mem(0x04) = a;
        }
    }
    MoveObjectByDeltas();
    FD6C(x);
}

// $FCC1: diving.
void FCC1(uint8_t x) {
    FD5F(x);
    uint8_t a = MemuDir[x];
    if (a < 0x02 && Mem(0x08) >= ObjectY) {
        MemuDir[x] = a | 0x02;
    }
    a = MemuDir[x];
    Mem(0x05) = (a & 0x01) ? 0xFF : 0x01;
    Mem(0x04) = (a & 0x02) ? 0xFD : 0x04;
    MemuCounter[x]++;
    bool c = MoveObjectByDeltas();
    if (!c) {
        MemuDir[x] |= 0x02;
    } else {
        FD6C(x);
    }
    if (MemuCounter[x] >= 0x50) {
        MemuStatus[x] = 0x01;
    }
}

// $FC98. Returns true when DrawEnemy pops the return address.
bool DoOneMemu(uint8_t x) {
    uint8_t s = MemuStatus[x];
    ChooseRoutineEffects(0xFC9D, s);
    switch (s) {
        case 0x01:
            FD84(x);
            FD08(x);
            FD25(x);
            return DrawEnemy();
        case 0x02:
            FD84(x);
            FCC1(x);
            return DrawEnemy();
        case 0x03:
            MemuStatus[x] = 0;
            SFX_EnemyHit();
            return false;
        default:
            return false;
    }
}

} // namespace

void UpdateMellows() {
    if (Mem(0x6BE4) == 0) {
        return;
    }
    uint8_t x = 0xF0;
    PageIndex = x;
    if (Mem(0x6BE9) != Read(kMellowEnemy)) {
        KillObject(x);
        return;
    }
    UpdateEnemyAnim(0x03);
    Mem(0x8A) = RandomNumber1;
    for (int i = 0x18; i >= 0; i -= 8) {
        x = (uint8_t)i;
        if (DoOneMemu(x)) {
            // The original pops a return address here with a byte pushed on top of it: the stack is
            // corrupted (never happens in practice).
            return;
        }
        MemuHit[x] &= 0xF8;
    }
}

// --- $FD8F Moving objects --------------------------------------------------------------------------

bool MoveObjectByDeltas() {
    Mem(0x02) = ScrollDir & 0x02;
    uint8_t a = Mem(0x04);
    bool c = false;
    if (a & 0x80) {
        a = Adc(a, Mem(0x08), c);
        if (!c) {
            a = Sbc(a, 0x0F, c);
            if (Mem(0x02) != 0) {
                return false;
            }
            Mem(0x0B)++;
        }
        Mem(0x08) = a;
    } else if (a != 0) {
        a = Adc(a, Mem(0x08), c);
        if (c || a >= 0xF0) {
            c = true;
            a = Adc(a, 0x0F, c);
            if (Mem(0x02) != 0) {
                return false;
            }
            Mem(0x0B)++;
        }
        Mem(0x08) = a;
    }
    // $FDBF
    a = Mem(0x05);
    c = false;
    if (a & 0x80) {
        a = Adc(a, Mem(0x09), c);
        if (!c) {
            if (Mem(0x02) == 0) {
                return false;
            }
            Mem(0x0B)++;
        }
    } else {
        if (a == 0) {
            return true;
        }
        a = Adc(a, Mem(0x09), c);
        if (c) {
            if (Mem(0x02) == 0) {
                return false;
            }
            Mem(0x0B)++;
        }
    }
    Mem(0x09) = a;
    return true;
}

// --- $FDE3 ---------------------------------------------------------------------------------------------

namespace {
void AddUniqueItem() {
    StoreItemInHistory(); // $DC54
}

// $FE05: a Zebetite / item slot reaching 2 is noted in the unique item history.
void FE05(uint8_t x) {
    uint8_t a = (uint8_t)(Mem((uint16_t)(0x0758 + x)) - 2);
    if (a != 0) {
        return;
    }
    Mem(0x06) = a;
    Mem((uint16_t)(0x0758 + x))++;
    Mem(0x07) = (uint8_t)((x >> 1) + 0x3C);
    AddUniqueItem();
}
} // namespace

void UpdateObjectsFDE3() {
    if (EndTimerHi == 0x99) {
        bool c = false;
        uint8_t a = Sbc(0x99, EndTimerLo, c);
        if (a == 0) {
            Mem(0x06) = a;
            Mem(0x07) = 0x38;
            AddUniqueItem();
        }
    }
    for (uint8_t x = 0x20; x != 0; x -= 8) {
        FE05(x);
    }
    FE05(0x00);
}

// --- $FE1B Tile regrowth --------------------------------------------------------------------------------

namespace {

// $FE6D SetTileAnim
void SetTileAnim(uint8_t x, uint8_t a) {
    TileAnimIndex[x] = a;
    Mem((uint16_t)(0x0505 + x)) = a;
    TileAnimDelay[x] = 0;
}

// $FECC GetTileFramePtr
void GetTileFramePtr(uint8_t x) {
    uint8_t y = (uint8_t)(TileAnimFrame[x] << 1);
    Mem(0x02) = Read((uint16_t)(kTileBlastFrameTbl + y));
    Mem(0x03) = Read((uint16_t)(kTileBlastFrameTbl + y + 1));
}

// $FF3C: room RAM address in $00/$01 -> pixel coordinates $02 (y) and $03 (x).
void FF3C() {
    uint8_t lo = Mem(0x00);
    Mem(0x02) = lo & 0xE0;
    bool c = false;
    uint8_t a = Lsr(Mem(0x01), c);
    Mem(0x02) = Ror(Mem(0x02), c);
    a = Lsr(a, c);
    Mem(0x02) = Ror(Mem(0x02), c);
    Mem(0x03) = (uint8_t)((lo & 0x1F) << 3);
}

// $FF54 UpdateTileAnim. Returns true when the animation ended: the original pops DoOneTile's return
// address.
bool UpdateTileAnim(uint8_t delay) {
    uint8_t x = PageIndex;
    if (TileAnimDelay[x] != 0) {
        if (--TileAnimDelay[x] != 0) {
            return false;
        }
    }
    TileAnimDelay[x] = delay;
    uint8_t y = TileAnimIndex[x];
    uint8_t a = Read((uint16_t)(kTileBlastAnim + y));
    if (a == 0xFE) {
        TileRoutine[x]++;
        return true;
    }
    TileAnimFrame[x] = a;
    TileAnimIndex[x] = (uint8_t)(y + 1);
    if (DrawTileBlast()) {
        x = PageIndex;
        TileAnimIndex[x]--; // no room in the PPU string buffer: retry next frame
    }
    return false;
}

// $FE83: the tile is back; it hurts Samus if she is inside it.
void TileRespawned(uint8_t x) {
    TileRoutine[x] = 0;
    Mem(0x00) = (uint8_t)(TileWRAMLo[x] + 0x21);
    Mem(0x01) = TileWRAMHi[x];
    FF3C();
    Mem(0x07) = Mem(0x02);
    Mem(0x09) = Mem(0x03);
    Mem(0x0B) = (Mem(0x01) >> 2) & 0x01;
    GetObject1CoordData(0);
    Mem(0x04) = (uint8_t)(0x04 + ObjRadY);
    Mem(0x05) = (uint8_t)(0x04 + ObjRadX);
    if (CheckObjectDistance()) {
        return;
    }
    CrashF311(0, false);
    HealthLoChange = 0x50;
    SubtractHealth();
}

// $FE27. Returns true when UpdateTileAnim popped the return address.
bool DoOneTile(uint8_t x) {
    PageIndex = x;
    uint8_t a = TileRoutine[x];
    if (a == 0) {
        return false;
    }
    ChooseRoutineEffects(0xFE31, a);
    switch (a) {
        case 0x01: // $FE3D
            TileRoutine[x]++;
            SetTileAnim(x, 0x00);
            TileDelay[x] = 0x50;
            Mem(0x00) = TileWRAMLo[x];
            Mem(0x01) = TileWRAMHi[x];
            return UpdateTileAnim(0x02);
        case 0x02:
        case 0x04: // $FE54
            return UpdateTileAnim(0x02);
        case 0x03: // $FE59
            if ((FrameCount & 0x03) == 0 && --TileDelay[x] == 0) {
                TileRoutine[x]++;
                SetTileAnim(x, Read((uint16_t)(kTable19 + TileType[x])));
            }
            return false;
        case 0x05:
            TileRespawned(x);
            return false;
        default:
            return false;
    }
}

} // namespace

bool UpdateTiles() {
    uint8_t x = 0xC0;
    for (;;) {
        if (DoOneTile(x)) {
            return false;
        }
        x = (uint8_t)(PageIndex - 0x10);
        if (x == 0) {
            break;
        }
    }
    return DoOneTile(0x00);
}

bool DrawTileBlast() {
    if (PPUStrIndex >= 0x1F) {
        return true;
    }
    uint8_t x = PageIndex;
    Mem(0x00) = TileWRAMLo[x];
    Mem(0x01) = TileWRAMHi[x];
    GetTileFramePtr(x);
    Mem(0x11) = 0;
    uint8_t a = Read(Ptr(0x02));
    Mem(0x04) = a >> 4;
    Mem(0x05) = a & 0x0F;
    Mem(0x10) = 1;
    do {
        x = Mem(0x05);
        do {
            a = Read((uint16_t)(Ptr(0x02) + Mem(0x10)));
            Mem(0x10)++;
            Write((uint16_t)(Ptr(0x00) + Mem(0x11)), a);
            Mem(0x11)++;
        } while (--x != 0);
        Mem(0x11) = (uint8_t)(Mem(0x11) + 0x20 - Mem(0x05));
    } while (--Mem(0x04) != 0);
    if (Mem(0x01) & 0x04) {
        Mem(0x01) |= 0x0C;
    }
    Mem(0x01) &= 0x2F;
    EraseTile(); // $C328: the PPU string
    return false;
}

// $FB7B, for the room loader.
void EnemyInitDelay(uint8_t x) {
    FB7B(x, EnDataIndex[x]);
}

} // namespace Game
