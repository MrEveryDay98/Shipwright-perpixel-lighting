// Bank 7: Samus's action handlers, health and animation control ($CC0D-$D4BE).

#include "game/Samus.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Objects.h"
#include "game/Projectiles.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;
using namespace Const;

// --- $CC0D UpdateSamus -----------------------------------------------------------------------------

void UpdateSamus() {
    PageIndex = 0; // Samus's object slot
    IsSamus = 1;
    if (GoSamusHandler()) {
        return; // the end of Samus's explosion leaves UpdateSamus early (see DrawFrame)
    }
    IsSamus--;
}

// The handlers return true when DrawFrame popped UpdateSamus's return address.
bool GoSamusHandler() {
    uint8_t action = ObjAction;
    if (action & 0x80) {
        SamusStand();
        return false;
    }
    ChooseRoutineEffects(0xCC22, action);
    switch (action) {
        case sa_Stand:
            SamusStand();
            return false;
        case sa_Run:
            return SamusRun();
        case sa_Jump:
        case sa_PntJump:
            return SamusJump();
        case sa_Roll:
            return SamusRoll();
        case sa_PntUp:
            SamusPntUp();
            return false;
        case sa_Door:
            return SamusDoor();
        case sa_Dead:
            return SamusDead();
        case sa_Dead2:
            SamusDead2();
            return false;
        case sa_Elevator:
            return SamusElevator();
        default:
            return false;
    }
}

// --- $CC36 SamusStand ------------------------------------------------------------------------------

void SamusStand() {
    uint8_t a = Joy1Status & 0xCF; // ignore SELECT and START
    if (a != 0) {
        ClearHrztAnimData();
        a = Joy1Status;
    }
    uint8_t pressed = a & 0x07; // DOWN, LEFT or RIGHT held
    if (pressed == 0) {
        pressed = Joy1Change & 0x08; // or UP just pressed
    }
    if (pressed != 0) {
        uint8_t dir = BitScan(pressed); // 0 right, 1 left, 2 down, 3 up
        if (dir < 0x02) {
            SamusDir = dir;
        }
        static const uint16_t kActionTable = 0xCC87;
        ObjAction = Read((uint16_t)(kActionTable + dir));
    }
    if ((uint8_t)((Joy1Change | Joy1Retrig) << 1) & 0x80) { // FIRE (B)
        FireWeapon();
    }
    if (Joy1Change & 0x80) { // JUMP (A)
        ObjAction = sa_Jump;
    }
    if (SetSamusData(0x04)) {
        return; // DrawFrame left SamusStand (the original pops its return address)
    }
    uint8_t action = ObjAction;
    if (action >= sa_Door) {
        return;
    }
    ChooseRoutineEffects(0xCC7D, action);
    switch (action) {
        case 0:
            break; // ExitSub
        case 1:
            SetSamusRun();
            break;
        case 2:
            SetSamusJump();
            break;
        case 3:
            SetSamusRoll();
            break;
        case 4:
            SetSamusPntUp();
            break;
    }
}

// --- $CC8B --------------------------------------------------------------------------------------------

void SetSamusExplode() {
    SamusJmpDsplcmnt = 0x50;
    ObjectCounter = SetSamusAnim(an_Explode);
}

void SetSamusRun() {
    WalkSoundDelay = 0x09;
    int x = 0;
    uint8_t anim = AnimResetIndex;
    if (anim != an_SamusStand) {
        x++;
        if (anim != 0x27) {
            SetSamusNextAnim(0x04);
        }
    }
    static const uint16_t kRunAnimationTbl = 0xCCBE;
    AnimResetIndex = Read((uint16_t)(kRunAnimationTbl + x));
    SetRunAcceleration(SamusDir);
}

// $CCB7: the second half of SetSamusRun, also called on its own.
void SetRunAcceleration(uint8_t dir) {
    static const uint16_t kRunAccelerationTbl = 0xCCC0;
    SamusHorzAccel = Read((uint16_t)(kRunAccelerationTbl + dir));
}

// --- $CCC2 SamusRun ---------------------------------------------------------------------------------

