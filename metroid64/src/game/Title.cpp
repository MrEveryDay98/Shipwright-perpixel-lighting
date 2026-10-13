// Bank 0: title screen and intro, START/CONTINUE, password entry and display, game over. Routine names
// and addresses follow the community disassembly (nmikstas/metroid-disassembly, Bank00.asm).
//
// Data tables (screens, sprites, palettes, the sparkle paths...) are read from bank 0 of the ROM, which
// is mapped at $8000 while these routines run.

#include "game/Title.h"

#include "game/Constants.h"
#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;
using namespace Const;

// Bank 0 tables used here.
namespace {
constexpr uint16_t kTitleRoutineTbl = 0x802A; // jump table after the JSR ChooseRoutine at $8027
constexpr uint16_t kRamValueTbl = 0x80C8;
constexpr uint16_t kIntroBkgPPUStr = 0x82F4; // IntroAttrib0_0 and the rest of the intro screen
constexpr uint16_t kInitSprklDatTbl = 0x87C4;
constexpr uint16_t kSprt0and4InitTbl = 0x88BE;
constexpr uint16_t kCrsSprtIndexTbl = 0x89AA;
constexpr uint16_t kSprklAddrTbl = 0x89AF;
constexpr uint16_t kCrsSprtDataTbl = 0x8A4B;
constexpr uint16_t kPalSelectTbl = 0x8A9A;
constexpr uint16_t kScrnFlshPalTbl = 0x8ABD;
constexpr uint16_t kIntroStarPntr = 0x8AFF;
constexpr uint16_t kFadeOutPalData = 0x8B6D;
constexpr uint16_t kErrorMsg = 0x8759;      // "ERROR TRY AGAIN"
constexpr uint16_t kErrorMsgBlank = 0x8768; // fifteen blanks covering it
constexpr uint16_t kStartContTbl = 0x9118;
constexpr uint16_t kCharSelectYTbl = 0x92B6;
constexpr uint16_t kCharSelectXTbl = 0x92BB;
constexpr uint16_t kCursorPosTbl = 0x92C8;
constexpr uint16_t kRestartYPosTbl = 0x9325;
constexpr uint16_t kRestartXPosTbl = 0x9328;
constexpr uint16_t kPassWordStr = 0x937F; // "PASS WORD"
constexpr uint16_t kGameOverStr = 0x93B9; // "GAME OVER"
constexpr uint16_t kPasswordRowsTbl = 0x943F;
constexpr uint16_t kStartContStr = 0x9984; // "START" / "CONTINUE"
constexpr uint16_t kPasswordRowTbl = 0x9998;
constexpr uint16_t kPasswordRow0 = 0x99A2;
constexpr uint16_t kPasswordPleaseStr = 0x99E3; // "PASS WORD PLEASE"
constexpr uint16_t kWorldMap = 0xA53E;
constexpr uint16_t kBankTable = 0xCA30; // bank 7
} // namespace

// The sprite RAM byte at $0200 + x (x wraps like the original's 8-bit index).
static uint8_t& Sprite(uint8_t x) {
    return Mem((uint16_t)(0x0200 + x));
}

// $C20E PrepPPUProcess_ (bank 7) and $9449 PreparePPUProcess: writes the PPU string at hi:lo now.
void PrepPPUProcess_(uint8_t lo, uint8_t hi) {
    GenByte00 = lo;
    Mem(0x01) = hi;
    ProcessPPUStr();
}

// $C36B WritePPUByte: appends a byte to the PPU string. False when the string is full: the original
// then ends the string and returns from the routine that called it.
static bool WritePPUByte(uint8_t& x, uint8_t value) {
    PPUDataString[x] = value;
    return NextPPUByte(x);
}

// --- $8027 Title routines -------------------------------------------------------------------------

static void ClearSpareMem();
static void IncTitleRoutine();
static void InitAfterReset();
static void DrawIntroBkg();
static void FadeInDelay();
static void FlashEffect();
static void METROIDFadeIn();
static void LoadFlashTimer();
static void METROIDSparkle();
static void METROIDFadeOut();
static void Crosshairs();
static void MoreCrosshairs();
static void ChangeIntroNT();
static void MessageFadeIn();
static void MessageFadeOut();
static void DelayIntroReplay();
static void PrepIntroRestart();
static void TitleScreenOff();
static void StartContScrn();
static void ChooseStartCont();
static void LoadPswdScreen();
static void EnterPassword();
static void DisplayPassword();
static void WaitForSTART();
static void TitleGameOver();
static void LoadStarSprites();

