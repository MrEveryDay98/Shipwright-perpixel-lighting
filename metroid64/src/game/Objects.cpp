// Bank 7: object animation and sprite drawing ($DC8F-$E093).

#include "game/Objects.h"

#include "game/Constants.h"
#include "game/Area.h"
#include "game/Enemies.h"
#include "game/Engine.h"
#include "game/Original.h"
#include "game/Present.h"
#include "game/Ram.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;
using namespace Const;

namespace {
// $FA18 KillObject lives in the enemy code (Enemies.cpp); X = the enemy slot.
void KillObject() {
    Original(0xFA18, { 0, PageIndex, 0 });
}
constexpr uint16_t kObjectAnimIdxTbl = 0x8572; // in the area bank
constexpr uint16_t kFramePtrTable = 0x860B;
constexpr uint16_t kPlacePtrTable = 0x86DF;
constexpr uint16_t kExplodeRotationTbl = 0xDC8B;
constexpr uint16_t kExplodeIndexTbl = 0xE049;

uint16_t Ptr00() {
    return (uint16_t)(Mem(0x00) | (Mem(0x01) << 8));
}
uint16_t Ptr02() {
    return (uint16_t)(Mem(0x02) | (Mem(0x03) << 8));
}
} // namespace

// --- $DC8F UpdateObjAnim -----------------------------------------------------------------------------

// Advances the animation of the object at PageIndex. delay is the number of frames each new frame shows.
void UpdateObjAnim(uint8_t delay) {
    uint8_t x = PageIndex;
    if (AnimDelay[x] != 0) {
        if (--AnimDelay[x] != 0) {
            return;
        }
    }
    AnimDelay[x] = delay;
    uint8_t y = AnimIndex[x];
    for (;;) {
        uint8_t frame = Read((uint16_t)(kObjectAnimIdxTbl + y));
        if (frame != 0xFF) {
            AnimFrame[x] = frame;
            AnimIndex[x] = (uint8_t)(y + 1);
            return;
        }
        y = AnimResetIndex[x]; // end of the animation: start over
    }
}

// $DCB7: $06 = the sign extension of a (0 or $FF). Returns with the carry clear.
uint8_t SignExtendTo06(uint8_t a) {
    Mem(0x06) = 0;
    if (a & 0x80) {
        Mem(0x06)--;
    }
    return a;
}

// --- $DCC3 GetSpriteCntrlData ------------------------------------------------------------------------

// Reads the control byte of the frame at ($00) (AABBXXXX: flip bits, palette, placement entry). Sets $04
// and $05 and returns the placement entry * 2 (the original returns it in both A and X).
uint8_t GetSpriteCntrlData() {
    Mem(0x0F) = 0;
    uint8_t control = Read(Ptr00());
    Mem(0x04) = control;
    // Palette bits to bits 0-1, flip bits kept, bit 5 set (behind the background).
    Mem(0x05) = (uint8_t)((control & 0xC0) | 0x20 | ((control >> 4) & 0x03));
    Mem(0x04) ^= (uint8_t)((ObjectCntrl & 0x10) << 2); // object mirroring flips the sprite's
    if (ObjectCntrl & 0x80) {
        ObjectCntrl = (uint8_t)(ObjectCntrl << 1);
        SpriteFlipBitsOveride(0);
    }
    return (uint8_t)((control & 0x0F) << 1);
}

// --- $DCF5 Enemy death: item drops -------------------------------------------------------------------

// Reached from DrawEnemy when an enemy's explosion ends. The original pops DrawEnemy's caller's return
// address here, so that caller is left (DrawEnemy returns true).
void EnemyExplosionDone() {
    ClearObjectCntrl();
    uint8_t x = PageIndex;
    if (InArea == 0x13) {
        uint8_t d = EnDataIndex[x];
        if (d == 0x04 || d == 0x02) {
            KillObject(); // Tourian's Metroids leave nothing
            return;
        }
    }
    if ((uint8_t)(Mem((uint16_t)(0x040C + x)) << 1) & 0x80) {
        MiniBossKilled();
        return;
    }
    Regs r = Original(0xF74B, { 0, x, 0 });
    GenByte00 = r.a;
    r = AreaHook(0x80B0, r);
    EnDataIndex[x] = r.a & 0x20;
    Enstatus[x] = 0x05;
    Mem((uint16_t)(0x040D + x)) = 0x60;
    uint8_t a = RandomNumber1;
    if (a < 0x10) {
        NoItemDrop();
        return;
    }
    ChooseItemDrop(a);
}