bool SamusRun() {
    if (SamusGravity != 0) {
        // In the air (running jump).
        uint8_t y = SamusJmpDsplcmnt;
        if (!(ObjVertSpeed & 0x80)) {
            if (y < 0x18) {
                AnimResetIndex = an_SamusJump;
            }
        } else {
            if (y >= 0x18) {
                if (AnimResetIndex != an_SamusFireJump) {
                    AnimResetIndex = an_SamusSalto;
                }
                if (y >= 0x20) {
                    if (Joy1Status & 0x08) {
                        AnimResetIndex = an_SamusJmpPntUp;
                    }
                    if (!(Joy1Status & 0x80)) {
                        StopVertMovement();
                    }
                }
            }
        }
        if (AnimResetIndex == an_SamusRun) {
            AnimResetIndex = an_SamusJump;
        }
        if (SamusInLava != 0 && (Joy1Change & 0x80)) {
            return SamusRunJump();
        }
        AirSteer();
        JumpFire();
        CheckHitPushback();
        return SetSamusData(0x02);
    }
    if (SamusOnElevator == 0) {
        SetRunAcceleration(SamusDir);
    }
    bool left = false;
    if (RunPointUpCheck(left)) {
        return left; // it already set Samus's data
    }
    if (--WalkSoundDelay == 0) {
        WalkSoundDelay = 0x09;
        SFXSamusWalk();
    }
    CheckHitPushback();
    uint8_t change = Joy1Change;
    if (change & 0x80) {
        return SamusRunJump();
    }
    if ((uint8_t)((change | Joy1Retrig) << 1) & 0x80) {
        RunFire();
    }
    if ((Joy1Status & 0x03) == 0) {
        StopHorzMovement();
        return SetSamusData(0x03);
    }
    uint8_t dir = BitScan(Joy1Status & 0x03);
    if (dir != SamusDir) {
        SamusDir = dir;
        SetSamusRun();
    }
    return SetSamusData(0x03);
}

// $CD40: jumping out of a run.
bool SamusRunJump() {
    SetSamusJump();
    SamusHorzSpdMax = 0x12;
    return SetSamusData(0x03);
}

// $CDBF: switches between running and running while pointing up. Returns true if it did, in which case
// the original abandons SamusRun and finishes with SetSamusData(3), whose result goes in left.
bool RunPointUpCheck(bool& left) {
    uint8_t x = (Joy1Status & 0x08) >> 3;
    uint8_t anim = Read((uint16_t)(0xCCBE + x)); // RunAnimationTbl
    if (anim == AnimResetIndex) {
        return false;
    }
    SetSamusAnim(anim);
    left = SetSamusData(0x03);
    return true;
}

// $CDD7: firing while running.
void RunFire() {
    FireWeapon();
    if (!(Joy1Status & 0x08)) {
        AnimIndex = an_SamusFireRun;
        return;
    }
    uint8_t x = (uint8_t)(AnimIndex - AnimResetIndex) & 0x03;
    static const uint16_t kTable05 = 0xCDF6;
    SetSamusNextAnim(Read((uint16_t)(kTable05 + x)));
}

// --- $CD6D SetSamusData --------------------------------------------------------------------------------

// Returns DrawFrame's result: true when Samus's explosion ended and the original left the caller's caller.
bool SetSamusData(uint8_t animDelay) {
    UpdateObjAnim(animDelay);
    if (!IsScrewAttackActive()) {
        // Screw attack: cycle Samus's palette every other frame.
        ObjectCntrl = (uint8_t)(((FrameCount >> 1) & 0x03) | 0xA0);
    }
    CheckHealthStatus();
    LavaAndMoveCheck();
    if (MetroidOnSamus != 0) {
        ObjectCntrl = 0xA1; // a Metroid is latched on: Samus turns blue
    }
    SetmirrorCntrlBit();
    return DrawFrame();
}

void SetmirrorCntrlBit() {
    ObjectCntrl |= (uint8_t)(SamusDir << 4);
}

// Returns the carry: clear when the screw attack is active.
bool IsScrewAttackActive() {
    if (ObjAction != sa_Run || !(SamusGear & gr_SCREWATTACK)) {
        return true;
    }
    uint8_t anim = AnimResetIndex;
    if (anim != an_SamusSalto) {
        if (anim != an_SamusJump || !(ObjVertSpeed & 0x80)) {
            return true;
        }
    }
    return anim >= AnimIndex;
}