void TitleMainRoutine() {
    uint8_t routine = TitleRoutine;
    if (routine < 0x15) { // intro running
        if (Joy1Change & BTN_START) {
            SpareMemD1 = 0;
            SpareMemBB = 0;
            SpareMemB7 = 0;
            SpareMemB8 = 0;
            PPUCNT0ZP &= 0xFC; // name table 0
            routine = 0x1B;    // START/CONTINUE screen
            TitleRoutine = routine;
        } else {
            RemIntroSprts();
            routine = TitleRoutine;
        }
    }
    ChooseRoutineEffects(kTitleRoutineTbl, routine);
    switch (routine) {
        case 0x00:
            InitAfterReset();
            break;
        case 0x01:
            DrawIntroBkg();
            break;
        case 0x02:
            FadeInDelay();
            break;
        case 0x03:
            METROIDFadeIn();
            break;
        case 0x04:
            LoadFlashTimer();
            break;
        case 0x05:
            FlashEffect();
            break;
        case 0x06:
            METROIDSparkle();
            break;
        case 0x07:
            METROIDFadeOut();
            break;
        case 0x08:
            Crosshairs();
            break;
        case 0x09:
            MoreCrosshairs();
            break;
        case 0x0A:
        case 0x0B:
            IncTitleRoutine();
            break;
        case 0x0C:
            ChangeIntroNT();
            break;
        case 0x0D:
            MessageFadeIn();
            break;
        case 0x0E:
            MessageFadeOut();
            break;
        case 0x0F:
            DelayIntroReplay();
            break;
        case 0x10:
            ClearSpareMem();
            break;
        case 0x11:
            PrepIntroRestart();
            break;
        case 0x12:
            TitleScreenOff();
            break;
        case 0x13:
        case 0x14:
            break; // TitleRoutineRtn
        case 0x15:
        case 0x1B:
            StartContScrn();
            break;
        case 0x16:
            ChooseStartCont();
            break;
        case 0x17:
            LoadPswdScreen();
            break;
        case 0x18:
            EnterPassword();
            break;
        case 0x19:
            DisplayPassword();
            break;
        case 0x1A:
            WaitForSTART();
            break;
        case 0x1C:
            TitleGameOver();
            break;
        case 0x1D:
            EndGame();
            break;
        case 0x1E:
            // The table's last entry is bank 7's SetTimer, entered with A = $1E and X = $FF (left by
            // UpdateTimer). Never used.
            SetTimer(0x1E, 0xFF);
            break;
        default:
            break; // the original would jump through whatever follows the table
    }
}

// $8068
static void ClearSpareMem() {
    SpareMemCB = 0;
    SpareMemC9 = 0;
    IncTitleRoutine();
}

// $806E
static void IncTitleRoutine() {
    TitleRoutine++;
}

// $8071: first routine after reset. Fills the room RAM with blank tiles and attributes.
static void InitAfterReset() {
    SpareMemCF = 2;
    SpareMemCC = 2;
    SpareMemCE = 1;
    SpareMemD1 = 1;
    SpareMemD0 = 0;
    SpareMemCD = 0;
    SpareMemD3 = 0;
    NARPASSWORD = 0;
    SpareMemCB = 0;
    SpareMemC9 = 0;
    IntroMusRstrt = 2; // the intro plays twice before its music restarts
    SpareMemB7 = 0;
    SpareMemB8 = 0;
    PalDataIndex = 0;
    ScrnFlashPalInd = 0;
    IntroStarOffset = 0;
    FadeDataIndex = 0;

    // $6000-$62FF = $00, $6300-$633F = $C0, $6340-$63FF = $C4, and the same for $6400-$67FF.
    GenPtr00LB = 0;
    for (uint8_t x = 0x60; x != 0x68; x++) {
        GenPtr00UB = x;
        uint8_t y = (uint8_t)((x & 0x03) << 1);
        GenByte02 = y;
        uint8_t a = Read((uint16_t)(kRamValueTbl + y));
        uint16_t page = (uint16_t)(x << 8);
        for (int i = 0; i < 0x100; i++) {
            if (i == 0x40) {
                a = Read((uint16_t)(kRamValueTbl + 1 + GenByte02));
            }
            Mem((uint16_t)(page + i)) = a;
        }
    }
    TitleRoutine++;
    LoadStarSprites();
}

// $80D0
static void DrawIntroBkg() {
    ABStatus = MUS_INTRO;
    MultiSFXFlag = MUS_INTRO; // intro music
    ScreenOff();
    ClearNameTables();
    PrepPPUProcess_(kIntroBkgPPUStr & 0xFF, kIntroBkgPPUStr >> 8);
    PalDataPending = 1;
    SpareMemC5 = 1;
    PPUCNT0ZP &= 0xFC; // name table 0
    TitleRoutine++;
    SpareMemD7 = 0;
    ScreenOn();
}

