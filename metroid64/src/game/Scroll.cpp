// Bank 7: door scrolling, lava, Samus's movement and screen scrolling with name table updates
// ($E1F1-$E71F).

#include "game/Scroll.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Memory.h"
#include "game/Projectiles.h"
#include "game/Ram.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;
using namespace Const;

// --- $E1F1 ScrollDoor --------------------------------------------------------------------------------

// Scrolls the screen while Samus is in a door: DoorStatus 1/2 scroll right/left a screen, 3/4 centre a
// vertical shaft first (down/up), then continue horizontally.
void ScrollDoor() {
    uint8_t x;
    switch (DoorStatus) {
        case 1:
        case 2:
            if (DoorStatus == 1) {
                ScrollRight();
            } else {
                ScrollLeft();
            }
            if (ScrollX != 0) {
                return;
            }
            // Scrolled a full screen: Samus comes out of the door.
            x = 0x05;
            DoorDelay = 0x20;
            if ((SamusDoorData & 0x20) || DoorScrollStatus >= 0x03) {
                // Item rooms and the like don't toggle; a shaft door already toggled when it was centred.
                MirrorCntrl = 0x47;
            } else {
                MirrorCntrl = ToggleScroll();
            }
            DoorStatus = x;
            return;
        case 3:
        case 4:
            if (DoorStatus == 3) {
                ScrollDown();
            } else {
                ScrollUp();
            }
            VertRoomCentered();
            return;
        default:
            return;
    }
}

// $E21B: once a vertical shaft is centred on the door, scrolling turns horizontal and Samus is carried out
// through the door. Returns X (the new DoorStatus, or ScrollY while still centring).
uint8_t VertRoomCentered() {
    if (ScrollY != 0) {
        return ScrollY;
    }
    DoorOnNameTable3 = 0;
    DoorOnNameTable0 = 0;
    uint8_t x = (ObjectX & 0x80) ? 0x01 : 0x02; // right hand door: scroll right
    MirrorCntrl = ToggleScroll();
    DoorStatus = x;
    return x;
}

// --- $E25D Lava ------------------------------------------------------------------------------------------

// Returns the carry: clear when Samus is in lava, which is anywhere in the bottom 24 pixels of a
// horizontal room.
bool IsSamusInLava() {
    if (ScrollDir <= 0x01) {
        return true;
    }
    return ObjectY <= 0xD8;
}

// $E269
void LavaAndMoveCheck() {
    if (ObjAction != sa_Elevator && ObjAction >= sa_Dead) {
        return;
    }
    uint8_t inLava = 0;
    if (!IsSamusInLava()) {
        DmgPushDir = 0xFF; // lava doesn't push Samus
        ClearHealthChange();
        SamusBlink = 0x32;
        if ((FrameCount & 0x03) == 0) {
            SFX_SamusJump();
        }
        // Damage twice every 8 frames, once with the varia.
        bool odd = FrameCount & 0x01;
        if (((FrameCount >> 1) & 0x03) == 0 && (!(SamusGear & gr_VARIA) || odd)) {
            HealthLoChange = 0x07;
            SubtractHealth();
        }
        inLava = 1;
    }
    SamusInLava = inLava;
    SamusMoveVertically();
}

// $E2A9
void SamusMoveVertically() {
    VertAccelerate();
    SamusScrY = (uint8_t)(ObjectY - ScrollY);
    uint8_t a = Mem(0x00);
    if (a & 0x80) {
        // Moving up.
        a = TwosCompliment(a);
        if (SamusInLava != 0) {
            a >>= 1;
            if (a == 0) {
                SamusMoveHorizontally();
                return;
            }
        }
        ObjectCounter = a;
        do {
            if (!MoveSamusUp()) {
                // Blocked: halve the speed and turn it downwards.
                bool carry = true;
                ObjVertSpeed = Ror(ObjVertSpeed, carry);
                VertCntrLinear = Ror(VertCntrLinear, carry);
                break;
            }
        } while (--ObjectCounter != 0);
        SamusMoveHorizontally();
        return;
    }
    if (a != 0 && SamusInLava != 0) {
        a >>= 2;
    }
    if (a == 0) {
        SamusMoveHorizontally();
        return;
    }
    // Moving down.
    ObjectCounter = a;
    do {
        if (!MoveSamusDown()) {
            if (ObjAction == sa_Roll) {
                // The ball bounces until it is too slow.
                bool carry = ObjVertSpeed & 0x01;
                ObjVertSpeed >>= 1;
                if (ObjVertSpeed != 0) {
                    VertCntrLinear = Ror(VertCntrLinear, carry);
                    carry = true;
                    VertCntrLinear = Sbc(0x00, VertCntrLinear, carry);
                    ObjVertSpeed = Sbc(0x00, ObjVertSpeed, carry);
                    SamusMoveHorizontally();
                    return;
                }
            } else {
                SFXSamusWalk(); // landed
            }
            SamusGravity = StopVertMovement();
            break;
        }
    } while (--ObjectCounter != 0);
    SamusMoveHorizontally();
}