// --- $CDFA CheckHealthStatus ------------------------------------------------------------------------

void CheckHealthStatus() {
    if (SamusHit & 0x20) {
        // Just hit: blink for 50 frames and get knocked back.
        SamusBlink = 0x32;
        DmgPushDir = 0xFF;
        Mem(0x77) = Mem(0x73);
        if (Mem(0x77) != 0) {
            if (Mem(0x77) & 0x80) {
                SFX_SamusHit();
            }
            DmgPushDir = (SamusHit & 0x08) >> 3;
        }
        ObjVertSpeed = 0xFD;
        SamusGravity = 0x38;
        if (IsSamusDead()) {
            CheckHealthBeep();
            return;
        }
    }
    uint8_t blink = SamusBlink;
    if (blink == 0) {
        CheckHealthBeep();
        return;
    }
    SamusBlink--;
    uint8_t x = DmgPushDir;
    if ((uint8_t)(x + 1) != 0) {
        uint8_t a = blink >> 4;
        bool push = true;
        if (a < 0x03) {
            if (SamusHorzAccel != 0) {
                push = false;
            } else {
                ClearHorzMvmntDataY(SamusHorzAccel);
            }
        }
        if (push) {
            if (x == 0) {
                a = TwosCompliment(a);
            }
            ObjHorzSpeed = a;
        }
    }
    if ((Mem(0x77) & 0x80) && !(FrameCount & 0x01)) {
        // Flicker: skip drawing Samus this frame.
        AnimDelay = 0;
        AnimFrame = 0xF7;
    }
    CheckHealthBeep();
}

void CheckHealthBeep() {
    uint8_t y = (uint8_t)(HealthHi - 1);
    bool low = false;
    if (y & 0x80) {
        low = true; // under 10
    } else if (y == 0 && HealthLo < 0x70) {
        low = true; // under 17
    }
    if (low && (FrameCount & 0x0F) == 0) {
        SFXBeep();
    }
    SamusHit = 0;
}

// Returns true (the original's zero flag) when Samus is dead.
bool IsSamusDead() {
    uint8_t a = ObjAction;
    return a == sa_Dead || a == sa_Dead2 || a == 0xFF;
}

// --- $CE92 SubtractHealth / $CEF9 AddHealth -----------------------------------------------------------

void SubtractHealth() {
    if ((HealthLoChange | HealthHiChange) == 0) {
        return;
    }
    if (IsSamusDead() || (uint8_t)(EndTimerHi + 1) != 0 || MthrBrainStatus >= 0x03) {
        ClearHealthChange(); // no damage when dead, during the escape or while Mother Brain dies
        return;
    }
    if (SamusGear & gr_VARIA) {
        // Varia halves the damage.
        bool c = false;
        HealthLoChange = Lsr(HealthLoChange, c);
        HealthHiChange = Lsr(HealthHiChange, c);
        if (c) {
            HealthLoChange = Adc(0x4F, HealthLoChange, c);
        }
    }
    Mem(0x03) = HealthLo;
    bool carry = true;
    HealthLo = Base10Subtract(HealthLoChange, carry);
    Mem(0x03) = HealthHi;
    HealthHi = Base10Subtract(HealthHiChange, carry);
    if (((HealthLo & 0xF0) | HealthHi) != 0 && carry) {
        return;
    }
    HealthLo = 0;
    HealthHi = 0;
    ObjAction = sa_Dead;
    SFX_SamusDie();
    SetSamusExplode();
}

void AddHealth() {
    Mem(0x03) = HealthLo;
    bool carry = false;
    HealthLo = Base10Add(HealthLoChange, carry);
    Mem(0x03) = HealthHi;
    HealthHi = Base10Add(HealthHiChange, carry);
    uint8_t max = (uint8_t)((TankCount << 4) | 0x0F);
    if (max < HealthHi) {
        HealthHi = max & 0xF9;
        HealthLo = 0x99;
    }
    ClearHealthChange();
}

// --- $CF2E ------------------------------------------------------------------------------------------