// $80F9
static void FadeInDelay() {
    PPUCNT0ZP &= 0xFE;
    Timer3 = 0x08; // 80 frames
    PalDataIndex = 0x08 >> 1;
    TitleRoutine++;
}

// $8A8C: next palette of the METROID fade in. Returns the Z flag it leaves: set at the end of the list.
static bool LoadPalData() {
    uint8_t a = Read((uint16_t)(kPalSelectTbl + PalDataIndex));
    if (a == 0xFF) {
        return true;
    }
    PalDataPending = a;
    PalDataIndex++;
    return PalDataIndex == 0;
}

static void LoadSparkleData();

// $8109: the METROID flash.
static void FlashEffect() {
    if (FrameCount & 0x03) {
        return;
    }
    PalDataIndex &= 0x03;
    LoadPalData();
    if (Timer3 != 0 || PalDataIndex != 0x04) {
        return;
    }
    TitleRoutine++;
    LoadSparkleData();
    Timer3 = 0x18; // 240 frames
}

// $812C
static void METROIDFadeIn() {
    if (Timer3 != 0 || (FrameCount & 0x0F) != 0) {
        return;
    }
    if (!LoadPalData()) {
        return;
    }
    Timer3 = 0x20; // 320 frames before the flash
    TitleRoutine++;
}

// $8142
static void LoadFlashTimer() {
    if (Timer3 != 0) {
        return;
    }
    Timer3 = 0x08;
    TitleRoutine++;
}

static void UpdtSparkleSprts();

// $814D
static void METROIDSparkle() {
    if (Timer3 != 0) {
        return;
    }
    if ((IntroSpr0Comp & IntroSpr1Comp) == 0x01) {
        TitleRoutine++;
        return;
    }
    UpdtSparkleSprts();
}

static void LoadInitSprtDat();
static void DoFadeOut();

// $8163
static void METROIDFadeOut() {
    if (FrameCount & 0x07) {
        return;
    }
    if (FadeDataIndex == 0x04) {
        LoadInitSprtDat();
        Timer3 = 0x08;
        First4SlowCntr = 0x08;
        ScndCrshrSprts = 0;
        TitleRoutine++;
    }
    DoFadeOut();
}

static void FlashIntroScreen();
static void DrwCrshrsSprts();
static void DrawCrossSprites();

// $8182
static void Crosshairs() {
    if (FlashScreen != 0) {
        FlashIntroScreen();
    }
    if (Timer3 != 0) {
        return;
    }
    uint8_t a = IntroSpr0Comp & IntroSpr1Comp & IntroSpr2Comp & IntroSpr3Comp;
    if (a != 0) {
        // The first four sprites are in place.
        a = 0x01;
        if (ScndCrshrSprts != 0x01) {
            ScndCrshrSprts++;
            DrawCross = a;
            FlashScreen = a;
            a = 0x00;
            CrossDataIndex = a;
        }
        a &= IntroSpr4Comp & IntroSpr5Comp & IntroSpr6Comp & IntroSpr7Comp;
        if (a != 0) {
            // All eight are in place.
            DrawCross = 0x01;
            FlashScreen = 0x01;
            LoadStarSprites();
            CrossDataIndex = 0;
            TitleRoutine++;
            DrawCrossSprites();
            return;
        }
    }
    DrwCrshrsSprts();
    DrawCrossSprites();
}

// $81D1
static void MoreCrosshairs() {
    if (FlashScreen != 0) {
        DrawCrossSprites();
        FlashIntroScreen();
        return;
    }
    TitleRoutine++;
    // Leftovers of abandoned code: written, never used.
    ObjectY = 0x60;
    ObjectX = 0x7C;
    AnimIndex = AnimResetIndex;
}

// $822E
static void ChangeIntroNT() {
    PPUCNT0ZP |= 0x01; // name table 1
    TitleRoutine++;
    Timer3 = 0x08;
    FadeDataIndex = 0x06; // FadeInPalData
    SpareMemC9 = 0;
}

// $8243
static void MessageFadeIn() {
    if (Timer3 != 0 || (FrameCount & 0x07) != 0) {
        return;
    }
    if (FadeDataIndex == 0x0B) {
        FadeDataIndex = 0;
        Timer3 = 0x30; // 480 frames
        TitleRoutine++;
        return;
    }
    DoFadeOut();
}

// $8263
static void MessageFadeOut() {
    if (Timer3 != 0 || (FrameCount & 0x07) != 0) {
        return;
    }
    if (FadeDataIndex == 0x05) {
        FadeDataIndex = 0x06;
        SpareMemCB = 0;
        TitleRoutine++;
        return;
    }
    DoFadeOut();
}