// $E31A
void SamusMoveHorizontally() {
    HorzAccelerate();
    SamusScrX = (uint8_t)(ObjectX - ScrollX);
    uint8_t a = Mem(0x00);
    bool left = a & 0x80;
    if (left) {
        a = TwosCompliment(a);
    } else if (a == 0) {
        return;
    }
    if (SamusInLava != 0) {
        a >>= 1;
        if (a == 0) {
            return;
        }
    }
    ObjectCounter = a;
    do {
        CheckStopHorzMvmt(left ? MoveSamusLeft() : MoveSamusRight());
    } while (--ObjectCounter != 0);
    if (SamusDoorData != 0) {
        SamusDoorDir = left ? 0x01 : 0x00;
    }
}

// $E365
void CheckStopHorzMvmt(bool moved) {
    if (moved) {
        return;
    }
    ObjectCounter = 0x01; // stop moving this frame
    if (SamusGravity != 0 || ObjAction == sa_Roll) {
        return;
    }
    StopHorzMovement();
}

// --- $E37A Acceleration ------------------------------------------------------------------------------

// Gravity: $38 when hit, $1A falling, $18 jumping, $12 jumping with the high jump boots. Leaves the
// vertical speed for this frame in $00.
void VertAccelerate() {
    bool falling = true;
    if (SamusGravity == 0) {
        SamusHorzSpdMax = 0x18;
        if (((ObjectY + ObjRadY) & 0x07) == 0 && !CheckMoveDown()) {
            falling = false; // standing on something solid
        }
        if (falling) {
            SamusOnElevatorOrEnemy();
            if (SamusOnElevator == 0 && OnFrozenEnemy == 0) {
                SamusGravity = 0x1A; // walked off a ledge
            }
        }
    }
    uint8_t x = 0x05; // maximum downward speed
    bool carry = false;
    VertCntrLinear = Adc(VertCntrLinear, SamusGravity, carry);
    ObjVertSpeed = Adc(ObjVertSpeed, 0x00, carry);
    uint8_t a = ObjVertSpeed;
    if (a & 0x80) {
        carry = VertCntrLinear == 0;
        a = Sbc(0x00, ObjVertSpeed, carry);
        carry = a >= 0x06;
        x = 0xFA; // maximum upward speed
    } else {
        carry = a >= 0x05;
    }
    if (carry) {
        StopVertMovement();
        ObjVertSpeed = x;
    }
    carry = false;
    VertCntrNonLinr = Adc(VertCntrNonLinr, VertCntrLinear, carry);
    Mem(0x00) = Adc(0x00, ObjVertSpeed, carry);
}

// $E3E5: leaves the horizontal speed for this frame in $00.
void HorzAccelerate() {
    Mem(0x00) = Mem(0x02) = (uint8_t)(SamusHorzSpdMax << 4);
    Mem(0x01) = Mem(0x03) = (uint8_t)(SamusHorzSpdMax >> 4);
    bool carry = false;
    HorzCntrLinear = Adc(HorzCntrLinear, SamusHorzAccel, carry);
    uint8_t x = HorzCntrLinear;
    ObjHorzSpeed = Adc((SamusHorzAccel & 0x80) ? 0xFF : 0x00, ObjHorzSpeed, carry);
    uint8_t y = ObjHorzSpeed;
    if (y & 0x80) {
        // Moving left: compare the magnitude, cap at the negated maximum.
        carry = true;
        x = Sbc(0x00, HorzCntrLinear, carry);
        y = Sbc(0x00, ObjHorzSpeed, carry);
        carry = true;
        Mem(0x00) = Sbc(0x00, Mem(0x00), carry);
        Mem(0x01) = Sbc(0x00, Mem(0x01), carry);
    }
    carry = x >= Mem(0x02);
    Sbc(y, Mem(0x03), carry);
    if (carry) {
        HorzCntrLinear = Mem(0x00);
        ObjHorzSpeed = Mem(0x01);
    }
    carry = false;
    HorzCntrNonLinr = Adc(HorzCntrNonLinr, HorzCntrLinear, carry);
    Mem(0x00) = Adc(0x00, ObjHorzSpeed, carry);
}

// --- $E457 Moving one pixel ----------------------------------------------------------------------------
// Each returns the carry: set when Samus moved. Collisions are only checked on 8-pixel boundaries.

