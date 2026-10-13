// Bank 7: Samus's action handlers, health and animation control ($CC0D-$D4BE).

#include "game/Samus.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Objects.h"
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
            SamusJump();
            return false;
        case sa_Roll:
            SamusRoll();
            return false;
        case sa_PntUp:
            SamusPntUp();
            return false;
        case sa_Door:
            SamusDoor();
            return false;
        case sa_Dead:
            return SamusDead();
        case sa_Dead2:
            SamusDead2();
            return false;
        case sa_Elevator:
            SamusElevator();
            return false;
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
        L_D09C();
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

// --- Not translated yet --------------------------------------------------------------------------------

void SamusJump() {
    Original(0xD002);
}
void SamusRoll() {
    Original(0xD0E1);
}
void SamusPntUp() {
    Original(0xD198);
}
void SamusDoor() {
    Original(0xD3A8);
}
void SamusElevator() {
    Original(0xD423);
}
void SetSamusRoll() {
    Original(0xD0B5);
}
void StopVertMovement() {
    Original(0xD147);
}
void L_D09C() {
    Original(0xD09C);
}
void FireWeapon() {
    Original(0xD1EE);
}
void LavaAndMoveCheck() {
    Original(0xE269);
}
void ClearHealthChange() {
    Original(0xF323);
}

} // namespace Game
