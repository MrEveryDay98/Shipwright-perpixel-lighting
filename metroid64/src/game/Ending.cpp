// Bank 0: the ending (TitleRoutine $1D) and its credits. Routine names and addresses follow the
// community disassembly (Bank00.asm, $9A07-$9F80). The screens, Samus's sprites, the end messages, the
// credits and the palettes are read from bank 0 of the ROM.

#include "game/Title.h"

#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

namespace {
constexpr uint16_t kEndGameTbl = 0x9AC5;      // jump table after the JSR ChooseRoutine at $9AC2
constexpr uint16_t kEndScreenPPUStr = 0xA052; // Samus on the planet's surface
constexpr uint16_t kPalChangeTable = 0x9B83;
constexpr uint16_t kSamusHeadSprtTble = 0x9CFA;
constexpr uint16_t kWavePntrTable = 0x9D12;
constexpr uint16_t kEndSamusAddrTbl = 0x9D5A;
constexpr uint16_t kEndStarDataTable = 0x9EB8;
constexpr uint16_t kEndGmPalPntrTbl = 0x9F81;
constexpr uint16_t kEndMsgStrngTbl0 = 0xA1BA;
constexpr uint16_t kEndMsgStrngTbl1 = 0xA265;
constexpr uint16_t kCreditsPntrTbl = 0xA291;
} // namespace

static uint8_t& Sprite(uint8_t x) {
    return Mem((uint16_t)(0x0200 + x));
}

// Writes the PPU string whose pointer is at table + y.
static void PrepPPUProcessAt(uint16_t entry) {
    PrepPPUProcess_(Read(entry), Read((uint16_t)(entry + 1)));
}

static void LoadCredits();

// $9A07: called by the NMI every frame.
void NMIScreenWrite() {
    if (TitleRoutine < 0x1D) {
        return;
    }
    LoadCredits();
    uint8_t a = EndMsgWrite;
    if (a != 0 && a < 0x05) {
        PrepPPUProcessAt((uint16_t)(kEndMsgStrngTbl0 - 2 + (uint8_t)(a << 1)));
    }
    a = HideShowEndMsg;
    if (a != 0 && a < 0x05) {
        PrepPPUProcessAt((uint16_t)(kEndMsgStrngTbl1 - 2 + (uint8_t)(a << 1)));
    }
}

// $9A39: START after the credits starts over with the gear Samus ended with.
void Restart() {
    if (!(Joy1Status & 0x10)) {
        return;
    }
    for (int y = 0x11; y >= 0; y--) {
        PasswordByte00[y] = 0;
    }
    for (int y = 0; y < 0x100; y++) {
        UnqItmHist[y] = 0;
    }
    if (SamusGear & 0x10) {
        PasswordByte00 = 0x01; // Maru Mari
    }
    if (SamusGear & 0x01) {
        PasswordByte00 |= 0x40; // bombs
    }
    if (SamusGear & 0x20) {
        PasswordByte01 = 0x08; // Varia
    }
    if (SamusGear & 0x02) {
        PasswordByte03 = 0x01; // high jump
    }
    if (SamusGear & 0x10) {
        PasswordByte03 |= 0x04; // screw attack, but tested with the Maru Mari bit (a bug of the original)
    }
    PasswordByte09 = SamusGear;
    PasswordByte08 = JustInBailey != 0 ? 0x80 : 0x00;
    InitializeGame();
}

static void LoadEndStarSprites();
static void LoadEndGFX();
static void ShowEndSamus();
static void EndSamusFlash();
static void SamusWave();
static void EndFadeOut();
static void RollCredits();

// $9AA7
void EndGame() {
    LoadEndStarSprites();
    if (IsCredits == 0 && (FrameCount & 0x0F) == 0) {
        // The star palettes cycle through 1-8.
        PalDataPending++;
        if (PalDataPending == 0x09) {
            PalDataPending = 0x01;
        }
    }
    uint8_t routine = GenByte33;
    ChooseRoutineEffects(kEndGameTbl, routine);
    switch (routine) {
        case 0:
            LoadEndGFX();
            break;
        case 1:
            ShowEndSamus();
            break;
        case 2:
            EndSamusFlash();
            break;
        case 3:
            SamusWave();
            break;
        case 4:
            EndFadeOut();
            break;
        case 5:
            RollCredits();
            break;
        case 6:
            Restart();
            break;
        case 7:
            break; // ExitSub
        default:
            break; // the original would jump through whatever follows the table
    }
}