bool MoveSamusUp() {
    if (((uint8_t)(ObjectY - ObjRadY) & 0x07) == 0 && !CheckMoveUp()) {
        return false;
    }
    if (ObjAction != sa_Elevator) {
        SamusOnElevatorOrEnemy();
        if ((SamusHit & 0x42) == 0x42) {
            return false;
        }
    }
    if (SamusScrY >= 0x66 || ScrollUp()) {
        SamusScrY--; // the screen didn't scroll: Samus moves on it
    }
    if (ObjectY == 0) {
        if (!(ScrollDir & 0x02)) {
            ToggleSamusHi();
        }
        ObjectY = 240;
    }
    ObjectY--;
    SamusJmpDsplcmnt++;
    return true;
}

bool MoveSamusDown() {
    if (((uint8_t)(ObjectY + ObjRadY) & 0x07) == 0 && !CheckMoveDown()) {
        return false;
    }
    if (ObjAction != sa_Elevator) {
        SamusOnElevatorOrEnemy();
        if (SamusOnElevator != 0 || OnFrozenEnemy != 0) {
            return false;
        }
    }
    if (SamusScrY < 0x84 || ScrollDown()) {
        SamusScrY++;
    }
    if (ObjectY == 239) {
        if (!(ScrollDir & 0x02)) {
            ToggleSamusHi();
        }
        ObjectY = 0xFF;
    }
    ObjectY++;
    SamusJmpDsplcmnt--;
    return true;
}

bool MoveSamusLeft() {
    if (((uint8_t)(ObjectX - ObjRadX) & 0x07) == 0 && !CheckMoveLeft()) {
        SamusDoorData = 0;
        return false;
    }
    SamusOnElevatorOrEnemy();
    if ((SamusHit & 0x41) == 0x41) {
        SamusDoorData = 0;
        return false;
    }
    if (SamusScrX >= 0x71 || ScrollLeft()) {
        SamusScrX--;
    }
    if (ObjectX == 0 && (ScrollDir & 0x02)) {
        ToggleSamusHi();
    }
    ObjectX--;
    return true;
}

bool MoveSamusRight() {
    if (((uint8_t)(ObjectX + ObjRadX) & 0x07) == 0 && !CheckMoveRight()) {
        SamusDoorData = 0;
        return false;
    }
    SamusOnElevatorOrEnemy();
    if ((SamusHit & 0x41) == 0x40) {
        SamusDoorData = 0;
        return false;
    }
    if (SamusScrX < 0x8F || ScrollRight()) {
        SamusScrX++;
    }
    ObjectX++;
    if (ObjectX == 0 && (ScrollDir & 0x02)) {
        ToggleSamusHi();
    }
    return true;
}

// --- $E4F1 Scrolling ---------------------------------------------------------------------------------------
// Each scrolls one pixel and returns the carry: set when the screen can't scroll that way (wrong scroll
// direction, or no room there on the map). Turning around in a shaft moves the map position back to
// the screen that is still showing.

bool ScrollUp() {
    if (ScrollDir == 1) {
        ScrollDir--;
        if (ScrollY != 0) {
            MapPosY--;
        }
    } else if (ScrollDir != 0) {
        return true;
    }
    uint8_t x = ScrollY;
    if (x == 0) {
        MapPosY--;
        if (GetRoomNum()) {
            MapPosY++;
            return true;
        }
        SwitchNameTable();
        x = 240;
    }
    ScrollY = (uint8_t)(x - 1);
    CheckNameTableUpdate();
    return false;
}

bool ScrollDown() {
    if (ScrollDir == 0) {
        ScrollDir++;
        if (ScrollY != 0) {
            MapPosY++;
        }
    } else if (ScrollDir != 1) {
        return true;
    }
    if (ScrollY == 0) {
        MapPosY++;
        if (GetRoomNum()) {
            MapPosY--;
            return true;
        }
    }
    uint8_t x = ScrollY;
    if (x == 239) {
        SwitchNameTable();
        x = 0xFF;
    }
    ScrollY = (uint8_t)(x + 1);
    CheckNameTableUpdate();
    return false;
}

bool ScrollLeft() {
    if (ScrollDir == 3) {
        ScrollDir--;
        if (ScrollX != 0) {
            MapPosX--;
        }
    } else if (ScrollDir != 2) {
        return true;
    }
    if (ScrollX == 0) {
        MapPosX--;
        if (GetRoomNum()) {
            MapPosX++;
            return true;
        }
        SwitchNameTable();
    }
    ScrollX--;
    CheckNameTableUpdate();
    return false;
}

bool ScrollRight() {
    if (ScrollDir == 2) {
        ScrollDir++;
        if (ScrollX != 0) {
            MapPosX++;
        }
    } else if (ScrollDir != 3) {
        return true;
    }
    if (ScrollX == 0) {
        MapPosX++;
        if (GetRoomNum()) {
            MapPosX--;
            return true;
        }
    }
    ScrollX++;
    if (ScrollX == 0) {
        SwitchNameTable();
    }
    CheckNameTableUpdate();
    return false;
}