// $DD30: picks the drop from ItemDropTbl, unless enough of that pickup is already out.
void ChooseItemDrop(uint8_t a) {
    uint8_t x = PageIndex;
    static const uint16_t kItemDropTbl = 0xDE35;
    uint8_t drop = Read((uint16_t)(kItemDropTbl + (a & 0x07)));
    EnAnimFrame[x] = drop;
    if (drop == 0x80) { // missile
        if (MaxMissilePickup == CrntMslePickups || MaxMissiles == 0) {
            NoItemDrop();
            return;
        }
        CrntMslePickups++;
        return;
    }
    if (MaxEnergyPickup == CrntEnrgyPickups) {
        NoItemDrop();
        return;
    }
    CrntEnrgyPickups++;
    if (drop != 0x89) {
        return;
    }
    bool carry = GenByte00 & 0x01;
    GenByte00 = (uint8_t)(GenByte00 >> 1);
    if (carry) {
        return;
    }
    NoItemDrop();
}

// $DD5B
void NoItemDrop() {
    if (InArea != 0x13) {
        KillObject();
        return;
    }
    // Tourian always drops something: reset the pickup counts and try again.
    uint8_t a = RandomNumber1;
    CrntEnrgyPickups = 0;
    CrntMslePickups = 0;
    MaxMissilePickup = 1;
    MaxEnergyPickup = 1;
    ChooseItemDrop(a);
}

// $DD75: Kraid or Ridley died.
void MiniBossKilled() {
    PowerUpMusic();
    uint8_t a = InArea & 0x0F;
    MiniBossKillDly = a;
    a >>= 1;
    MaxMissiles[a] = a;
    AddToMaxMissiles(75);
    NoItemDrop();
}

// $DD97: adds to both MissileCount and MaxMissiles (capped at 255). Returns the new MaxMissiles.
uint8_t AddToMaxMissiles(uint8_t n) {
    unsigned c = MissileCount + n;
    MissileCount = c > 0xFF ? 0xFF : (uint8_t)c;
    unsigned m = MaxMissiles + n;
    MaxMissiles = m > 0xFF ? 0xFF : (uint8_t)m;
    return MaxMissiles;
}

// --- $DD8B DrawEnemy ---------------------------------------------------------------------------------

// Draws the enemy at PageIndex. Returns true when its explosion ended: the original then leaves the
// routine that called DrawEnemy (see EnemyExplosionDone).
static bool DrawEnemyFrame();

// Draws the enemy at PageIndex (its sprites are tagged for the presentation).
bool DrawEnemy() {
    uint8_t from = SpritePagePos, slot = PageIndex;
    bool done = DrawEnemyFrame();
    Present::TagSprites(from, Present::Owner::Enemy, slot);
    return done;
}

