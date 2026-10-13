// Bank 7: Samus's projectiles and bombs, elevators, starting the area music ($D4BF-$D9D3).

#include "game/Projectiles.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Original.h"
#include "game/Ram.h"
#include "game/Samus.h"

namespace Game {

using namespace Vars;
using namespace Const;

namespace {
uint16_t Ptr04() {
    return (uint16_t)(Mem(0x04) | (Mem(0x05) << 8));
}
} // namespace

// --- $D4BF UpdateProjectiles ------------------------------------------------------------------------

void UpdateProjectiles() {
    DoOneProjectile(0xD0);
    DoOneProjectile(0xE0);
    DoOneProjectile(0xF0);
}

void DoOneProjectile(uint8_t x) {
    PageIndex = x;
    uint8_t action = ObjAction[x];
    ChooseRoutineEffects(0xD4D3, action);
    switch (action) {
        case 0x01:
        case 0x0B:
            UpdateBullet();
            break;
        case 0x02:
            UpdateWaveBullet();
            break;
        case 0x03:
            UpdateIceBullet();
            break;
        case 0x04:
            BulletExplode();
            break;
        case 0x05:
        case 0x08:
            LayBomb();
            break;
        case 0x06:
        case 0x09:
            BombCountdown();
            break;
        case 0x07:
        case 0x0A:
            BombExplode();
            break;
        default:
            break;
    }
}

// --- $D4EB UpdateBullet ------------------------------------------------------------------------------

void UpdateBullet() {
    UpdtngPrjctl = 1;
    KillIfOffScreen();
    CheckProjectileHit();
    bool carry = MoveProjectile();
    CheckBulletStat(carry);
}

// $D4F8: carry set when the shot moved freely this frame.
void CheckBulletStat(bool carry) {
    uint8_t x = PageIndex;
    if (carry) {
        if (!(SamusGear & gr_LONGBEAM)) {
            if (--Mem((uint16_t)(0x030F + x)) == 0) {
                ObjAction[x] = 0; // short range: the shot runs out
            }
        }
    } else {
        if (ObjAction[x] == 0) {
            UpdtngPrjctl--;
            return;
        }
        ExplodeProjectile(x);
    }
    DrawBullet();
}

void DrawBullet() {
    AnimDrawObject(0x01);
    UpdtngPrjctl--;
}

// --- $D52C UpdateWaveBullet ----------------------------------------------------------------------------

void UpdateWaveBullet() {
    UpdtngPrjctl = 1;
    KillIfOffScreen();
    uint8_t x = PageIndex;
    CheckProjectileHit();
    x = PageIndex;
    // The wave's path: pairs of [frames][speeds], from Table0C (sideways) or Table0D (upwards).
    static const uint16_t kTable0A = 0xD58B;
    uint8_t y = Mem((uint16_t)(0x0502 + x)) & 0xFE;
    Mem(0x0A) = Read((uint16_t)(kTable0A + y));
    Mem(0x0B) = Read((uint16_t)(kTable0A + y + 1));
    uint16_t path = (uint16_t)(Mem(0x0A) | (Mem(0x0B) << 8));
    uint8_t step;
    for (;;) {
        y = Mem((uint16_t)(0x0500 + x));
        step = Read((uint16_t)(path + y));
        if (step == 0xFF) {
            Mem((uint16_t)(0x0500 + x)) = step; // wrap to the start
            Mem((uint16_t)(0x0500 + x))++;
            Mem((uint16_t)(0x0501 + x)) = 0;
            continue;
        }
        if (step != Mem((uint16_t)(0x0501 + x))) {
            break;
        }
        // This step's frames are used up: next step.
        Mem((uint16_t)(0x0500 + x)) += 2;
        Mem((uint16_t)(0x0501 + x)) = 0;
    }
    Mem((uint16_t)(0x0501 + x))++;
    y++;
    Regs r = AreaHook(0x8296, { Read((uint16_t)(path + y)), x, y });
    x = PageIndex;
    ObjVertSpeed[x] = r.a;
    r = AreaHook(0x832F, { Read((uint16_t)(path + r.y)), x, r.y });
    x = PageIndex;
    ObjHorzSpeed[x] = r.a;
    if (Mem((uint16_t)(0x0502 + x)) & 0x01) {
        ObjHorzSpeed[x] = TwosCompliment(r.a);
    }
    bool carry = MoveProjectile();
    if (!carry) {
        carry = MoveObjectByTiles();
    }
    CheckBulletStat(carry);
}

void UpdateIceBullet() {
    ObjectCntrl = 0x81;
    UpdateBullet();
}

void BulletExplode() {
    UpdtngPrjctl = 1;
    uint8_t x = PageIndex;
    if (AnimFrame[x] == 0xF7) {
        ObjAction[x] = 0;
    }
    DrawBullet();
}

// $D5DA: when the shot hit something ($030A set by the collision code), it explodes.
void CheckProjectileHit() {
    uint8_t x = PageIndex;
    if (Mem((uint16_t)(0x030A + x)) == 0) {
        return;
    }
    Mem((uint16_t)(0x030A + x)) = 0;
    ExplodeProjectile(x);
}

// $D5E4
void ExplodeProjectile(uint8_t x) {
    uint8_t action = ObjAction[x];
    if (action == wa_BulletExplode) {
        return;
    }
    SetProjectileAnim(x, action == wa_Missile ? an_MissileExpld : 0x1D);
    ObjAction[x] = wa_BulletExplode;
}

// $D5FC
void KillIfOffScreen() {
    uint8_t x = PageIndex;
    if (!(ObjectOnScreen[x] & 0x01)) {
        ObjAction[x] = 0;
    }
}

// $D609: moves the projectile, or hits the tile in its way. Returns the carry: set when it moved.
bool MoveProjectile() {
    GetObjCoords();
    uint8_t tile = Read(Ptr04());
    if (tile >= 0xA0) {
        return MoveObjectByTiles(); // passable
    }
    Regs r = AreaHook(0x95C0, { tile });
    tile = r.a;
    if (tile == 0x4E) {
        return DoorTileHit();
    }
    if (!IsBlastableRange(tile)) {
        return false; // a wall
    }
    return IsBlastTile(tile, r.x, false);
}

// $D624: moves the object at PageIndex by its speeds. Returns the carry: clear (and the projectile
// removed) when it ran into something.
bool MoveObjectByTiles() {
    uint8_t x = PageIndex;
    Mem(0x05) = ObjHorzSpeed[x];
    Mem(0x04) = ObjVertSpeed[x];
    Regs r = { ObjCoordsToTemp(x), x, 0 };
    r = Original(0xFD8F, r);
    if (!r.c) {
        ObjAction[PageIndex] = 0; // the original reaches this through KillIfOffScreen's tail
        return false;
    }
    StoreObjectCoords(r.x);
    return true;
}

// $D638: the object's new position from $08/$09/$0B.
void StoreObjectCoords(uint8_t x) {
    ObjectY[x] = Mem(0x08);
    ObjectX[x] = Mem(0x09);
    ObjectHi[x] = Mem(0x0B) & 0x01;
}

// $D648
void ToggleObjectHi(uint8_t x) {
    ObjectHi[x] ^= 0x01;
}

// $D651: carry set for tiles a shot or bomb can blast (from $70, or $80 in Brinstar).
bool IsBlastableRange(uint8_t tile) {
    if (InArea != 0x10 && tile >= 0x70) {
        return true;
    }
    return tile >= 0x80;
}

// --- Bombs ---------------------------------------------------------------------------------------------

// $D65E
void LayBomb() {
    uint8_t x = PageIndex;
    SetProjectileAnim(x, an_BombTick);
    Mem((uint16_t)(0x030F + x)) = 0x18; // fuse
    ObjAction[x]++;
    DrawBomb();
}

void DrawBomb() {
    AnimDrawObject(0x03);
}

// $D670
void BombCountdown() {
    uint8_t x = PageIndex;
    if (FrameCount & 0x01) {
        if (--Mem((uint16_t)(0x030F + x)) == 0) {
            uint8_t anim = ObjAction[x] == 0x09 ? an_BombExplode : 0x37;
            SetProjectileAnim(x, anim);
            ObjAction[x]++;
            SFXBombExplode();
        }
    }
    DrawBomb();
}

// $D691
void BombExplode() {
    uint8_t x = PageIndex;
    Mem((uint16_t)(0x030F + x))++;
    BombBlastTiles();
    x = PageIndex;
    if (AnimFrame[x] == 0xF7) {
        ObjAction[x] = 0;
    }
    DrawBomb();
}

// $D6A7: on frames 1-5 of the explosion, blasts the tile at the bomb, above, below, left and right.
void BombBlastTiles() {
    GetObjCoords();
    Mem(0x0A) = Mem(0x04);
    Mem(0x0B) = Mem(0x05);
    uint8_t x = PageIndex;
    uint8_t step = Mem((uint16_t)(0x030F + x));
    bool otherTable = ScrollDir & 0x02;
    switch (step) {
        case 1:
            break; // the bomb's own tile
        case 2:    // above
            TileOffsetUp(0x40);
            if ((Mem(0x05) & 0x03) == 0x03 && Mem(0x04) >= 0xC0) {
                if (otherTable) {
                    return;
                }
                TileOffsetUp(0x80);
            }
            break;
        case 3: // below
            TileOffsetDown(0x40);
            if ((Mem(0x05) & 0x03) == 0x03 && Mem(0x04) >= 0xC0) {
                if (otherTable) {
                    return;
                }
                TileOffsetDown(0x80);
            }
            break;
        case 4: // left
            TileOffsetUp(0x02);
            if ((Mem(0x04) & 0x1F) >= 0x1E) {
                if (!otherTable) {
                    return;
                }
                TileOffsetDown(0x1E);
                Mem(0x05) ^= 0x04;
            }
            break;
        case 5: // right
            TileOffsetDown(0x02);
            if ((Mem(0x04) & 0x1F) < 0x02) {
                if (!otherTable) {
                    return;
                }
                TileOffsetUp(0x1E);
                Mem(0x05) ^= 0x04;
            }
            break;
        default:
            return;
    }
    BlastTileAt04(x);
}

// $D76A
void BlastTileAt04(uint8_t x) {
    uint8_t tile = Read(Ptr04());
    if (IsBlastableRange(tile) && tile < 0xA0) {
        IsBlastTile(tile, x, false);
    }
}

// $D77F: ($04) = ($0A) + a, in room RAM.
void TileOffsetDown(uint8_t a) {
    bool carry = false;
    Mem(0x04) = Adc(a, Mem(0x0A), carry);
    Mem(0x05) = (uint8_t)((Adc(Mem(0x0B), 0x00, carry) & 0x07) | 0x60);
}

// $D78B: ($04) = ($0A) - a, in room RAM.
void TileOffsetUp(uint8_t a) {
    Mem(0x00) = a;
    bool carry = true;
    Mem(0x04) = Sbc(Mem(0x0A), a, carry);
    Mem(0x05) = (uint8_t)((Sbc(Mem(0x0B), 0x00, carry) & 0x07) | 0x60);
}

// --- $D79F GetObjCoords -----------------------------------------------------------------------------------

// ($04) = the room RAM address of the tile at the object's position.
void GetObjCoords() {
    uint8_t x = PageIndex;
    Mem(0x02) = ObjectY[x];
    Mem(0x03) = ObjectX[x];
    Mem(0x0B) = ObjectHi[x];
    MakeCartRAMPtr();
}

// --- $D7B3 UpdateElevator ------------------------------------------------------------------------------

void UpdateElevator() {
    uint8_t x = 0x20;
    PageIndex = x;
    uint8_t action = ObjAction[x];
    ChooseRoutineEffects(0xD7BD, action);
    switch (action) {
        case 1:
            ElevatorIdle();
            break;
        case 2:
            ElevatorCenter();
            break;
        case 3:
        case 8:
            ElevatorMove();
            break;
        case 4:
            ElevatorScroll();
            break;
        case 5:
        case 7:
            ElevatorNext();
            break;
        case 6:
            ElevatorChangeArea();
            break;
        case 9:
            ElevatorStop();
            break;
        default:
            break;
    }
}

void ElevatorIdle() {
    uint8_t x = 0x20;
    if (SamusOnElevator != 0) {
        uint8_t button = (Mem(0x032F) & 0x80) ? 0x08 : 0x04; // UP for an elevator going up, else DOWN
        if (button & Joy1Status) {
            uint8_t y = StopVertMovement();
            AnimDelay = y;
            SamusGravity = y;
            ObjVertSpeed[x] = y;
            ObjAction[x]++;
            ObjAction = sa_Elevator;
            SetSamusAnim(an_SamusFront);
            ObjectX = 128;
            ObjectY = 112;
        }
    }
    ShowElevator();
}

void ShowElevator() {
    if (FrameCount & 0x01) {
        DrawFrame();
    }
}

// $D80E: scrolls until the elevator is centred, then switches to vertical scrolling.
void ElevatorCenter() {
    uint8_t x = PageIndex;
    if (ScrollX == 0) {
        MirrorCntrl |= 0x08;
        ScrollDir &= 0x01;
        ObjAction[x]++;
        ShowElevator();
        return;
    }
    ObjectX = 0x80;
    if ((uint8_t)(ObjectX[x] - ScrollX) & 0x80) {
        ScrollRight();
    } else {
        ScrollLeft();
    }
    ShowElevator();
}

void ElevatorMove() {
    uint8_t x = PageIndex;
    uint8_t y;
    if (Mem((uint16_t)(0x030F + x)) & 0x80) {
        // Up.
        y = ObjectY[x];
        if (y == 0) {
            ToggleObjectHi(x);
            y = 240;
        }
        y--;
        ObjectY[x] = y;
    } else {
        // Down.
        ObjectY[x]++;
        y = ObjectY[x];
        if (y == 240) {
            ToggleObjectHi(x);
            y = 0;
            ObjectY[x] = 0;
        }
    }
    if (y == 0x83) {
        ObjAction[x]++;
    }
    ShowElevator();
}

void ElevatorScroll() {
    uint8_t x = PageIndex;
    if (ScrollY != 0) {
        ElevScrollRoom();
        return;
    }
    AnimResetIndex = 0x4E;
    AnimIndex = 0x41;
    AnimResetIndex[x] = 0x5D;
    AnimIndex[x] = 0x50;
    ObjAction[x]++;
    Timer1 = 0x40;
    ShowElevator();
}

void ElevScrollRoom() {
    uint8_t x = PageIndex;
    if (Mem((uint16_t)(0x030F + x)) & 0x80) {
        ScrollUp();
    } else {
        ScrollDown();
    }
    ShowElevator();
}

// $D8A3
void ElevatorNext() {
    uint8_t x = PageIndex;
    ObjAction[x]++;
    if (ObjAction[x] == 0x08) {
        AnimFrame[x] = 0x23;
        SetSamusAnim(an_SamusFront);
        ShowElevator();
        return;
    }
    AnimDrawObject(0x01);
}

// $D8BF: the screen is black: load the destination area (or start the ending).
void ElevatorChangeArea() {
    uint8_t x = PageIndex;
    uint8_t dest = Mem((uint16_t)(0x030F + x));
    if (dest == 0x8F) {
        MainRoutine = 0x07;
        AtEnding++;
        Mem(0x33) = 0;
        SwitchPending = 1;   // bank 0
        TitleRoutine = 0x1D; // the ending
        return;
    }
    uint8_t a = dest;
    if (!(dest & 0x80)) {
        // Going down to the area in dest.
    } else {
        a = dest == 0x84 ? 1 : 0; // back up to Brinstar or Norfair
    }
    IsEngineRunning((uint8_t)(a | 0x10));
    PalToggle ^= 0x07;
    uint8_t pal = PalToggle;
    if (InArea >= 0x12) {
        pal = 0x01;
    }
    PalDataPending = pal;
    WaitNMIPass_();
    SelectSamusPal();
    StartMusic();
    ScreenOn();
    CopyPtrs();
    DestroyEnemies();
    x = 0x20;
    PageIndex = x;
    AnimResetIndex = 0x6B;
    AnimIndex = 0x5F;
    AnimResetIndex[x] = 0x7A;
    AnimIndex[x] = 0x6E;
    ObjAction[x]++;
    Timer1 = 0x40;
}

// --- $D92C StartMusic -----------------------------------------------------------------------------------

void StartMusic() {
    uint8_t flags;
    if (ElevatorStatus == 0x06 && (Mem(0x032F) & 0x80)) {
        ItemRmMusicSts = 0x81;
        flags = 0x20;
    } else {
        flags = Read(0x95CD); // the area's music
        uint8_t y = ItemRmMusicSts;
        if (!(y & 0x80) && y != 0) {
            ItemRmMusicSts = 0x81;
            flags = 0x20; // item room music
        }
    }
    MusicInitFlag |= flags;
}

// $D94E
void ElevatorStop() {
    if (ScrollY != 0) {
        ElevScrollRoom();
        return;
    }
    ObjAction = sa_Stand;
    StopHorzMovement();
    uint8_t x = PageIndex;
    ObjAction[x] = 0x01;
    Mem((uint16_t)(0x030F + x)) ^= 0x80; // the elevator goes the other way next time
    if (!(Mem((uint16_t)(0x030F + x)) & 0x80)) {
        MirrorCntrl = ToggleScroll();
    }
    ShowElevator();
}

// $E252: returns MirrorCntrl with the mirroring toggled (the caller stores it).
uint8_t ToggleScroll() {
    ScrollDir ^= 0x03;
    return MirrorCntrl ^ 0x08;
}

// --- $D976 SamusOnElevatorOrEnemy ----------------------------------------------------------------------

void SamusOnElevatorOrEnemy() {
    SamusOnElevator = 0;
    OnFrozenEnemy = 0;
    Regs r = Original(0xF186, { 0, 0x50, 0 });
    uint8_t x = 0x50;
    for (;;) {
        bool next = true;
        if (Enstatus[x] == 0x04) { // frozen
            r = Original(0xF152, { 0, x, r.y });
            r = Original(0xF1BF, r);
            r = Original(0xF1FA, r);
            if (!r.c && SamusOnTop()) {
                OnFrozenEnemy++;
                next = false;
            }
            x = r.x;
        }
        if (!next) {
            break;
        }
        x = (uint8_t)(x - 0x10);
        if (x & 0x80) {
            break;
        }
    }
    if (ElevatorStatus == 0) {
        return;
    }
    r = Original(0xDC82, { 0, 0x20, 0 });
    if (r.c) {
        return;
    }
    if (SamusOnTop()) {
        SamusOnElevator++;
    }
}

// $D9BA: true (the original's zero flag) when Samus stands on the object just measured.
bool SamusOnTop() {
    if (!(Mem(0x10) & 0x02)) {
        if ((uint8_t)(Mem(0x11) + 1) == Mem(0x04)) {
            return true;
        }
    }
    SamusHit = (uint8_t)((SamusHit & 0x38) | Mem(0x10) | 0x40);
    return false;
}

// --- Not translated yet --------------------------------------------------------------------------------

void WaitNMIPass_() {
    WaitNMIPass();
}

} // namespace Game