// $8283
static void DelayIntroReplay() {
    TitleRoutine++;
    Timer3 = 0x10; // 160 frames
}

// $82A3
static void PrepIntroRestart() {
    if (Timer3 != 0) {
        return;
    }
    SpareMemD2 = 0;
    SpareMemBB = 0;
    IsSamus = 0;
    for (int y = 0x1F; y >= 0; y--) {
        ObjAction[y] = 0;
    }
    PPUCNT0ZP &= 0xFC;
    SpareMemB7 = 0;
    SpareMemB8 = 0;
    PalDataIndex = 0;
    ScrnFlashPalInd = 0;
    IntroStarOffset = 0;
    FadeDataIndex = 0;
    SpareMemCD = 0;
    Joy1Change = 0;
    Joy1Status = 0;
    Joy1Retrig = 0;
    SpareMemD7 = 0;
    SpareMemCE = 1;
    SpareMemCC = 2;
    SpareMemCF = 2;
    TitleRoutine = 2; // FadeInDelay
    if (IntroMusRstrt != 0) {
        IntroMusRstrt--;
        return;
    }
    MultiSFXFlag = MUS_INTRO;
    IntroMusRstrt = 2;
}

// $82ED
static void TitleScreenOff() {
    ScreenOff();
    TitleRoutine++;
}

// --- $87AB Sparkles -------------------------------------------------------------------------------

static void LoadSparkleData() {
    for (int x = 0x0A; x >= 0; x--) {
        uint8_t v = Read((uint16_t)(kInitSprklDatTbl + x));
        IntroSpr0YCoord[x] = v;
        IntroSpr1YCoord[x] = v;
    }
    IntroSpr1YCoord = 0x6B;
    IntroSpr1XCoord = 0xDC;
}

static void SparkleUpdate(uint8_t x);

// $87CF: each sparkle moves twice per frame.
static void UpdtSparkleSprts() {
    SparkleUpdate(0x00);
    SparkleUpdate(0x00);
    SparkleUpdate(0x10);
    SparkleUpdate(0x10);
}

static void DoSprkleprteCoord(uint8_t x);
static void WriteIntroSprite(uint8_t x);

// $87D9
static void SparkleUpdate(uint8_t x) {
    if (IntroSpr0NxtCntr[x] == 0) {
        DoSprkleprteCoord(x);
    }
    if (IntroSpr0Comp[x] != 0) {
        return;
    }
    IntroSpr0NxtCntr[x]--;
    IntroSpr0YCoord[x] += SprklSpr0YChange[x];
    IntroSpr0XCoord[x] += SprklSpr0XChange[x];
    if (--IntroSpr0ChngCnt[x] == 0) {
        // Swap the sparkle's pattern between $C6 and $C5 and flip it.
        IntroSpr0PattTbl[x] ^= 0x03;
        IntroSpr0ChngCnt[x] = 0x20;
        IntroSpr0Cntrl[x] ^= 0x40;
    }
    WriteIntroSprite(x);
}

// $8871
static uint8_t NibbleSubtract(uint8_t a) {
    if (a >= 0x08) {
        return TwosCompliment(a & 0x07);
    }
    return a;
}

// $881A: reads the sparkle's next movement: [frames][YX nibbles], or a whole-byte X move after a
// first byte with its MSB set. A zero first byte ends the path.
static void DoSprkleprteCoord(uint8_t x) {
    uint8_t y = x >> 3;
    GenPtr00LB = Read((uint16_t)(kSprklAddrTbl + y));
    GenPtr00UB = Read((uint16_t)(kSprklAddrTbl + y + 1));
    uint16_t ptr = GenPtr00.Word();
    y = IntroSpr0Index[x];
    uint8_t a = Read((uint16_t)(ptr + y));
    if (a & 0x80) {
        a = 0x01;
        IntroSpr0ByteTyp[x] = a;
    }
    if (a == 0) {
        a = 0x01;
        IntroSpr0Comp[x] = a;
    }
    IntroSpr0NxtCntr[x] = a;
    y++;
    a = Read((uint16_t)(ptr + y));
    bool nibbles = (int8_t)--IntroSpr0ByteTyp[x] < 0;
    if (!nibbles) {
        SprklSpr0YChange[x] = 0;
        a = Read((uint16_t)(ptr + y));
        nibbles = !(a & 0x80);
    }
    if (nibbles) {
        uint8_t data = a;
        IntroSpr0ByteTyp[x] = 0;
        SprklSpr0YChange[x] = NibbleSubtract(data >> 4);
        a = NibbleSubtract(data & 0x0F);
    }
    SprklSpr0XChange[x] = a; // StoreSprklMove
    IntroSpr0Index[x] += 2;
}