static bool DrawEnemyFrame() {
    uint8_t x = PageIndex;
    if (EnAnimFrame[x] == 0xF7) {
        ClearObjectCntrl();
        return false;
    }
    Mem(0x0A) = EnYRoomPos[x];
    Mem(0x0B) = EnXRoomPos[x];
    Mem(0x06) = EnNameTable[x];
    // Frame pointers: two tables ($41 and $43), the second for frames $80 and up.
    uint8_t frame = EnAnimFrame[x];
    uint8_t y = (uint8_t)(frame << 1);
    uint16_t table =
        (frame & 0x80) ? (uint16_t)(Mem(0x43) | (Mem(0x44) << 8)) : (uint16_t)(Mem(0x41) | (Mem(0x42) << 8));
    Mem(0x00) = Read((uint16_t)(table + y));
    Mem(0x01) = Read((uint16_t)(table + (uint8_t)(y + 1)));
    uint8_t place = GetSpriteCntrlData();
    uint16_t places = (uint16_t)(Mem(0x45) | (Mem(0x46) << 8));
    Mem(0x02) = Read((uint16_t)(places + place));
    Mem(0x03) = Read((uint16_t)(places + (uint8_t)(place + 1)));
    y = 0;
    if (place == 0x02) {
        // Exploding: spin the pieces; after 25 frames the enemy is done.
        x = PageIndex;
        uint8_t counter = ++EnCounter[x];
        Mem(0x05) = (uint8_t)((Mem(0x05) & 0x3F) | Read((uint16_t)(kExplodeRotationTbl + (counter & 0x03))));
        if (counter == 0x19) {
            EnemyExplosionDone();
            return true;
        }
    }
    x = PageIndex;
    y++;
    EnRadY[x] = Read((uint16_t)(Ptr00() + y));
    ReduceYRadius(EnRadY[x]);
    y++;
    EnRadX[x] = Read((uint16_t)(Ptr00() + y));
    Mem(0x09) = EnRadX[x];
    y++;
    Mem(0x11) = y;
    Mem(0x08) = (uint8_t)(IsObjectVisible() << 1);
    x = PageIndex;
    Mem((uint16_t)(0x0405 + x)) = (uint8_t)((Mem((uint16_t)(0x0405 + x)) & 0xFD) | Mem(0x08));
    if (Mem(0x08) == 0) {
        ClearObjectCntrl();
        return false;
    }
    DrawSpriteObject(SpritePagePos);
    return false;
}

// --- $DE3D ReduceYRadius -----------------------------------------------------------------------------

uint8_t ReduceYRadius(uint8_t radius) {
    bool carry = true;
    uint8_t a = Sbc(radius, 0x10, carry);
    if (!carry) {
        a = 0;
    }
    Mem(0x08) = a;
    return a;
}

// --- $DE47 AnimDrawObject / $DE4A DrawFrame ------------------------------------------------------------

bool AnimDrawObject(uint8_t delay) {
    UpdateObjAnim(delay);
    return DrawFrame();
}

// Draws the object at PageIndex into sprite RAM. Returns true in one case: Samus's explosion finished.
// The original then pops a return address, so the routine that called DrawFrame's caller is left too
// (for Samus, UpdateSamus doesn't get to clear IsSamus).
bool DrawFrame() {
    uint8_t x = PageIndex;
    uint8_t frame = AnimFrame[x];
    if (frame == 0xF7) {
        ClearObjectCntrl(); // no frame to show
        return false;
    }
    if (frame == 0x07) {
        ObjectCntrl &= 0xEF; // Samus facing the screen is never mirrored (elevators)
    }
    Mem(0x0A) = ObjectY[x];
    Mem(0x0B) = ObjectX[x];
    Mem(0x06) = ObjectHi[x];
    uint8_t f = (uint8_t)(AnimFrame[x] << 1);
    Mem(0x00) = Read((uint16_t)(kFramePtrTable + f));
    Mem(0x01) = Read((uint16_t)(kFramePtrTable + f + 1));
    uint8_t place = GetSpriteCntrlData();
    Mem(0x02) = Read((uint16_t)(kPlacePtrTable + place));
    Mem(0x03) = Read((uint16_t)(kPlacePtrTable + place + 1));
    if (IsSamus != 0 && place == 0x0E) {
        // Samus exploding: spin the pieces, and after 25 frames move on to the second death handler.
        x = PageIndex;
        uint8_t counter = ++ObjectCounter;
        Mem(0x05) = (uint8_t)((Mem(0x05) & 0x3F) | Read((uint16_t)(kExplodeRotationTbl + (counter & 0x03))));
        if (counter == 0x19) {
            x = PageIndex;
            ObjAction[x] = sa_Dead2;
            AnimDelay[x] = 0x28;
            ClearObjectCntrl();
            return true;
        }
    }
    x = PageIndex;
    uint8_t y = 1;
    ObjRadY[x] = Read((uint16_t)(Ptr00() + y));
    ReduceYRadius(ObjRadY[x]);
    y++;
    ObjRadX[x] = Read((uint16_t)(Ptr00() + y));
    Mem(0x09) = ObjRadX[x];
    y++;
    Mem(0x11) = y;
    uint8_t visible = IsObjectVisible();
    ObjectOnScreen[PageIndex] = visible;
    if (visible == 0) {
        ClearObjectCntrl();
        return false;
    }
    DrawSpriteObject(SpritePagePos);
    return false;
}