// Turns Samus's acceleration around when she was hit while moving towards the push.
void CheckHitPushback() {
    bool fromRight = SamusHit & 0x01; // the carry the original's LSR leaves
    if (((SamusHit >> 1) & 0x02) == 0) {
        return;
    }
    uint8_t accel = SamusHorzAccel;
    if (!fromRight) {
        if (accel & 0x80) {
            return;
        }
    } else if (!(accel & 0x80) && accel != 0) {
        return;
    }
    SamusHorzAccel = TwosCompliment(accel);
    ClearHorzMvmntData();
}

// Returns Y (0).
uint8_t ClearHorzMvmntData() {
    return ClearHorzMvmntDataY(0);
}

// $CF4E: the second half, storing Y.
uint8_t ClearHorzMvmntDataY(uint8_t y) {
    ObjHorzSpeed = y;
    HorzCntrLinear = y;
    return y;
}

void StopHorzMovement() {
    if (SamusHorzAccel == 0) {
        SFXSamusWalk();
    }
    ClearHrztAnimData();
}

void ClearHrztAnimData() {
    uint8_t y = NoHorzMoveNoDelay();
    ObjAction = y; // standing
    if (Joy1Status & 0x08) {
        SetSamusPntUp();
        return;
    }
    SetSamusAnim(an_SamusStand);
}

// Returns A (the animation set), which callers may store.
uint8_t SetSamusAnim(uint8_t anim) {
    AnimResetIndex = anim;
    return SetSamusNextAnim(anim);
}

uint8_t SetSamusNextAnim(uint8_t anim) {
    AnimIndex = anim;
    AnimDelay = 0;
    return 0;
}

void SetSamusPntUp() {
    ObjAction = sa_PntUp;
    SetSamusAnim(an_SamusPntUp);
    NoHorzMoveNoDelay();
}

// Returns Y (0).
uint8_t NoHorzMoveNoDelay() {
    uint8_t y = ClearHorzData();
    AnimDelay = y;
    return y;
}

// $CF88: steering in the air.
void AirSteer() {
    if (Joy1Status & 0x03) {
        uint8_t x = BitScan(Joy1Status & 0x03);
        SetRunAcceleration(x);
        if (SamusGravity & 0x80) {
            return;
        }
        if (AnimResetIndex == an_SamusSalto) {
            return;
        }
        SamusDir = x;
        static const uint16_t kTable06 = 0xD096;
        SetSamusAnim(Read((uint16_t)(kTable06 + 1 + x)));
        return;
    }
    uint8_t g = SamusGravity;
    if ((g & 0x80) || g == 0) {
        return;
    }
    if (AnimResetIndex != an_SamusJump) {
        return;
    }
    ClearHorzData();
}

// Returns Y (0).
uint8_t ClearHorzData() {
    uint8_t y = ClearHorzMvmntData();
    SamusHorzAccel = y;
    return y;
}

// --- $CFBE / $CFC3 SetSamusJump ----------------------------------------------------------------------

static void StartJump(uint8_t anim) {
    AnimResetIndex = anim;
    AnimIndex = (uint8_t)(anim - 1);
    AnimDelay = 0x04;
    SamusJmpDsplcmnt = 0;
    ObjVertSpeed = 0xFC;
    if (ObjAction == sa_Run && (SamusGear & gr_SCREWATTACK)) {
        Mem(0x0686) = 0;
        SFX_ScrewAttack();
    }
    SFX_SamusJump();
    SamusGravity = (SamusGear & gr_HIGHJUMP) ? 0x12 : 0x18; // lower gravity: higher jump
}

void SetSamusJumpPntUp() {
    StartJump(an_SamusJmpPntUp);
}

void SetSamusJump() {
    StartJump(an_SamusJump);
}

// --- $D41A SamusDead / $D41F SamusDead2 --------------------------------------------------------------

bool SamusDead() {
    return SetSamusData(0x01);
}

void SamusDead2() {
    AnimDelay--;
}

// --- $D002 SamusJump -------------------------------------------------------------------------------------

