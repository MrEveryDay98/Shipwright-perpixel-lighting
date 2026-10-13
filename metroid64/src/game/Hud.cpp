// Bank 7: Samus's status bar ($E0C1-$E1E0).

#include "game/Hud.h"

#include "game/Memory.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

// --- $E0C1 DisplayBar ------------------------------------------------------------------------------

// Sprite RAM index arithmetic is 8-bit (X) but the stores are absolute,X, so the last sprites of a full
// page write a few bytes past $02FF, as in the original.
void DisplayBar() {
    static const uint16_t kDataDisplayTbl = 0xE1B9; // 10 sprites: energy digits, missile digits and icon, "EN.."
    uint8_t start = SpritePagePos;
    uint8_t x = start;
    for (uint8_t y = 0; y != 0x28; y++, x++) {
        SpriteRAM[x] = Read((uint16_t)(kDataDisplayTbl + y));
    }
    SpritePagePos = x;
    x = start;
    x = SPRWriteDigit(x, HealthHi & 0x0F);
    x = SPRWriteDigit(x, HealthLo >> 4);
    if ((uint8_t)(EndTimerHi + 1) == 0) {
        bool erase = true;
        if (MaxMissiles != 0) {
            HexToDec(MissileCount);
            x = SPRWriteDigit(x, Mem(0x02));
            x = SPRWriteDigit(x, Mem(0x01));
            x = SPRWriteDigit(x, Mem(0x00));
            erase = x == 0; // "branch always" falls through when X wrapped to 0
        }
        if (erase && x < 0xF4) {
            // No missile containers: blank out the missile icon.
            SpriteRAM[x + 0x0D] = 0xFF;
            if (x < 0xF0) {
                SpriteRAM[x + 0x11] = 0xFF;
            }
        }
    } else {
        // Escape sequence: the timer replaces the missile count, and "TIME" the missile icon.
        x = SPRWriteDigit(x, EndTimerHi >> 4);
        x = SPRWriteDigit(x, EndTimerHi & 0x0F);
        x = SPRWriteDigit(x, EndTimerLo >> 4);
        SpriteRAM[x + 1] = 0x58;
        SpriteRAM[x + 2]++;
        if (x < 0xFC) {
            SpriteRAM[x + 5] = 0x59;
            SpriteRAM[x + 6]++;
        }
    }

    x = SpritePagePos;
    if (TankCount == 0) {
        return;
    }
    // Energy tanks, right to left: full ones first, then empty ones.
    Mem(0x03) = TankCount;
    Mem(0x00) = 0x40;
    uint8_t tile = 0x6F;
    Mem(0x01) = HealthHi >> 4;
    if (Mem(0x01) == 0) {
        tile--;
    }
    do {
        x = AddOneTank(x, tile);
        if (--Mem(0x01) == 0) {
            tile--;
        }
    } while (--Mem(0x03) != 0);
    SpritePagePos = x;
}

// $E173: the digit's tile goes into the sprite at X; returns X of the next sprite.
uint8_t SPRWriteDigit(uint8_t x, uint8_t digit) {
    SpriteRAM[x + 1] = (uint8_t)(digit | 0xA0);
    return (uint8_t)(x + 4);
}

// $E17B
uint8_t AddOneTank(uint8_t x, uint8_t tile) {
    SpriteRAM[x] = 0x17;
    SpriteRAM[x + 1] = tile;
    SpriteRAM[x + 2] = 0x01;
    SpriteRAM[x + 3] = Mem(0x00);
    Mem(0x00) = (uint8_t)(Mem(0x00) - 0x0A);
    return (uint8_t)(x + 4);
}

// $E198: hundreds in $02, tens in $01, ones in $00 (also returned).
uint8_t HexToDec(uint8_t a) {
    Mem(0x0A) = 100;
    Mem(0x02) = GetDigit(a);
    Mem(0x0A) = 10;
    Mem(0x01) = GetDigit(a);
    Mem(0x00) = a;
    return a;
}

// $E1AD: how many times $0A goes into A; A keeps the remainder.
uint8_t GetDigit(uint8_t& a) {
    uint8_t y = 0;
    while (a >= Mem(0x0A)) {
        a = (uint8_t)(a - Mem(0x0A));
        y++;
    }
    return y;
}

} // namespace Game