// --- $DEE6 WriteSpriteRAM / $DF19 DrawSpriteObject -------------------------------------------------------

// Draws the sprites of the frame at ($00) from index $11, using the placement data at ($02) from index $0F
// and the screen position in $10/$0E. x is the next free byte of sprite RAM.
void DrawSpriteObject(uint8_t x) {
    for (;;) {
        uint8_t y = Mem(0x11);
        uint8_t b = Read((uint16_t)(Ptr00() + y));
        if (b < 0xFC) {
            // WriteSpriteRAM: one sprite.
            y = Mem(0x0F);
            SpriteRAM[x] = (uint8_t)(YDisplacement(y) + Mem(0x10));
            SpriteRAM[x]--; // the PPU draws sprites one line lower
            Mem(0x0F)++;
            y = Mem(0x11);
            SpriteRAM[x + 1] = Read((uint16_t)(Ptr00() + y));
            SpriteRAM[x + 2] = (uint8_t)(((ObjectCntrl << 2) & 0x40) ^ Mem(0x05));
            Mem(0x11)++;
            y = Mem(0x0F);
            SpriteRAM[x + 3] = (uint8_t)(XDisplacement(y) + Mem(0x0E));
            Mem(0x0F)++;
            x += 4;
            continue;
        }
        if (b == 0xFC) {
            // OffsetObjectPosition: move the rest of the frame.
            y++;
            Mem(0x10) = (uint8_t)(Mem(0x10) + Read((uint16_t)(Ptr00() + y)));
            Mem(0x11) += 2;
            y = Mem(0x11);
            Mem(0x0E) = (uint8_t)(Mem(0x0E) + Read((uint16_t)(Ptr00() + y)));
            Mem(0x11)++;
            continue;
        }
        if (b == 0xFD) {
            // GetNewControlByte: new sprite attributes for the next sprites.
            y++;
            bool carry = ObjectCntrl & 0x80;
            ObjectCntrl = (uint8_t)(ObjectCntrl << 1);
            if (carry) {
                SpriteFlipBitsOveride(y);
            } else {
                ObjectCntrl = (uint8_t)(ObjectCntrl >> 1);
                Mem(0x05) = Read((uint16_t)(Ptr00() + y));
            }
            y++;
            Mem(0x11) = y;
            continue;
        }
        if (b == 0xFE) {
            // SkipPlacementData
            Mem(0x0F) += 2;
            Mem(0x11)++;
            continue;
        }
        SpritePagePos = x; // $FF: end of frame
        ClearObjectCntrl();
        return;
    }
}

void ClearObjectCntrl() {
    ObjectCntrl = 0;
}

// --- $DF6B Sprite placement --------------------------------------------------------------------------

// The y offset of a sprite from placement byte y. Returns the offset; the original returns with the
// carry clear, ready for adding the object's position.
uint8_t YDisplacement(uint8_t y) {
    uint8_t p = Read((uint16_t)(Ptr02() + y));
    uint8_t a = p;
    if ((p & 0xF0) == 0x80) {
        // ExplodeYDisplace: pieces fly apart over time.
        uint8_t idx = Read((uint16_t)(kExplodeIndexTbl + ((p & 0x0E) >> 1)));
        uint8_t counter = IsSamus != 0 ? ObjectCounter : EnCounter[PageIndex];
        uint8_t t = (uint8_t)(idx + counter);
        uint8_t move = Read((uint16_t)(kExplodeIndexTbl + 2 + t));
        a = (uint8_t)(move + Read((uint16_t)(Ptr02() + (uint8_t)(Mem(0x0F) + 0x0C))));
    }
    if (Mem(0x04) & 0x80) {
        return NegativeDisplacement(a);
    }
    return a;
}