bool SamusJump() {
    if ((ObjVertSpeed & 0x80) && SamusJmpDsplcmnt >= 0x20 && !(Joy1Status & 0x80)) {
        StopVertMovement(); // JUMP released after 32 pixels: start falling
    }
    JumpSteer();
    CheckHitPushback();
    if (Joy1Status & 0x08) {
        AnimResetIndex = an_SamusJmpPntUp;
        ObjAction = sa_PntJump;
    }
    JumpFire();
    if (SamusInLava != 0 && (Joy1Change & 0x80)) {
        SetSamusJump();
        return SetSamusData(0x03);
    }
    if (SamusGravity == 0) {
        // Landed.
        if (ObjAction == sa_PntJump) {
            SetSamusPntUp();
        }
        StopHorzMovement(); // the original's branch after SetSamusPntUp is never taken
    }
    return SetSamusData(0x03);
}

// $D055: turning around in the air.
void JumpSteer() {
    uint8_t x, y = 0;
    uint8_t pad = Joy1Status;
    if (pad & 0x01) { // RIGHT
        x = 0x01;
    } else if (pad & 0x02) { // LEFT
        x = 0xFF;
        y = 1;
    } else {
        ObjHorzSpeed = 0;
        return;
    }
    if (y != SamusDir) {
        uint8_t anim = AnimResetIndex;
        if (ObjAction == sa_PntJump) {
            static const uint16_t kTable04 = 0xD099;
            if (anim == Read((uint16_t)(kTable04 + y))) {
                anim = Read((uint16_t)(kTable04 + 1 + y));
            }
        } else {
            static const uint16_t kTable06 = 0xD096;
            if (anim == Read((uint16_t)(kTable06 + y))) {
                anim = Read((uint16_t)(kTable06 + 1 + y));
            }
        }
        SetSamusAnim(anim);
        AnimDelay = 0x08;
        SamusDir = y;
    }
    ObjHorzSpeed = x;
}

// $D09C: firing while jumping.
void JumpFire() {
    if (!((uint8_t)((Joy1Change | Joy1Retrig) << 1) & 0x80)) {
        return;
    }
    if (AnimResetIndex == an_SamusJmpPntUp) {
        FireUp();
        return;
    }
    FireHorizontal();
    SetSamusAnim(an_SamusFireJump);
}

// --- $D0B5 SetSamusRoll / $D0E1 SamusRoll -----------------------------------------------------------

void SetSamusRoll() {
    if (!(SamusGear & gr_MARUMARI) || SamusGravity != 0) {
        ObjAction = sa_Stand;
        return;
    }
    uint8_t x = SamusDir;
    AnimResetIndex = an_SamusRoll;
    AnimIndex = an_SamusRunJump;
    SamusHorzAccel = Read((uint16_t)(0xCCC0 + x)); // RunAccelerationTbl
    Mem(0x0686) = 0x01;
    SFX_SamusBall();
}

bool SamusRoll() {
    bool leave = (Joy1Change & 0x08) || (Joy1Change & 0x80); // UP or JUMP pressed
    if (leave && !(Joy1Status & 0x04)) {
        // Try to stand up.
        bool carry = false;
        ObjRadY = Adc(ObjRadY, 0x08, carry);
        if (CheckMoveUp()) {
            Regs r = Original(0xE8BE, { 0, 0x00, 0 });
            Mem(0x05) = r.x;
            Mem(0x04) = 0xF5;
            r = Original(0xFD8F, r);
            StoreObjectCoords(r.x);
            StopHorzMovement();
            AnimIndex--;
            StopVertMovement();
            return SetSamusData(0x04);
        }
    }
    uint8_t dir = BitScan(Joy1Change);
    if (dir < 0x02) {
        SamusDir = dir;
        SetSamusAnim(an_SamusRoll);
    }
    SetRunAcceleration(SamusDir);
    CheckHitPushback();
    CheckBombLaunch();
    if ((Joy1Status & 0x03) == 0) {
        ClearHorzData();
    }
    return SetSamusData(0x02);
}

// Returns Y (0).
uint8_t StopVertMovement() {
    ObjVertSpeed = 0;
    VertCntrLinear = 0;
    return 0;
}

