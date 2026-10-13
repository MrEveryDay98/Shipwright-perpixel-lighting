// Bank 7: Kraid and Ridley statues, power-up items and the unique item history ($D9D4-$DC8A).

#include "game/Items.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Objects.h"
#include "game/Original.h"
#include "game/Projectiles.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;
using namespace Const;

// --- $D9D4 UpdateStatues ---------------------------------------------------------------------------

void UpdateStatues() {
    PageIndex = 0x60;
    uint8_t y = Mem(0x0360);
    if (y == 0) {
        return; // no statues in this room
    }
    bool carry;
    if (y == 1) {
        DrawStatueTiles(0);
        carry = DrawStatueTiles(1);
        if (!carry) {
            Mem(0x0360)++;
        }
    }
    if (Mem(0x0360) == 0x02) {
        carry = true; // from the comparison
        if (KraidStatueStat & 0x80) {
            carry = DrawStatueTiles(2);
        }
        if (RidlyStatueStat & 0x80) {
            carry = DrawStatueTiles(3);
        }
        if (!carry) {
            Mem(0x0360)++;
        }
    }
    UpdateStatue(0x60);
    UpdateStatue(0x61);
    CheckStatuesRaised();
}

// $DA1A
void UpdateStatue(uint8_t x) {
    StatueRise(x);
    StatueSound(x);
    static const uint16_t kStatueFrames = 0xDA3B;
    Mem(0x0363) = Read((uint16_t)(kStatueFrames + (x & 0x01)));
    uint8_t status = Mem((uint16_t)(0x681B + x));
    if (status != 0 && !(status & 0x80) && !(FrameCount & 0x01)) {
        return; // blinking: only shown on odd frames
    }
    DrawFrame();
}

// $DA3D: raises the statue a step at a time and redraws the tiles under it.
void StatueRise(uint8_t x) {
    if (Mem((uint16_t)(0x0304 + x)) & 0x80) {
        return;
    }
    Mem((uint16_t)(0x0304 + x)) = 0x01;
    if ((Mem((uint16_t)(0x030F + x)) & 0x0F) == 0) {
        return;
    }
    Mem((uint16_t)(0x0304 + x))++;
    Mem((uint16_t)(0x030F + x))--;
    if ((Mem((uint16_t)(0x030F + x)) & 0x0F) != 0) {
        return;
    }
    uint8_t v = Mem((uint16_t)(0x0304 + x)) | 0x80;
    Mem((uint16_t)(0x0304 + x)) = v;
    Mem((uint16_t)(0x681B + x)) = v;
    Mem((uint16_t)(0x0304 + x))++;
    uint8_t which = x & 0x01;
    DrawStatueTiles(which);
    DrawStatueTiles((uint8_t)(which + 2));
}

// $DA7C
void StatueSound(uint8_t x) {
    Mem(0x036D) = Mem((uint16_t)(0x030F + x));
    static const uint16_t kStatueSides = 0xDA39;
    Mem(0x036E) = Read((uint16_t)(kStatueSides + (x & 0x01)));
    uint8_t status = Mem((uint16_t)(0x681B + x));
    if (status != 0 && !(status & 0x80) && Mem((uint16_t)(0x0304 + x)) == 0x01 && Mem((uint16_t)(0x0306 + x)) != 0) {
        Mem((uint16_t)(0x030F + x))--;
        Mem(0x0683) |= 0x10; // statue raise sound
    }
    Mem((uint16_t)(0x0306 + x)) = 0;
}

// $DAB0: queues the statue tiles at position y. Returns the carry from DrawTileBlast (set when it couldn't
// be queued this frame).
bool DrawStatueTiles(uint8_t y) {
    static const uint16_t kTable0E = 0xDAD2;
    static const uint16_t kTable1B = 0xDAD6;
    Mem(0x05C8) = Read((uint16_t)(kTable0E + y));
    Mem(0x05C9) = (uint8_t)((Mem(0x036C) << 2) | Read((uint16_t)(kTable1B + y)));
    Mem(0x05C3) = 0x09;
    PageIndex = 0xC0;
    bool carry = Original(0xFEDC).c; // DrawTileBlast
    PageIndex = 0x60;
    return carry;
}

// $DADA: once both statues are up, the bridge to Tourian appears.
void CheckStatuesRaised() {
    if ((Mem(0x54) & 0x80) || DoorStatus != 0) {
        return;
    }
    uint8_t a = KraidStatueStat & RidlyStatueStat;
    if (!(a & 0x80)) {
        return;
    }
    Mem(0x54) = a;
    uint8_t x = 0x70;
    for (uint8_t y = 0x08; y != 0; y--) {
        Mem((uint16_t)(0x0500 + x)) = 0x03;
        Mem((uint16_t)(0x0507 + x)) = (uint8_t)(y << 1);
        TileType[x] = 0x04;
        TileWRAMHi[x] = (uint8_t)((Mem(0x036C) << 2) | 0x62);
        bool carry = false;
        TileWRAMLo[x] = Adc((uint8_t)(y << 1), 0x08, carry);
        x = (uint8_t)(x - 0x10);
    }
}