// $887B
static void WriteIntroSprite(uint8_t x) {
    Sprite((uint8_t)(0x10 + x)) = (uint8_t)(IntroSpr0YCoord[x] - 1);
    Sprite((uint8_t)(0x11 + x)) = IntroSpr0PattTbl[x];
    Sprite((uint8_t)(0x12 + x)) = IntroSpr0Cntrl[x];
    Sprite((uint8_t)(0x13 + x)) = IntroSpr0XCoord[x];
}

// --- $8897 Crosshairs -----------------------------------------------------------------------------

static void LoadInitSprtDat() {
    Second4Delay = 0x20;
    for (int x = 0x3F; x >= 0; x--) {
        uint8_t v = Read((uint16_t)(kSprt0and4InitTbl + x));
        if (v != PPUCNT0ZP) {
            IntroSpr0YCoord[x] = v;
            IntroSpr4YCoord[x] = v;
        }
    }
    IntroSpr6YCoord = 0xB8;
    IntroSpr7YCoord = 0xB8;
    IntroSpr6YRise = 0x16;
    IntroSpr7YRise = 0x16;
}

static bool UpdateSpriteCoords(uint8_t x);

// $8963
static void DoSpriteMovement(uint8_t x) {
    if (IntroSpr0Comp[x] != 0) {
        return;
    }
    if (!UpdateSpriteCoords(x)) {
        IntroSpr0Comp[x] = 0x01; // in place
    }
    WriteIntroSprite(x);
}

// $88FE: the eight sprites that converge on the middle of the screen.
static void DrwCrshrsSprts() {
    if (First4SlowCntr != 0 && --First4SlowCntr == 0) {
        // Double the speed of all eight sprites.
        for (int s = 0; s < 8; s++) {
            IntroSpr0XRun[s * 0x10] <<= 1;
            IntroSpr0YRise[s * 0x10] <<= 1;
        }
    }
    DoSpriteMovement(0x00);
    DoSpriteMovement(0x10);
    DoSpriteMovement(0x20);
    if (Second4Delay != 0 && --Second4Delay != 0) {
        DoSpriteMovement(0x30);
        return;
    }
    DoSpriteMovement(0x30);
    DoSpriteMovement(0x40);
    DoSpriteMovement(0x50);
    DoSpriteMovement(0x60);
    DoSpriteMovement(0x70);
}

// $8976: the cross in the middle grows and shrinks.
static void DrawCrossSprites() {
    if (DrawCross == 0) {
        return;
    }
    uint8_t y = CrossDataIndex;
    if (y >= 0x04) {
        if (y != 0x04) {
            return;
        }
        DrawCross = 0;
    }
    GenByte00 = Read((uint16_t)(kCrsSprtIndexTbl + y));
    y = 0;
    do {
        uint8_t x = Read((uint16_t)(kCrsSprtDataTbl + y));
        y++;
        do {
            Sprite(x) = Read((uint16_t)(kCrsSprtDataTbl + y));
            x++;
            y++;
        } while (x & 0x03);
    } while (y != GenByte00);
    if (FrameCount & 0x01) {
        CrossDataIndex++;
    }
}

// --- $8AA7 Palettes -------------------------------------------------------------------------------

static void FlashIntroScreen() {
    uint8_t a = Read((uint16_t)(kScrnFlshPalTbl + ScrnFlashPalInd));
    if (a == 0xFF) {
        ScrnFlashPalInd = 0;
        FlashScreen = 0;
        return;
    }
    PalDataPending = a;
    ScrnFlashPalInd++;
}

// The Y register PrepPPUPalStr ($C37E) leaves behind when it finishes normally: the index of the
// string's terminating zero.
static uint8_t PalStrEndIndex(uint16_t src) {
    uint8_t y = 0;
    for (;;) {
        uint8_t control = Read((uint16_t)(src + y));
        if (control == 0) {
            return y;
        }
        uint8_t count = control & 0x3F;
        if (control & 0x40) {
            y++;
        } else {
            y = (uint8_t)(y + (count == 0 ? 0x100 : count));
        }
        y++;
    }
}

void StarPalSwitch() {
    if ((FrameCount & 0x0F) != 0 || PPUStrIndex != 0) {
        return;
    }
    // Sprite palette entries $3F19-$3F1B.
    PPUDestPtrLB = PPU_PAL_LB + 0x19;
    PPUDestPtrUB = PPU_PAL_UB;
    uint8_t y = (uint8_t)((IntroStarOffset & 0x07) << 1);
    PPUSrcPtrLB = Read((uint16_t)(kIntroStarPntr + y));
    PPUSrcPtrUB = Read((uint16_t)(kIntroStarPntr + y + 1));
    IntroStarOffset++;
    y = PalStrEndIndex(PPUSrcPtr.Word());
    PrepPPUPalStr();
    // Then $3F1D-$3F1F from the data that follows.
    PPUDestPtrLB = PPU_PAL_LB + 0x1D;
    PPUDestPtrUB = PPU_PAL_UB;
    y++;
    AddYToPtr02(y);
    PrepPPUPalStr();
}