// $D150: lays a bomb in the first free bomb slot (D, E or F) when FIRE is pressed.
void CheckBombLaunch() {
    if (!(SamusGear & gr_BOMBS)) {
        return;
    }
    if (!((uint8_t)((Joy1Change | Joy1Retrig) << 1) & 0x80)) {
        return;
    }
    if ((ObjVertSpeed | SamusOnElevator) != 0) {
        return;
    }
    uint8_t x = 0xD0;
    if (ObjAction[x] != 0) {
        x = 0xE0;
        if (ObjAction[x] != 0) {
            x = 0xF0;
            if (ObjAction[x] != 0) {
                return;
            }
        }
    }
    ObjectHi[x] = ObjectHi;
    ObjectX[x] = ObjectX;
    ObjectY[x] = (uint8_t)(ObjectY + 0x04);
    ObjAction[x] = wa_LayBomb;
    SFXBombLaunch();
}

// --- $D198 SamusPntUp --------------------------------------------------------------------------------

void SamusPntUp() {
    if (!(Joy1Status & 0x08)) {
        ObjAction = sa_Stand;
    }
    if (Joy1Status & 0x07) {
        uint8_t x = BitScan(Joy1Status & 0x07);
        if (x < 0x02) {
            SamusDir = x;
        }
        static const uint16_t kTable07 = 0xD1EB;
        ObjAction = Read((uint16_t)(kTable07 + x));
    }
    if ((uint8_t)((Joy1Change | Joy1Retrig) << 1) & 0x80) {
        FireWeapon();
    }
    if (Joy1Change & 0x80) {
        ObjAction = sa_PntJump;
    }
    if (SetSamusData(0x04)) {
        return;
    }
    uint8_t action = ObjAction;
    ChooseRoutineEffects(0xD1D7, action);
    switch (action) {
        case 0:
            StopHorzMovement();
            break;
        case 1:
            SetSamusRun();
            break;
        case 3:
            SetSamusRoll();
            break;
        case 6:
            SetSamusJumpPntUp();
            break;
        default:
            break;
    }
}

// --- $D1EE FireWeapon -----------------------------------------------------------------------------------

void FireWeapon() {
    if (Joy1Status & 0x08) {
        FireUp();
    } else {
        FireHorizontal();
    }
}

// $D1F7: finds a free projectile slot (D0, E0, F0). Returns true (the original's zero flag) with its
// index in y when one is free and usable: missiles only fire from slot D0.
bool FindProjectileSlot(uint8_t& y) {
    y = 0xD0;
    while (ObjAction[y] != 0) {
        y += 0x10;
        if (y == 0) {
            y = 1;
            return false;
        }
    }
    Mem((uint16_t)(0x030A + y)) = 0;
    if (MissileToggle == 0) {
        return true;
    }
    return y == 0xD0;
}

// $D210
void FireHorizontal() {
    uint8_t y;
    if (MetroidOnSamus == 0 && FindProjectileSlot(y)) {
        InitProjectile(y);
        SetWaveBeam(y, SamusDir);
        SetIceBeam(y);
        Mem((uint16_t)(0x030F + y)) = 0x0C;      // frames the shot lasts
        static const uint16_t kTable99 = 0xD273; // bullet speeds
        ObjHorzSpeed[y] = Read((uint16_t)(kTable99 + SamusDir));
        ObjVertSpeed[y] = 0;
        ObjectOnScreen[y] = 1;
        CheckMissileLaunch(y);
        static const uint16_t kTable08 = 0xD26F;
        uint8_t x = (uint8_t)(((ObjAction[y] << 1) | SamusDir) & 0x03);
        Mem(0x05) = Read((uint16_t)(kTable08 + x));
        Mem(0x04) = 0xFA;
        y = PlaceProjectile(y);
        Mem(0x061F) |= (uint8_t)((SamusGear & gr_LONGBEAM) << 5);
        if (ObjAction[y] == 0x01) {
            SFX_BulletFire();
        }
    }
    SetSamusNextAnim(0x09);
}