// $9AD5
static void LoadEndGFX() {
    ClearAll();
    InitEndGFX();
    uint8_t a = JustInBailey != 0 ? 0x04 : 0x00;
    EndingType = a;
    SpritePtrIndex = (uint8_t)(a << 1);
    PrepPPUProcess_(kEndScreenPPUStr & 0xFF, kEndScreenPPUStr >> 8);
    NmiOn();
    MultiSFXFlag = 0x20; // end music
    Timer3 = 0x60;       // 960 frames
    SpriteByteCntr = 0x36;
    SpriteAttribByte = 0;
    ColorCntIndex = 0;
    IsCredits = 0;
    EndMsgWrite = 0;
    HideShowEndMsg = 0;
    CreditPageNumber = 0;
    PalDataPending = 0x01;
    ClrChangeCounter = 0x08;
    GenByte33++;
    ScreenOn();
}

static void LoadEndSamSprts();
static void LoadWaveSprites();

// $9B1C
static void ShowEndSamus() {
    LoadEndSamSprts();
    uint8_t a = Timer3;
    if (a == 0) {
        GenByte33++;
        return;
    }
    if (a == 0x50) {
        EndMsgWrite++;
        return;
    }
    if (a == 0x01) {
        HideShowEndMsg++;
    }
}

// $9B34
static void EndSamusFlash() {
    if ((FrameCount & 0x1F) == 0) {
        ColorCntIndex++;
        uint8_t a = ColorCntIndex;
        if (a == 0x08) {
            // Half way through: the ending is chosen and the password made.
            ChooseEnding();
            CalculatePassword();
            SpritePtrIndex = (uint8_t)(EndingType << 1);
            a = 0x36;
            SpriteByteCntr = a;
        }
        if (a == 0x10) {
            Timer3 = 0x10;
            SpriteAttribByte = EndingType >= 0x04 ? 1 : 0;
            GenByte33++;
            EraseAllSprites();
            return;
        }
    }
    if (--ClrChangeCounter == 0) {
        ClrChangeCounter = Read((uint16_t)(kPalChangeTable + ColorCntIndex));
        SpriteAttribByte++;
        if (SpriteAttribByte == 0x03) {
            SpriteAttribByte = 0;
        }
    }
    LoadEndSamSprts();
}

// $9B93
static void SamusWave() {
    if (Timer3 == 0) {
        Timer3 = 0x10;
        PalDataPending = 0x08;
        GenByte33++;
        return;
    }
    uint8_t a = EndingType;
    if (a < 0x04) {
        LoadEndSamSprts();
        return;
    }
    WaveSpritePtr = (uint8_t)((a - 0x04) << 2); // jumpsuit 0, bikini 4
    if (FrameCount & 0x08) {
        WaveSpritePtr += 2; // hand down
    }
    WaveSpriteCntr = 0x10;
    SpriteByteCntr = 0x2D;
    LoadWaveSprites();
}

// $9BCD
static void EndFadeOut() {
    if (Timer3 == 0) {
        if (IsCredits == 0) {
            PalDataPending = 0x08;
            IsCredits++;
        }
        if ((FrameCount & 0x07) == 0) {
            PalDataPending++;
            if (PalDataPending == 0x0C) {
                Timer3 = 0x10;
                GenByte33++;
            }
        }
    }
    if (EndingType >= 0x04) {
        LoadWaveSprites();
    } else {
        LoadEndSamSprts();
    }
}

// $9BFC
static void RollCredits() {
    uint8_t a = Timer3;
    if (a != 0) {
        if (a != 0x02) {
            return;
        }
        ScreenOff();
        ClearNameTable0();
        EraseAllSprites();
        PalDataPending = 0x0D;
        ScreenOn();
        WaitNMIPass(); // WaitNMIPass_
        return;
    }
    a = CreditPageNumber;
    if (a == 0) {
        CreditPageNumber++;
    }
    if (a == 0x06 && ScrollY >= 0x88) {
        GenByte33++;
        return;
    }
    if ((FrameCount & 0x03) != 0) {
        return;
    }
    ScrollY++; // scroll up one line every 4 frames
    if (ScrollY == 0xF0) {
        CreditPageNumber++;
        ScrollY = 0;
        PPUCNT0ZP ^= 0x02; // the other name table
    }
}