// $E54A: keeps loading the room ahead; once it's loaded, copies a row or column of it into the name table
// each time the scroll crosses a tile boundary.
void CheckNameTableUpdate() {
    SetupRoom();
    if ((uint8_t)(RoomNumber + 1) != 0) {
        return;
    }
    if (!(ScrollDir & 0x02)) {
        VertNameTableUpdate();
    } else {
        HorzNameTableUpdate();
    }
}

// $E571
void VertNameTableUpdate() {
    static const uint16_t kTable11 = 0xE55E; // ScrollY & 7 at which to update: 7 going up, 0 going down
    if ((ScrollY & 0x07) != Read((uint16_t)(kTable11 + ScrollDir)) || ScrollDir != TempScrollDir) {
        return;
    }
    // $01:$00 = row * 32
    Mem(0x00) = ScrollY & 0xF8;
    uint8_t a = 0;
    bool carry;
    for (int i = 0; i < 2; i++) {
        carry = false;
        Mem(0x00) = Asl(Mem(0x00), carry);
        a = Rol(a, carry);
    }
    WriteNameTableLine(a);
}

// $E701
void HorzNameTableUpdate() {
    static const uint16_t kTable02 = 0xE6FF - 2; // ScrollX & 7 at which to update: 7 going left, 0 right
    if ((ScrollX & 0x07) != Read((uint16_t)(kTable02 + ScrollDir)) || ScrollDir != TempScrollDir) {
        return;
    }
    Mem(0x00) = (uint8_t)((ScrollX & 0xF8) >> 3); // column
    WriteNameTableLine(0x00);
}

// $E590: queues one row (32 tiles) or column (30 tiles) of room RAM at offset hi:$00 for the name table.
void WriteNameTableLine(uint8_t hi) {
    static const uint16_t kTable01 = 0xE5E0; // PPU string control bytes: row, column
    Mem(0x01) = hi;
    uint8_t wramHi;
    uint8_t ppuHi = GetNameAddrs(wramHi);
    Mem(0x03) = ppuHi | Mem(0x01);
    Mem(0x01) = wramHi | Mem(0x01);
    Mem(0x02) = Mem(0x00);
    Mem(0x04) = Read((uint16_t)(kTable01 + (ScrollDir >> 1)));
    PPUDataPending = 1;
    uint8_t x = PPUStrIndex;
    if (!WritePPUByte(x, Mem(0x03)) || !WritePPUByte(x, Mem(0x02))) {
        return;
    }
    // SeparateControlBits
    uint8_t control = Mem(0x04);
    PPUDataString[x] = control & 0xBF;
    Mem(0x05) = control & 0x3F;
    if (!NextPPUByte(x)) {
        return;
    }
    do {
        if (!WritePPUByte(x, Read((uint16_t)(Mem(0x00) | (Mem(0x01) << 8))))) {
            return;
        }
        Mem(0x06) = 0;
        AddYToPtr00((Mem(0x04) & 0x80) ? 0x20 : 0x01);
    } while (--Mem(0x05) != 0);
    PPUStrIndex = x;
    EndPPUString(x);
}

// $E564: the high bytes of the name table being drawn (returned) and of its room RAM.
uint8_t GetNameAddrs(uint8_t& wramHi) {
    static const uint16_t kPPUAddrs = 0xE560;
    static const uint16_t kWRAMAddrs = 0xE562;
    uint8_t y = GetNameTable() & 0x01;
    wramHi = Read((uint16_t)(kWRAMAddrs + y));
    return Read((uint16_t)(kPPUAddrs + y));
}

// $E5E2: queues one of the two halves of the attribute table (RoomNumber $F2 is the first pass).
void WritePPUAttribTbl() {
    uint8_t lo = RoomNumber == 0xF2 ? 0xC0 : 0xE0;
    Mem(0x00) = lo;
    Mem(0x02) = lo;
    uint8_t wramHi;
    Mem(0x03) = GetNameAddrs(wramHi) | 0x03;
    Mem(0x01) = wramHi | 0x03;
    PPUDataPending = 1;
    uint8_t x = PPUStrIndex;
    if (!WritePPUByte(x, Mem(0x03)) || !WritePPUByte(x, Mem(0x02))) {
        return;
    }
    Mem(0x04) = 0x20;
    if (!WritePPUByte(x, 0x20)) {
        return;
    }
    uint16_t src = Mem(0x00) | (Mem(0x01) << 8);
    for (uint8_t y = 0; Mem(0x04) != 0; y++, Mem(0x04)--) {
        if (!WritePPUByte(x, Read((uint16_t)(src + y)))) {
            return;
        }
    }
    PPUStrIndex = x;
    EndPPUString(x);
}

} // namespace Game