// $D275
void FireUp() {
    uint8_t y;
    if (MetroidOnSamus == 0 && FindProjectileSlot(y)) {
        InitProjectile(y);
        SetWaveBeam(y, 0x02);
        SetIceBeam(y);
        Mem((uint16_t)(0x030F + y)) = 0x0C;
        ObjVertSpeed[y] = 0xFC;
        ObjHorzSpeed[y] = 0;
        ObjectOnScreen[y] = 1;
        CheckMissileLaunchUp(y);
        static const uint16_t kTable09 = 0xD2E3;
        Mem(0x05) = Read((uint16_t)(kTable09 + 4 + SamusDir));
        Mem(0x04) = Read((uint16_t)(kTable09 + 6 + (ObjAction[y] & 0x01)));
        y = PlaceProjectile(y);
        Mem(0x061F) |= (uint8_t)((SamusGear & gr_LONGBEAM) << 5);
        if (ObjAction[y] == 0x01) {
            SFX_BulletFire();
        }
    }
    static const uint16_t kTable09 = 0xD2E3;
    uint8_t x = SamusDir;
    uint8_t anim = Read((uint16_t)(kTable09 + x));
    if (SamusGravity != 0) {
        anim = Read((uint16_t)(kTable09 + 2 + x));
    }
    if (ObjAction != sa_Run) {
        SetSamusNextAnim(anim);
    }
}

// $D2EB: a new regular shot in slot y.
void InitProjectile(uint8_t y) {
    ObjAction[y]++;
    ObjRadY[y] = 0x02;
    ObjRadX[y] = 0x02;
    SetProjectileAnim(y, an_Bullet);
}

void SetProjectileAnim(uint8_t x, uint8_t anim) {
    AnimResetIndex[x] = anim;
    AnimIndex[x] = anim;
    AnimDelay[x] = 0;
}

// $D306: positions the new shot ($04/$05: offset from Samus). Returns the slot (the original reloads Y).
uint8_t PlaceProjectile(uint8_t y) {
    Regs r = Original(0xE8BE, { 0, 0x00, y });
    r.x = r.y;
    r = Original(0xFD8F, r);
    r.y = r.x;
    StoreObjectCoords(r.x);
    return r.y;
}

void CheckMissileLaunch(uint8_t y) {
    if (MissileToggle == 0 || y != 0xD0) {
        return;
    }
    static const uint16_t kMissileAnims = 0xD33E;
    LaunchMissile(y, Read((uint16_t)(kMissileAnims + SamusDir)));
}

// $D340
void CheckMissileLaunchUp(uint8_t y) {
    if (MissileToggle == 0 || y != 0xD0) {
        return;
    }
    LaunchMissile(y, 0x8F);
}

// $D323
void LaunchMissile(uint8_t y, uint8_t anim) {
    SetBulletAnim(y, anim);
    SFXMissileLaunch();
    ObjAction[y] = wa_Missile;
    Mem((uint16_t)(0x030F + y)) = 0xFF;
    if (--MissileCount != 0) {
        return;
    }
    MissileToggle--; // out of missiles: back to the beam
    SelectSamusPal();
}

void SetBulletAnim(uint8_t y, uint8_t anim) {
    AnimIndex[y] = anim;
    AnimResetIndex[y] = anim;
    AnimDelay[y] = 0;
}

// $D359/$D35B: direction for the shot ($0502,y), and the wave beam if Samus has it.
void SetWaveBeam(uint8_t y, uint8_t dir) {
    Mem((uint16_t)(0x0502 + y)) = dir;
    if (!(SamusGear & gr_WAVEBEAM) || MissileToggle != 0) {
        return;
    }
    Mem((uint16_t)(0x0501 + y)) = 0;
    Mem((uint16_t)(0x0304 + y)) = 0;
    Mem((uint16_t)(0x0500 + y)) = ((y >> 4) & 0x01) ? 0x00 : 0x0C;
    ObjAction[y] = wa_WaveBeam;
    SetBulletAnim(y, an_WaveBeam);
    SFX_WaveFire();
}

// $D38E
void SetIceBeam(uint8_t y) {
    if (MissileToggle != 0 || !(SamusGear & gr_ICEBEAM)) {
        return;
    }
    ObjAction[y] = wa_IceBeam;
    Mem(0x061F) |= 0x01;
    SFX_BulletFire();
}

// --- $D3A8 SamusDoor ---------------------------------------------------------------------------------------