// $9C45: writes the credits into the lines that just scrolled out of view (the top half of the name
// table at ScrollY 0-3, its bottom half at 128-131).
static void LoadCredits() {
    uint8_t y = CreditPageNumber;
    if (y == 0 || y >= 0x07) {
        return;
    }
    uint8_t x = 0;
    uint8_t a = ScrollY;
    if (a & 0x80) {
        x = 1;
        a -= 0x80;
    }
    if (a >= 0x04) {
        return;
    }
    Mem(0x01) = a;
    y--;
    if (x == 0) {
        y--;
        if (y & 0x80) {
            return;
        }
        a = (uint8_t)((y << 3) + 0x04);
    } else {
        a = (uint8_t)(y << 3);
    }
    a = (uint8_t)(a + Mem(0x01));
    PrepPPUProcessAt((uint16_t)(kCreditsPntrTbl + (uint8_t)(a << 1)));
}

// $9C7F: the waving hand of suitless Samus, then the rest of her.
static void LoadWaveSprites() {
    uint8_t x = WaveSpritePtr;
    GenByte00 = Read((uint16_t)(kWavePntrTable + x));
    Mem(0x01) = Read((uint16_t)(kWavePntrTable + x + 1));
    uint16_t src = GenWord00.Word();
    x = 0x20;
    uint8_t y = 0;
    do {
        Sprite(x) = Read((uint16_t)(src + y));
        x++;
        y++;
    } while (y != WaveSpriteCntr);
    LoadEndSamSprts();
}

// $9C9A: Samus's sprites from [y][pattern][x] triples; a pattern with its MSB set is mirrored.
static void LoadEndSamSprts() {
    uint8_t x = 0x30;
    uint8_t y = SpritePtrIndex;
    GenByte00 = Read((uint16_t)(kEndSamusAddrTbl + y));
    Mem(0x01) = Read((uint16_t)(kEndSamusAddrTbl + y + 1));
    uint16_t src = GenWord00.Word();
    y = 0;
    do {
        Sprite(x) = Read((uint16_t)(src + y));
        x++;
        y++;
        uint8_t a = Read((uint16_t)(src + y));
        if (a & 0x80) {
            Sprite(x) = a & 0x7F;
            a = SpriteAttribByte ^ 0x40;
            if (a == 0) {
                // The original's BNE falls through here: the pattern is overwritten.
                Sprite(x) = a;
                a = SpriteAttribByte;
            }
        } else {
            Sprite(x) = a;
            a = SpriteAttribByte;
        }
        x++;
        Sprite(x) = a;
        y++;
        x++;
        Sprite(x) = Read((uint16_t)(src + y));
        y++;
        x++;
    } while (y != SpriteByteCntr);
    if (GenByte33 >= 0x02 && ColorCntIndex >= 0x08 && EndingType == 0x03) {
        // Helmet off: Samus's head doesn't flash.
        for (uint8_t i = 0; i != 0x18; i++) {
            Sprite(i) = Read((uint16_t)(kSamusHeadSprtTble + i));
        }
    }
}

// $9EAA: the ending's stars. The table is 156 bytes long, so the last ones land past sprite RAM, in
// $0300-$030B.
static void LoadEndStarSprites() {
    for (int y = 0; y < 0x9C; y++) {
        Mem((uint16_t)(0x0270 + y)) = Read((uint16_t)(kEndStarDataTable + y));
    }
}

// $9F54: called by the NMI instead of the usual palette writer during the ending.
void EndGamePalWrite() {
    uint8_t a = PalDataPending;
    if (a == 0 || a == 0x0C) {
        return;
    }
    if (a == 0x0D) {
        PalDataPending = 0; // start the cycle over
    }
    PrepPPUProcessAt((uint16_t)(kEndGmPalPntrTbl - 2 + (uint8_t)(a << 1)));
    Hw::PpuAddr(0x3F);
    Hw::PpuAddr(0x00);
    Hw::PpuAddr(0x00);
    Hw::PpuAddr(0x00);
}

} // namespace Game