// $8B5F
static void DoFadeOut() {
    uint8_t a = Read((uint16_t)(kFadeOutPalData + FadeDataIndex));
    if (a == 0xFF) {
        return;
    }
    PalDataPending = a;
    FadeDataIndex++;
}

// --- $909F START/CONTINUE and password screens ----------------------------------------------------

void ClearAll() {
    ScreenOff();
    ClearNameTables();
    EraseAllSprites();
    PPUCNT0ZP &= 0xFC;
    ScrollY = 0;
    ScrollX = 0;
    WaitNMIPass();
    VBOffAndHorzWr();
}

// $90D1
static void TurnOnDisplay() {
    NmiOn();
    ScreenOn();
}

// $90BA
static void StartContScrn() {
    ClearAll();
    PrepPPUProcess_(kStartContStr & 0xFF, kStartContStr >> 8);
    StartContinue = 0;
    PalDataPending = 0x0D;
    TitleRoutine = 0x16; // ChooseStartCont
    TurnOnDisplay();
}

static void InitializeStats();

// $90D7
static void ChooseStartCont() {
    uint8_t a = Joy1Change & (BTN_START | BTN_SELECT);
    if (a == BTN_START) {
        if (StartContinue == 0) {
            InitializeStats();
            return;
        }
        TitleRoutine = 0x17; // LoadPswdScreen
    }
    if (a == BTN_SELECT) {
        StartContinue ^= 0x01;
        TriangleSFXFlag |= 0x08;
    }
    // The selection square.
    Sprite(0) = Read((uint16_t)(kStartContTbl + StartContinue));
    Sprite(1) = 0x6E;
    Sprite(2) = 0x03;
    Sprite(3) = 0x50;
}

static void DispInpChars();

// $911A
static void LoadPswdScreen() {
    ClearAll();
    PrepPPUProcess_(kPasswordPleaseStr & 0xFF, kPasswordPleaseStr >> 8);
    InitGFX7(); // the font
    DispInpChars();
    PalDataPending = 0x13;
    InputRow = 0;
    InputColumn = 0;
    Timer3 = 0;
    PasswordCursor = 0;
    PasswordStat00 = 0;
    TitleRoutine++;
    TurnOnDisplay();
}

// $9450: queues TileSize/TileInfo for the name table at hi:lo.
static void PrepareEraseTiles(uint8_t lo, uint8_t hi) {
    GenByte00 = lo;
    Mem(0x01) = hi;
    Mem(0x02) = 0x80;
    Mem(0x03) = 0x07;
    EraseTile();
}