bool SamusDoor() {
    uint8_t status = DoorStatus;
    if (status >= 0x05) {
        if (--DoorDelay == 0) {
            if (status & 0x80) {
                DoorStatus = status & 0x7F;
            } else {
                // Out of the door: the new room is set up.
                DoorEnemyCleanup();
                Original(0xED65);
                AreaHook(0x95AB, {});
                uint8_t music = ItemRmMusicSts;
                bool setItemMusic = false;
                if (music != 0) {
                    StartMusic();
                    if (music & 0x80) {
                        ItemRmMusicSts = 0;
                    } else {
                        setItemMusic = true;
                    }
                }
                for (;;) {
                    if (setItemMusic) {
                        ItemRmMusicSts = 0x80;
                        setItemMusic = false;
                    }
                    if (KrdRdlyPresent == 0) {
                        break;
                    }
                    TourianMusic();
                    KrdRdlyPresent = 0;
                    setItemMusic = true;
                }
                ObjAction = SamusDoorData & 0x0F;
                SamusDoorData = 0;
                DoorStatus = 0;
                StopVertMovement();
                MoveOutDoor();
            }
        } else {
            MoveOutDoor();
        }
    }
    CheckHealthStatus();
    SetmirrorCntrlBit();
    return DrawFrame();
}

void MoveOutDoor() {
    if (SamusDoorDir != 0) {
        if (ObjectX == 0) {
            ToggleSamusHi();
        }
        ObjectX--;
    } else {
        if (++ObjectX == 0) {
            ToggleSamusHi();
        }
    }
}

// --- $D423 SamusElevator --------------------------------------------------------------------------------

bool SamusElevator() {
    uint8_t status = ElevatorStatus;
    if (status == 0x03 || status == 0x08) {
        uint8_t y;
        if (!(Mem(0x032F) & 0x80)) {
            // Going down.
            if ((uint8_t)(ObjectY - ScrollY) >= 0x84) {
                ScrollDown();
            }
            y = ObjectY;
            if (y == 239) {
                ToggleSamusHi();
                y = 0xFF;
            }
            ObjectY = (uint8_t)(y + 1);
        } else {
            // Going up.
            if ((uint8_t)(ObjectY - ScrollY) < 0x64) {
                ScrollUp();
            }
            y = ObjectY;
            if (y == 0) {
                ToggleSamusHi();
                y = 240;
            }
            ObjectY = (uint8_t)(y - 1);
        }
    } else {
        ObjVertSpeed = 0;
        if (status == 0x05 || status == 0x07) {
            SetmirrorCntrlBit();
            return AnimDrawObject(0x01);
        }
    }
    if (!(FrameCount & 0x01)) {
        return false;
    }
    SetmirrorCntrlBit();
    return AnimDrawObject(0x01);
}

// $D48C: frees enemies and objects that don't survive a room change.
void DoorEnemyCleanup() {
    for (int x = 0x60; x >= 0; x -= 0x20) {
        if (!(Mem((uint16_t)(0x0405 + x)) & 0x02)) {
            Enstatus[x] = 0;
        }
    }
    uint8_t y = GetNameTable();
    for (int x = 0x18; x >= 0; x -= 0x08) {
        if (y == Mem((uint16_t)(0x072C + x))) {
            Mem((uint16_t)(0x0728 + x)) = 0xFF;
        }
    }
}

// --- $E249 ToggleSamusHi ----------------------------------------------------------------------------------

void ToggleSamusHi() {
    ObjectHi ^= 0x01;
}

// --- $E1E1 BitScan ------------------------------------------------------------------------------------

// The number of the lowest set bit of a (8 if none). The original saves X in $0E around it.
uint8_t BitScan(uint8_t a) {
    uint8_t x = 0;
    while (!(a & 1)) {
        a >>= 1;
        if (++x == 8) {
            break;
        }
    }
    return x;
}

// --- Not translated yet ---

bool CheckMoveUp() {
    return Original(0xE7A2).c;
}
uint8_t GetNameTable() {
    return Original(0xEB85).a;
}

void ClearHealthChange() {
    Original(0xF323);
}

} // namespace Game