// --- $DB2F MakeBitMask ---------------------------------------------------------------------------------

uint8_t MakeBitMask(uint8_t y) {
    return (uint8_t)(1 << (y & 0x07));
}

// --- $DB37 UpdateItems -------------------------------------------------------------------------------------

void UpdateItems() {
    PageIndex = 0x40; // power-up object RAM at $0340
    CheckOneItem(0x00);
    CheckOneItem(0x08);
}

void CheckOneItem(uint8_t x) {
    ItemIndex = x;
    if ((uint8_t)(PowerUpType[x] + 1) == 0) {
        return; // no item in this slot
    }
    PowerUpY = PowerUpYCoord[x];
    PowerUpX = PowerUpXCoord[x];
    PowerUpHi = PowerUpNameTable[x];
    GetObjCoords();
    x = ItemIndex;
    if (Read((uint16_t)(Mem(0x04) | (Mem(0x05) << 8))) < 0xA0) {
        return; // not shown while covered by a solid tile
    }
    PowerUpAnimFrame = (uint8_t)((PowerUpType[x] & 0x0F) | 0x50);
    ObjectCntrl = (uint8_t)(((FrameCount >> 1) & 0x03) | 0x80); // flashing colours
    uint8_t spritePos = SpritePagePos;
    DrawFrame();
    if (spritePos == SpritePagePos) {
        return; // not drawn
    }
    uint8_t type = PowerUpType[ItemIndex];
    // Beam orbs get their colour: ice beam 1, wave and long beam 0.
    bool beam = false;
    if (type == 0x07) {
        SpriteRAM[spritePos + 6] = 0x01;
        beam = true;
    } else if (type == 0x06 || type == 0x02) {
        SpriteRAM[spritePos + 6] = 0x00;
        beam = true;
    }
    uint8_t a = beam ? 0xFF : type;
    if (AreObjectsTouching(0x00, 0x40)) {
        return; // carry set: Samus isn't touching it
    }
    PowerUpMusic();
    x = ItemIndex;
    if ((uint8_t)(a + 1) != 0) {
        // Not a beam: remember the item so it doesn't come back.
        Mem(0x08) = PowerUpNameTable[x];
        Mem(0x09) = PowerUpType[x];
        GetItemXYPos();
    }
    uint8_t item = PowerUpType[x];
    if (item >= 0x08) {
        MissileEnergyPickup(item);
    } else {
        if (item >= 0x06) {
            SamusGear &= 0x3F; // wave and ice beam replace each other
        }
        SamusGear |= MakeBitMask(item);
    }
    PowerUpDelay = 0xFF; // pause while the jingle plays
    PowerUpType[x] = 0xFF;
    if (ItemRmMusicSts != 0) {
        ItemRmMusicSts = 0x01; // restart the item room music afterwards
    }
    SelectSamusPal();
}

// $DBF7: missiles (type 9) add 5; an energy tank (type 8) adds a tank and fills energy.
void MissileEnergyPickup(uint8_t item) {
    if (item != 0x08) {
        AddToMaxMissiles(0x05);
        return;
    }
    if (TankCount != 0x06) {
        TankCount++;
    }
    HealthHi = (uint8_t)((TankCount << 4) | 0x09);
    HealthLo = 0x99;
}

// $DC1C: the map position of the item ($08 its name table, $09 its type) for the item history.
void GetItemXYPos() {
    Mem(0x07) = MapPosX;
    Mem(0x06) = MapPosY;
    bool carry = ScrollDir & 0x01;
    bool vertical = (ScrollDir >> 1) == 0;
    if (!vertical) {
        if (carry && ScrollX != 0) {
            Mem(0x07)--;
        }
    } else {
        if (carry && ScrollY != 0) {
            Mem(0x06)--;
        }
    }
    uint8_t other = (PPUCNT0ZP ^ Mem(0x08)) & 0x01;
    if (vertical) {
        Mem(0x06) = (uint8_t)(Mem(0x06) + other);
    } else {
        Mem(0x07) = (uint8_t)(Mem(0x07) + other);
    }
    AddItemToHistory();
}

void AddItemToHistory() {
    CreateItemID();
    uint8_t y = NumUniqueItems;
    UnqItmHist[y] = Mem(0x06);
    UnqItmHist[y + 1] = Mem(0x07);
    NumUniqueItems = (uint8_t)(y + 2);
}

// $DC67: packs map x/y and the item type into $06/$07.
void CreateItemID() {
    Mem(0x06) = (uint8_t)((Mem(0x07) << 5) | Mem(0x06));
    Mem(0x07) >>= 3;
    Mem(0x07) = (uint8_t)((Mem(0x09) << 2) | Mem(0x07));
}

// $DC7F: returns the carry: clear when objects x and y overlap.
bool AreObjectsTouching(uint8_t x, uint8_t y) {
    return Original(0xDC7F, { 0, x, y }).c;
}

} // namespace Game