// $9147
static void EnterPassword() {
    EraseAllSprites();
    if (Joy1Change & BTN_START) {
        CheckPassword();
        return;
    }
    // The error message line, or blanks over it. The original doesn't advance PPUStrIndex past it.
    PPUDataPending = 0x01;
    uint8_t x = PPUStrIndex;
    if (!WritePPUByte(x, 0x21) || !WritePPUByte(x, 0xA8) || !WritePPUByte(x, 0x0F)) {
        return;
    }
    uint16_t msg = Timer3 != 0 ? kErrorMsg : kErrorMsgBlank;
    Mem(0x02) = msg & 0xFF;
    Mem(0x03) = msg >> 8;
    for (uint8_t y = 0; y != 0x0F; y++) {
        if (!WritePPUByte(x, Read((uint16_t)(msg + y)))) {
            return;
        }
    }

    if (Joy1Change & BTN_A) {
        TriangleSFXFlag |= 0x01;
        // Name table column of the password character under the cursor.
        uint8_t a = PasswordCursor;
        if (a >= 0x12) {
            a += 0x3E;
        } else if (a >= 0x0C) {
            a += 0x3D;
        } else if (a >= 0x06) {
            a += 0x0A;
        } else {
            a += 0x09;
        }
        Mem(0x06) = a;
        uint8_t y = (uint8_t)(InputRow << 1);
        GenByte00 = Read((uint16_t)(kPasswordRowTbl + y));
        Mem(0x01) = Read((uint16_t)(kPasswordRowTbl + y + 1));
        uint8_t ch = Read((uint16_t)(GenWord00.Word() + InputColumn));
        TileInfo0 = ch;
        TileSize = 0x11;
        PrepareEraseTiles(Mem(0x06), 0x21);
        PasswordChar00[PasswordCursor] = ch;
        a = (uint8_t)(PasswordCursor + 1);
        if (a >= 0x18) {
            a = 0;
        }
        PasswordCursor = a;
    }

    // CheckBackspace
    if (Joy1Change & BTN_B) {
        uint8_t a = PasswordCursor;
        PasswordCursor = a == 0 ? 0x17 : (uint8_t)(a - 1);
    }
    if (FrameCount & 0x08) {
        // The password cursor.
        Sprite(4) = PasswordCursor < 0x0C ? 0x3F : 0x4F;
        Sprite(5) = 0x6E;
        Sprite(6) = 0x20;
        uint8_t a = PasswordCursor;
        if (a >= 0x0C) {
            a -= 0x0C;
        }
        Sprite(7) = Read((uint16_t)(kCursorPosTbl + a));
    }

    // Moving the character selection.
    x = InputRow;
    uint8_t y = InputColumn;
    uint8_t dirs = Joy1Retrig & 0x0F;
    if (dirs != 0) {
        TriangleSFXFlag |= 0x08;
        if (dirs & BTN_RIGHT) {
            y++;
            if (y == 0x0D) {
                x++;
                if (x == 0x05) {
                    x = 0;
                }
                InputRow = x;
                y = 0;
            }
            InputColumn = y;
        }
        if (dirs & BTN_LEFT) {
            y--;
            if (y & 0x80) {
                x--;
                if (x & 0x80) {
                    x = 0x04;
                }
                InputRow = x;
                y = 0x0C;
            }
            InputColumn = y;
        }
        if (dirs & BTN_DOWN) {
            x++;
            if (x == 0x05) {
                x = 0;
            }
            InputRow = x;
        }
        if (dirs & BTN_UP) {
            x--;
            if (x & 0x80) {
                x = 0x04;
            }
            InputRow = x;
        }
    }
    if (FrameCount & 0x08) {
        Sprite(8) = Read((uint16_t)(kCharSelectYTbl + x));
        Sprite(9) = 0x6E;
        Sprite(0x0A) = 0x20;
        Sprite(0x0B) = Read((uint16_t)(kCharSelectXTbl + y));
    }
}

// $92D4: starts the game from the password data (or a new game).
void InitializeGame() {
    ClearRAM_33_DF();
    ClearSamusStats();
    LoadPasswordData();
    SpritePagePos = 0;
    PageIndex = 0;
    ObjectCntrl = 0;
    ObjectHi = 0;
    SilenceMusic();
    AnimFrame = 0x5A;
    uint8_t x = (InArea & 0x0F) != 0 ? 1 : 0; // Brinstar, or a vertical shaft elsewhere
    ObjectY = Read((uint16_t)(kRestartYPosTbl + x));
    ObjectX = Read((uint16_t)(kRestartXPosTbl + x));
    if (++SamusStat02 == 0) {
        SamusStat03++;
    }
    MainRoutine = 0x01;
    ScreenNmiOff();
    LoadSamusGFX();
    NmiOn();
    SwitchPending = Read((uint16_t)(kBankTable + (InArea & 0x0F)));
}

// $932B: a new game.
static void InitializeStats() {
    SamusStat00 = 0;
    TankCount = 0;
    SamusGear = 0;
    MissileCount = 0;
    MaxMissiles = 0;
    KraidStatueStat = 0;
    RidlyStatueStat = 0;
    SamusAgeLo = 0;
    SamusAgeMid = 0;
    SamusAgeHi = 0;
    SamusStat01 = 0;
    AtEnding = 0;
    JustInBailey = 0;
    SwitchPending = 0x02; // Brinstar
}

static void PasswordToScreen();

// $9359
static void DisplayPassword() {
    if (Timer3 != 0) {
        return; // "GAME OVER" stays for 160 frames
    }
    ClearAll();
    PrepPPUProcess_(kPassWordStr & 0xFF, kPassWordStr >> 8);
    InitGFX7();
    CalculatePassword();
    NmiOn();
    PasswordToScreen();
    WaitNMIPass();
    PalDataPending = 0x13;
    TitleRoutine++;
    ScreenOn();
}

// $9394
static void WaitForSTART() {
    if (Joy1Change & BTN_START) {
        CheckPassword();
    }
}

// $939E GameOver (bank 7 has a GameOver of its own).
static void TitleGameOver() {
    ClearAll();
    PrepPPUProcess_(kGameOverStr & 0xFF, kGameOverStr >> 8);
    InitGFX7();
    NmiOn();
    Timer3 = 0x10;
    TitleRoutine = 0x19; // DisplayPassword
    ScreenOn();
}

// $93F9: six password characters, ending at index y, into TileInfo.
static void LoadPasswordTiles(uint8_t y) {
    TileSize = 0x16;
    for (int x = 5; x >= 0; x--) {
        TileInfo0[x] = PasswordChar00[y];
        y--;
    }
}