uint8_t XDisplacement(uint8_t y) {
    uint8_t p = Read((uint16_t)(Ptr02() + y));
    uint8_t a = p;
    if ((p & 0xF0) == 0x80) {
        // ExplodeXDisplace: two pixels per frame, left or right by the placement byte's bit 0.
        uint8_t counter = EnCounter[PageIndex];
        if (IsSamus != 0) {
            counter = ObjectCounter;
        }
        uint8_t move = (uint8_t)(counter << 1);
        if (!(Read((uint16_t)(Ptr02() + Mem(0x0F))) & 0x01)) {
            move = (uint8_t)((move ^ 0xFF) + 1);
        }
        a = (uint8_t)(move + Read((uint16_t)(Ptr02() + (uint8_t)(Mem(0x0F) + 0x0C))));
    }
    if (Mem(0x04) & 0x40) {
        return NegativeDisplacement(a);
    }
    return a;
}

// Mirrors an 8-pixel sprite's offset: -a - 8, plus one as the original computes it.
uint8_t NegativeDisplacement(uint8_t a) {
    bool carry = true;
    return Adc((uint8_t)(a ^ 0xFF), 0xF8, carry);
}

// --- $DFDF IsObjectVisible -------------------------------------------------------------------------------

// Uses the object's room position ($0A, $0B), name table ($06) and x radius ($09). Sets its screen position
// in $10/$0E. Returns 1 if it is fully on screen, else 0.
uint8_t IsObjectVisible() {
    uint8_t objY = Mem(0x0A);
    bool carry = true;
    Mem(0x10) = Sbc(objY, ScrollY, carry);
    carry = true;
    Mem(0x0E) = Sbc(Mem(0x0B), ScrollX, carry);
    bool otherTable = ((Mem(0x06) ^ PPUCNT0ZP) & 0x01) != 0;
    if (!(ScrollDir & 0x02)) {
        // VertScrollCheck
        carry = objY >= ScrollY;
        if (otherTable) {
            if (carry) {
                return 0;
            }
            Mem(0x10) = Sbc(Mem(0x10), 0x0F, carry);
            uint8_t bottom = (uint8_t)(Mem(0x09) + Mem(0x10));
            return bottom < 0xF0 ? 1 : 0;
        }
        if (!carry) {
            return 0;
        }
        return Mem(0x09) < Mem(0x10) ? 1 : 0;
    }
    // HorzScrollCheck (the carry is still the one from subtracting ScrollX)
    if (otherTable) {
        if (carry) {
            return 0;
        }
        return (Mem(0x09) + Mem(0x0E)) <= 0xFF ? 1 : 0;
    }
    if (!carry) {
        return 0;
    }
    return Mem(0x09) < Mem(0x0E) ? 1 : 0;
}

// --- $E038 SpriteFlipBitsOveride --------------------------------------------------------------------

// The object's flip bits take priority over the frame's: $05 = frame control byte y's flip bits | ObjectCntrl.
void SpriteFlipBitsOveride(uint8_t y) {
    ObjectCntrl = (uint8_t)(ObjectCntrl >> 1);
    Mem(0x05) = (uint8_t)((Read((uint16_t)(Ptr00() + y)) & 0xC0) | ObjectCntrl);
    ObjectCntrl |= 0x80;
}

// --- $E094 UpdateEnemyAnim ----------------------------------------------------------------------------

// Steps the animation of the enemy at PageIndex once its delay runs out; delay is the new delay. The
// enemy's animation data is at (EnemyAnimPtr), $FF marking the end of a sequence.
void UpdateEnemyAnim(uint8_t delay) {
    uint8_t x = PageIndex;
    if (Enstatus[x] == 0x05) {
        return; // dying
    }
    if (EnAnimDelay[x] != 0 && --EnAnimDelay[x] != 0) {
        return;
    }
    EnAnimDelay[x] = delay;
    uint8_t y = EnAnimIndex[x];
    uint8_t frame;
    while ((frame = Read((uint16_t)(EnemyAnimPtr.Word() + y))) == 0xFF) {
        y = EnResetAnimIndex[x];
    }
    EnAnimFrame[x] = frame;
    EnAnimIndex[x] = (uint8_t)(y + 1);
}

} // namespace Game