// $93C6
static void PasswordToScreen() {
    WaitNMIPass();
    LoadPasswordTiles(0x05);
    PrepareEraseTiles(0xA9, 0x21);
    LoadPasswordTiles(0x0B);
    PrepareEraseTiles(0xB0, 0x21);
    LoadPasswordTiles(0x11);
    PrepareEraseTiles(0xE9, 0x21);
    LoadPasswordTiles(0x17);
    PrepareEraseTiles(0xF0, 0x21);
}

// $940B: the five rows of characters to choose from.
static void DispInpChars() {
    Hw::PpuStatus();
    uint8_t y = 0;
    GenByte00 = 0;
    Mem(0x01) = 0;
    do {
        uint8_t x = (uint8_t)(Mem(0x01) << 1);
        Hw::PpuAddr(Read((uint16_t)(kPasswordRowsTbl + x)));
        Hw::PpuAddr(Read((uint16_t)(kPasswordRowsTbl + x + 1)));
        for (x = 0; x != 0x0D; x++) {
            Hw::PpuData(Read((uint16_t)(kPasswordRow0 + y)));
            Hw::PpuData(0xFF); // blank tile
            y++;
        }
        Mem(0x01)++;
    } while (Mem(0x01) != 0x05);
}

// --- $981E Intro sprite movement --------------------------------------------------------------------

// $9871: a / 8, spread over frames: returns a >> 3, plus one on the frames the remainder calls for.
static uint8_t CalcDisplacement(uint8_t a) {
    Mem(0x04) = a;
    Mem(0x00) = 0x08;
    do {
        bool c = Mem(0x04) & 0x01;
        Mem(0x04) >>= 1;
        if (c && (FrameCount & Mem(0x00)) == 0) {
            Mem(0x04)++;
        }
        Mem(0x00) >>= 1;
    } while (Mem(0x00) != 0);
    return Mem(0x04);
}

// $981E: moves a crosshair sprite. Returns the carry: clear once it has reached its end point.
static bool UpdateSpriteCoords(uint8_t x) {
    uint8_t a = CalcDisplacement(IntroSpr0XRun[x]);
    if (IntroSpr0XDir[x] & 0x80) {
        a = (uint8_t)((a ^ 0xFF) + 1);
    }
    IntroSpr0XCoord[x] += a;
    // Carry of (coord - end), flipped for a sprite moving right: set while not there yet.
    bool c = Cmp(IntroSpr0XCoord[x], IntroSpr0XChange[x]) != (bool)(IntroSpr0XDir[x] & 0x01);
    if (c) {
        a = CalcDisplacement(IntroSpr0YRise[x]);
        if (IntroSpr0YDir[x] & 0x80) {
            a = (uint8_t)((a ^ 0xFF) + 1);
        }
        IntroSpr0YCoord[x] += a;
        c = Cmp(IntroSpr0YCoord[x], IntroSpr0YChange[x]) != (bool)(IntroSpr0YDir[x] & 0x01);
        if (c) {
            return true;
        }
    }
    // In place: snap to the end point.
    IntroSpr0YCoord[x] = IntroSpr0YChange[x];
    IntroSpr0XCoord[x] = IntroSpr0XChange[x];
    return false;
}

// $988A: nudges the intro stars.
void DecSpriteYCoord() {
    if (TitleRoutine >= 0x1D || SpriteLoadPend == 0 || (FrameCount & 0x01)) {
        return;
    }
    uint8_t x = 0x9F;
    do {
        IntroStrSprt00[x]--;
        Sprite((uint8_t)(0x60 + x))--;
        x -= 4;
    } while (x != 0xFF);
    SpriteLoadPend = 0;
}

// $98AE: the intro stars, $6E00-$6E9F, into sprite RAM $0260-$02FF.
static void LoadStarSprites() {
    for (int y = 0x9F; y >= 0; y--) {
        Mem((uint16_t)(0x0260 + y)) = IntroStrSprt00[y];
    }
    SpriteLoadPend = 0;
}

// --- $A93E CopyMap --------------------------------------------------------------------------------

void CopyMap() {
    GenPtr00.SetWord(kWorldMap);
    GenPtr02.SetWord(WorldMapRAM.addr);
    for (int x = 4; x != 0; x--) {
        uint16_t src = GenPtr00.Word();
        uint16_t dst = GenPtr02.Word();
        for (int y = 0; y < 0x100; y++) {
            Write((uint16_t)(dst + y), Read((uint16_t)(src + y)));
        }
        GenPtr00UB++;
        GenPtr02UB++;
    }
}

} // namespace Game
