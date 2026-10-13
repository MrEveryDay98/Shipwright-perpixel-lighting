// Bank 7: the game engine (fixed at $C000 in the original). Routine names and addresses follow the
// community disassembly (nmikstas/metroid-disassembly, Bank07.asm).

#include "game/Engine.h"

#include "game/Enemies.h"

#include "game/Game.h"
#include "game/Area.h"
#include "game/Constants.h"
#include "game/Hw.h"
#include "game/Original.h"
#include "game/Ram.h"
#include "game/Objects.h"
#include "game/Projectiles.h"
#include "game/Samus.h"

#include <initializer_list>

namespace Game {

using namespace Vars;
using namespace Const;

namespace {
std::function<void(int)> sNmiHook;
uint64_t sNmiCount = 0;
int sFrameNmis = 0;
} // namespace

void SetNmiHook(std::function<void(int)> hook) {
    sNmiHook = std::move(hook);
}

uint64_t NmiCount() {
    return sNmiCount;
}

// --- $C000 RandomNumbers -------------------------------------------------------------------------

uint8_t RandomNumbers() {
    for (int i = 0; i < 5; i++) {
        RandomNumber1 += 0x05;
        RandomNumber2 += 0x13;
    }
    return RandomNumber1;
}

// --- $C01A Startup -------------------------------------------------------------------------------

void PowerOn() {
    for (auto& b : gRam) {
        b = 0;
    }
    for (auto& b : gWram) {
        b = 0;
    }
    sNmiCount = 0;

    // MMC1 CHR registers are cleared (Metroid uses CHR RAM), then PRG bank 0 is mapped.
    MMCWriteReg3(0);

    // Clear RAM pages $07 down to $02 (zero page and the stack page are left alone).
    // Cartridge RAM $6000-$7FFF is cleared too. Both loops leave their pointer in $00/$01.
    for (uint16_t a = 0x0200; a < 0x0800; a++) {
        Mem(a) = 0;
    }
    for (uint16_t a = 0x6000; a < 0x8000; a++) {
        Mem(a) = 0;
    }
    GenByte00 = 0x00;
    Mem(0x01) = 0x5F;

    MMCReg0Cntrl = 0x0E; // vertical mirroring, switch $8000, 16 KiB PRG, 8 KiB CHR
    SwitchUprBits = 0x00;
    ScrollX = 0;
    ScrollY = 0;
    Hw::PpuScroll(0);
    Hw::PpuScroll(0);
    GameMode = 1;
    ClearNameTables();
    EraseAllSprites();

    PPUCNT0ZP = 0x90; // NMI on, BG patterns at $1000
    Hw::PpuCtrl(0x90);
    PPUCNT1ZP = 0x02;

    MirrorCntrl = 0x47;
    PrepVertMirror();

    Hw::ApuWrite(0x4011, 0x00); // DMC volume 0
    Hw::ApuWrite(0x4015, 0x0F); // enable square, triangle and noise channels

    TitleRoutine = 0;
    MainRoutine = 0;
    RandomNumber1 = 0x11;
    RandomNumber2 = 0xFF;

    SwitchPending = 1; // switch to bank 0
    CheckSwitch();
    // The original now falls into the wait for the first NMI.
}

// --- $C0BC Main loop / $C0D9 NMI ------------------------------------------------------------------

static void MainLoopPass() {
    CheckSwitch();
    UpdateTimer();
    GoMainRoutine();
    FrameCount++;
    NMIStatus = 0;
}

void Nmi() {
    if (sNmiHook) {
        sNmiHook(sFrameNmis);
    }
    sNmiCount++;
    sFrameNmis++;

    Hw::OamAddr(0);
    Hw::OamDma(0x02);
    if (NMIStatus == 0) { // the main loop finished its frame in time
        if (GameMode != 0) {
            NMIScreenWrite();
        }
        CheckPalWrite();
        CheckPPUWrite();
        WritePPUCtrl();
        WriteScroll();
        ReadJoyPads();
    }
    SoundEngine();
    UpdateAge();
    NMIStatus = 1;
}

void WaitForNmi() {
    // The original spins until the NMI has run. The port runs it here.
    Nmi();
}

int RunFrame() {
    sFrameNmis = 0;
    Nmi();
    RandomNumbers();
    MainLoopPass();
    return sFrameNmis;
}

// --- $C114 GoMainRoutine -------------------------------------------------------------------------

void GoMainRoutine() {
    if (GameMode != 0) {
        TitleMainRoutine(); // bank 0's dispatcher at $8000
        return;
    }
    if (Joy1Change & 0x10) { // START
        uint8_t next = 0;
        bool toggle = true;
        if (MainRoutine == 0x03) {
            next = 0x05; // pause
        } else if (MainRoutine == 0x05) {
            next = 0x03; // resume
        } else {
            toggle = false;
        }
        if (toggle) {
            MainRoutine = next;
            GamePaused ^= 0x01;
            PauseMusic();
        }
    }
    uint8_t routine = MainRoutine;
    ChooseRoutineEffects(0xC141, routine);
    switch (routine) {
        case 0:
            AreaInit();
            break;
        case 1:
            MoreInit();
            break;
        case 2:
            SamusInit();
            break;
        case 3:
            GameEngine();
            break;
        case 4:
            GameOver();
            break;
        case 5:
            PauseMode();
            break;
        case 6:
            GoPassword();
            break;
        case 7:
            MainRoutine++; // IncrementRoutine
            break;
        case 8:
            SamusIntro();
            break;
        case 9:
            WaitTimer();
            break;
        default:
            break; // the original would jump through garbage
    }
}

// $C27C ChooseRoutine: the original pops the jump table's address from the stack and jumps through it,
// leaving the target in CodePtr. The port dispatches with a switch; this reproduces the RAM it leaves.
void ChooseRoutineEffects(uint16_t table, uint8_t index) {
    CodePtr.SetWord(Read16((uint16_t)(table + index * 2)));
}

// --- $C158 ClearNameTables -----------------------------------------------------------------------

void ClearNameTables() {
    ClearNameTable0();
    uint8_t table = 0x02; // name table 1
    if (GameMode != 0 && TitleRoutine == 0x1D) {
        table = 0x03; // the ending uses name table 2
    }
    Mem(0x01) = table;
    GenByte00 = 0xFF;
    ClearNameTable();
}

void ClearNameTable0() {
    Mem(0x01) = 0x01;
    GenByte00 = 0xFF;
    ClearNameTable();
}

// Fills name table ($01 - 1) and its attributes with the byte in $00.
void ClearNameTable() {
    Hw::PpuStatus();
    PPUCNT0ZP &= 0xFB; // increment by 1
    Hw::PpuCtrl(PPUCNT0ZP);
    static const uint16_t kHiPPUTable = 0xC19F;
    Hw::PpuAddr(Read((uint16_t)(kHiPPUTable + Mem(0x01) - 1)));
    Hw::PpuAddr(0x00);
    for (int i = 0; i < 0x400; i++) {
        Hw::PpuData(GenByte00);
    }
}

// --- $C1A3 EraseAllSprites / $C1BC RemIntroSprts ---------------------------------------------------

void EraseAllSprites() {
    GenWord00.SetWord(0x0200);
    for (int i = 0; i < 0x100; i++) {
        Mem((uint16_t)(0x200 + i)) = 0xF0;
    }
    if (GameMode != 0) {
        DecSpriteYCoord();
    }
}

void RemIntroSprts() {
    GenWord00.SetWord(0x0200);
    for (int i = 0x5F; i >= 0; i--) {
        Mem((uint16_t)(0x200 + i)) = 0xF4;
    }
    if (GameMode != 0) {
        DecSpriteYCoord();
    }
}

// --- $C1D4 ClearRAM_33_DF ------------------------------------------------------------------------

void ClearRAM_33_DF() {
    for (uint16_t a = 0x33; a < 0xE0; a++) {
        Mem(a) = 0;
    }
}

// --- $C1E0 CheckPalWrite -------------------------------------------------------------------------

void CheckPalWrite() {
    if (GameMode != 0 && TitleRoutine >= 0x1D) {
        EndGamePalWrite();
        return;
    }
    uint8_t pending = PalDataPending;
    if (pending == 0) {
        if (GameMode != 0 && TitleRoutine < 0x15) {
            StarPalSwitch();
        }
        return;
    }
    // Palette number = PalDataPending - 1; PalPntrTbl ($9560) is in the current area bank.
    static const uint16_t kPalPntrTbl = 0x9560;
    uint8_t index = (uint8_t)((pending - 1) * 2);
    uint8_t lo = Read((uint16_t)(kPalPntrTbl + index));
    uint8_t hi = Read((uint16_t)(kPalPntrTbl + index + 1));
    PalDataPending = 0;
    GenByte00 = lo;
    Mem(0x01) = hi;
    ProcessPPUStr();
}

// --- $C215 ReadJoyPads ---------------------------------------------------------------------------

void ReadJoyPads() {
    Mem(0x01) = 0;
    ReadOnePad(0);
    Mem(0x01)++;
    ReadOnePad(1);
}

void ReadOnePad(int x) {
    Hw::JoypadStrobe(1);
    Hw::JoypadStrobe(0);
    uint8_t buttons = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t v = Hw::JoypadRead(x);
        GenByte00 = v;
        // A disconnected pad reads as all buttons held (bit 0 or bit 1 counts).
        buttons = (uint8_t)((buttons << 1) | ((v | (v >> 1)) & 1));
    }
    int pad = Mem(0x01);
    uint8_t previous = Joy1Status[pad];
    GenByte00 = previous;
    Joy1Status[pad] = buttons;
    uint8_t changed = 0;
    if ((buttons ^ previous) != 0) {
        // With any change, B counts as newly pressed again while held.
        GenByte00 = previous & 0xBF;
        changed = (uint8_t)(GenByte00 ^ buttons) & buttons;
    }
    Joy1Change[pad] = changed;
    Joy1Retrig[pad] = changed;
    uint8_t delay = 0x20;
    if (Joy1Status[pad] == GenByte00) {
        if (--RetrigDelay1[pad] != 0) {
            return;
        }
        Joy1Retrig[pad] = Joy1Status[pad];
        delay = 0x08;
    }
    RetrigDelay1[pad] = delay;
}

// --- $C29A WriteScroll ---------------------------------------------------------------------------

void WriteScroll() {
    Hw::PpuStatus();
    Hw::PpuScroll(ScrollX);
    Hw::PpuScroll(ScrollY);
}

// --- $C2A8 pointer and shift helpers ------------------------------------------------------------

void AddYToPtr00(uint8_t y) {
    bool c = false;
    GenByte00 = Adc(GenByte00, y, c);
    if (c) {
        Mem(0x01)++;
    }
}

void AddYToPtr02(uint8_t y) {
    bool c = false;
    Mem(0x02) = Adc(Mem(0x02), y, c);
    if (c) {
        Mem(0x03)++;
    }
}

// --- $C2CA PPU writing ----------------------------------------------------------------------------

void CheckPPUWrite() {
    if (PPUDataPending == 0) {
        return;
    }
    GenWord00.SetWord(0x07A1);
    ProcessPPUStr();
    PPUStrIndex = 0;
    PPUDataString = 0;
    PPUDataPending = 0;
}

// Writes the PPU data string at ($00): [hi][lo][control][data...] chunks, ending with $00. Control bit
// 7 selects a +32 address increment, bit 6 repeats one byte (RLE), bits 0-5 are the count.
void ProcessPPUStr() {
    for (;;) {
        Hw::PpuStatus();
        uint16_t ptr = GenWord00.Word();
        uint8_t hi = Read(ptr);
        if (hi == 0) {
            WriteScroll();
            return;
        }
        // PPUWrite
        Hw::PpuAddr(hi);
        uint8_t y = 1;
        Hw::PpuAddr(Read((uint16_t)(ptr + y)));
        y++;
        uint8_t control = Read((uint16_t)(ptr + y));
        SetPPUInc(control & 0x80);
        bool rle = control & 0x40;
        uint8_t count = control & 0x3F;
        if (rle) {
            y++;
        }
        do {
            if (!rle) {
                y++;
            }
            Hw::PpuData(Read((uint16_t)(ptr + y)));
        } while (--count != 0);
        y++;
        AddYToPtr00(y);
    }
}

void SetPPUInc(bool by32) {
    uint8_t v = PPUCNT0ZP | 0x04;
    if (!by32) {
        v &= 0xFB;
    }
    Hw::PpuCtrl(v);
    PPUCNT0ZP = v;
}

// Appends a byte to the PPU string. Returns false when the string is full: the original then ends the
// string and abandons the routine that was building it.
static bool WritePPUByte(uint8_t& x, uint8_t value) {
    PPUDataString[x] = value;
    return NextPPUByte(x);
}

bool NextPPUByte(uint8_t& x) {
    x++;
    if (x < 0x4F) {
        return true;
    }
    x = PPUStrIndex;
    EndPPUString(x);
    return false;
}

void EndPPUString(uint8_t x) {
    PPUDataString[x] = 0;
}

// $C328: queues the tiles of a blasted (or regrowing) block. ($02) points at [size][tiles...], ($00) is
// the name table address.
void EraseTile() {
    PPUDataPending = 1;
    uint8_t y = 0;
    uint16_t src = Mem(0x02) | (Mem(0x03) << 8);
    uint8_t size = Read(src);
    Mem(0x05) = size & 0x0F;
    Mem(0x04) = size >> 4;
    uint8_t x = PPUStrIndex;
    do {
        if (!WritePPUByte(x, Mem(0x01)) || !WritePPUByte(x, GenByte00)) {
            return;
        }
        Mem(0x06) = Mem(0x05);
        if (!WritePPUByte(x, Mem(0x05))) {
            return;
        }
        do {
            y++;
            if (!WritePPUByte(x, Read((uint16_t)(src + y)))) {
                return;
            }
        } while (--Mem(0x06) != 0);
        PPUStrIndex = x;
        Mem(0x06) = y;
        AddYToPtr00(0x20);
        y = Mem(0x06);
    } while (--Mem(0x04) != 0);
    EndPPUString(x);
}

// $C37E: queues a PPU string from ($02) for ($00), used by the intro for the star palettes.
void PrepPPUPalStr() {
    PPUDataPending = 1;
    uint8_t y = 0;
    uint16_t src = Mem(0x02) | (Mem(0x03) << 8);
    for (;;) {
        uint8_t x = PPUStrIndex;
        uint8_t control = Read((uint16_t)(src + y));
        if (control == 0) {
            EndPPUString(x);
            return;
        }
        Mem(0x04) = control;
        if (!WritePPUByte(x, Mem(0x01)) || !WritePPUByte(x, GenByte00)) {
            return;
        }
        // SeparateControlBits
        Mem(0x04) = control;
        PPUDataString[x] = control & 0xBF;
        Mem(0x05) = control & 0x3F;
        if (!NextPPUByte(x)) {
            return;
        }
        bool rle = Mem(0x04) & 0x40;
        if (rle) {
            y++;
        }
        do {
            if (!rle) {
                y++;
            }
            if (!WritePPUByte(x, Read((uint16_t)(src + y)))) {
                return;
            }
            Mem(0x06) = y;
            AddYToPtr00((Mem(0x04) & 0x80) ? 0x20 : 0x01);
            y = Mem(0x06);
        } while (--Mem(0x05) != 0);
        PPUStrIndex = x;
        y++;
    }
}

// --- $C3D4 Math ----------------------------------------------------------------------------------

uint8_t TwosCompliment(uint8_t a) {
    return (uint8_t)((a ^ 0xFF) + 1);
}

static uint8_t ExtractNibbles(uint8_t a) {
    Mem(0x01) = a & 0x0F;
    Mem(0x02) = a & 0xF0;
    return Mem(0x03) & 0x0F;
}

// BCD: $03 + a. The carry in is added too, as the original never clears it. Returns the BCD result; the
// carry reports overflow past 99.
uint8_t Base10Add(uint8_t a, bool& carry) {
    uint8_t r = ExtractNibbles(a);
    r = Adc(r, Mem(0x01), carry);
    carry = Cmp(r, 0x0A);
    if (carry) {
        r = Adc(r, 0x05, carry);
    }
    carry = false;
    r = Adc(r, Mem(0x02), carry);
    Mem(0x02) = r;
    r = Mem(0x03) & 0xF0;
    r = Adc(r, Mem(0x02), carry);
    if (!carry) {
        carry = Cmp(r, 0xA0);
        if (!carry) {
            return r;
        }
    }
    r = Adc(r, 0x5F, carry);
    carry = true;
    return r;
}

// BCD: $03 - a (minus one more if the carry is clear on entry). The carry is clear on underflow.
uint8_t Base10Subtract(uint8_t a, bool& carry) {
    uint8_t r = ExtractNibbles(a);
    r = Sbc(r, Mem(0x01), carry);
    Mem(0x01) = r;
    if (!carry) {
        r = Adc(r, 0x0A, carry);
        Mem(0x01) = r;
        r = Adc(Mem(0x02), 0x0F, carry);
        Mem(0x02) = r;
    }
    r = Mem(0x03) & 0xF0;
    carry = true;
    r = Sbc(r, Mem(0x02), carry);
    if (!carry) {
        r = Adc(r, 0xA0, carry);
        carry = false;
    }
    return r | Mem(0x01);
}

// --- $C42C NMI and PPU control ---------------------------------------------------------------------

void WaitNMIPass() {
    NMIStatus = 0;
    WaitForNmi();
}

void ScreenOff() {
    WriteAndWait(PPUCNT1ZP & 0xE7);
}

void WriteAndWait(uint8_t mask) {
    PPUCNT1ZP = mask;
    WaitNMIPass();
}

void ScreenOn() {
    WriteAndWait(PPUCNT1ZP | 0x1E);
}

void WritePPUCtrl() {
    Hw::PpuCtrl(PPUCNT0ZP);
    Hw::PpuMask(PPUCNT1ZP);
    PrepPPUMirror();
}

void ScreenNmiOff() {
    WriteAndWait(PPUCNT1ZP & 0xE7);
    PPUCNT0ZP &= 0x7F;
    Hw::PpuCtrl(PPUCNT0ZP);
}

void VBOffAndHorzWr() {
    uint8_t v = PPUCNT0ZP & 0x7B;
    Hw::PpuCtrl(v);
    PPUCNT0ZP = v;
}

void NmiOn() {
    uint8_t v = PPUCNT0ZP | 0x80;
    Hw::PpuCtrl(v);
    PPUCNT0ZP = v;
}

// --- $C494 Timers ----------------------------------------------------------------------------------

void WaitTimer() {
    if (Timer3 != 0) {
        return;
    }
    uint8_t next = NextRoutine;
    if (next != 0x04 && next != 0x06) {
        StartMusic(); // a power-up's jingle has finished: back to the area music
        next = NextRoutine;
    }
    MainRoutine = next;
}

void SetTimer(uint8_t tenths, uint8_t nextRoutine) {
    Timer3 = tenths;
    NextRoutine = nextRoutine;
    MainRoutine = 0x09;
}

// --- $C4B2 Mirroring --------------------------------------------------------------------------------

void PrepVertMirror() {
    SetPPUMirror(0x47);
}

void SetPPUMirror(uint8_t mirrorCntrl) {
    GenByte00 = (mirrorCntrl >> 3) & 0x01;
    MMCReg0Cntrl = (MMCReg0Cntrl & 0xFE) | GenByte00;
    Hw::SetMmcControl(MMCReg0Cntrl);
}

void PrepPPUMirror() {
    SetPPUMirror(MirrorCntrl);
}

// --- $C266 UpdateTimer ---------------------------------------------------------------------------

void UpdateTimer() {
    int x = 1; // Timer1 and Timer2 count down every frame
    if ((int8_t)--TimerDelay < 0) {
        TimerDelay = 0x09;
        x = 2; // ...and Timer3 every 10 frames
    }
    for (; x >= 0; x--) {
        if (Timer1[x] != 0) {
            Timer1[x]--;
        }
    }
}

// --- $C4DE CheckSwitch ---------------------------------------------------------------------------

void CheckSwitch() {
    uint8_t pending = SwitchPending;
    if (pending == 0) {
        return;
    }
    // SwitchOK
    SwitchPending = 0;
    uint8_t bank = pending - 1;
    CurrentBank = bank;
    ROMSwitch(bank);
    GoBankInit(bank);
}

uint8_t ROMSwitch(uint8_t bank) {
    GenByte00 = bank;
    SwitchUprBits = (SwitchUprBits & 0x18) | bank;
    return MMCWriteReg3(SwitchUprBits);
}

uint8_t MMCWriteReg3(uint8_t value) {
    Hw::SetPrgBank(value);
    return GenByte00;
}

void GoBankInit(uint8_t bank) {
    // BankInitTable at $C51F; the original jumps through $0A/$0B.
    Mem(0x0A) = Read((uint16_t)(0xC51F + bank * 2));
    Mem(0x0B) = Read((uint16_t)(0xC520 + bank * 2));
    switch (bank) {
        case 0:
            InitBank0();
            break;
        case 1:
            InitBank1();
            break;
        case 2:
            InitBank2();
            break;
        case 3:
            InitBank3();
            break;
        case 4:
            InitBank4();
            break;
        case 5:
            InitBank5();
            break;
        default:
            break;
    }
}

// --- $C531 Bank initialization ------------------------------------------------------------------

void InitBank0() {
    GamePaused = 0;
    GameMode = 1;
    ScreenNmiOff();
    CopyMap();
    ClearNameTables();
    // Star sprites for the title screen: $98C0-$995F into $6E00-$6E9F.
    for (uint8_t y = 0xA0; y != 0; y--) {
        Mem((uint16_t)(0x6DFF + y)) = Read((uint16_t)(0x98BF + y));
    }
    InitTitleGFX();
    NmiOn();
}

void InitBank1() {
    GameMode = 0;
    ScreenNmiOff();
    if (MainRoutine != 0x03) {
        MainRoutine = 0;
        InArea = 0;
        GamePaused = 0;
        ClearRAM_33_DF();
        ClearSamusStats();
    }
    ROMSwitch(0x00);
    InitBrinstarGFX();
    NmiOn();
}

void ClearSamusStats() {
    for (int y = 0x0F; y >= 0; y--) {
        Mem((uint16_t)(0x0100 + y)) = 0;
    }
}

void InitBank2() {
    GameMode = 0;
    ScreenNmiOff();
    InitNorfairGFX();
    NmiOn();
}

void InitBank3() {
    GameMode = 0;
    ScreenNmiOff();
    static const uint16_t kMetroidData = 0xC5A8; // initial Metroid behaviour data, to $77F0-$77FD
    for (int y = 0x0D; y >= 0; y--) {
        Mem((uint16_t)(0x77F0 + y)) = Read((uint16_t)(kMetroidData + y));
    }
    InitTourianGFX();
    NmiOn();
}

void InitBank4() {
    GameMode = 0;
    ScreenNmiOff();
    InitKraidGFX();
    NmiOn();
}

void InitBank5() {
    GameMode = 0;
    ScreenNmiOff();
    InitRidleyGFX();
    NmiOn();
}

// --- $C5D0 Pattern table loading ----------------------------------------------------------------------

void InitEndGFX() {
    GameMode = 1;
    InitGFX6();
}

void InitTitleGFX() {
    LoadGFX(0x15);
    LoadSamusGFX();
}

void LoadSamusGFX() {
    LoadGFX(0x00);
    if (JustInBailey != 0) {
        LoadGFX(0x1B); // Samus without the suit
    }
    LoadGFX(0x14);
    LoadGFX(0x17);
    LoadGFX(0x18);
    LoadGFX(0x19);
    LoadGFX(0x16);
}

static void LoadGFXList(std::initializer_list<uint8_t> entries) {
    for (uint8_t e : entries) {
        LoadGFX(e);
    }
}

void InitBrinstarGFX() {
    LoadGFXList({ 0x03, 0x04, 0x05, 0x06, 0x19, 0x16 });
}
void InitNorfairGFX() {
    LoadGFXList({ 0x04, 0x05, 0x07, 0x08, 0x09, 0x19, 0x16 });
}
void InitTourianGFX() {
    LoadGFXList({ 0x05, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x1A, 0x1C, 0x19, 0x16 });
}
void InitKraidGFX() {
    LoadGFXList({ 0x04, 0x05, 0x0A, 0x0F, 0x10, 0x11, 0x19, 0x16 });
}
void InitRidleyGFX() {
    LoadGFXList({ 0x04, 0x05, 0x0A, 0x12, 0x13, 0x19, 0x16 });
}
void InitGFX6() {
    LoadGFXList({ 0x01, 0x02, 0x19, 0x16 });
}
void InitGFX7() {
    LoadGFXList({ 0x17, 0x16 });
}

// Copies one block of graphics into the pattern tables. GFXInfo ($C6E0) entries are 7 bytes:
// [bank][source][PPU destination][length]; the entry is copied to $00-$06 first.
void LoadGFX(uint8_t entry) {
    static const uint16_t kGFXInfo = 0xC6E0;
    uint8_t offset = (uint8_t)(0xFF + (entry + 1) * 7); // last byte of the entry (8-bit, as the original)
    for (int x = 6; x >= 0; x--) {
        Mem((uint16_t)x) = Read((uint16_t)(kGFXInfo + (uint8_t)(offset - (6 - x))));
    }
    ROMSwitch(GenByte00);
    PPUCNT0ZP &= 0xFB;
    Hw::PpuCtrl(PPUCNT0ZP);
    CopyGFXBlock();
    ROMSwitch(CurrentBank);
}

void CopyGFXBlock() {
    if (Mem(0x05) == 0) {
        Mem(0x06)--;
    }
    for (;;) {
        Hw::PpuAddr(Mem(0x04));
        Hw::PpuAddr(Mem(0x03));
        uint8_t y = 0;
        do {
            Hw::PpuData(Read((uint16_t)((Mem(0x01) | (Mem(0x02) << 8)) + y)));
            if (--Mem(0x05) == 0) {
                if (Mem(0x06) == 0) {
                    return;
                }
                Mem(0x06)--;
            }
        } while (++y != 0);
        Mem(0x02)++;
        Mem(0x04)++;
    }
}

// --- $C801 AreaInit / MoreInit ----------------------------------------------------------------------

void AreaInit() {
    ScrollX = 0;
    ScrollY = 0;
    PPUCNT0ZP &= 0xFC; // name table $2000
    MainRoutine++;
    ABStatus = Joy1Status & 0xC0;
    EraseAllSprites();
    IsEngineRunning(0x10);
    MoreInit(); // the original falls through
}

void MoreInit() {
    PalDataPending = 1;
    SpareMem75 = 0xFF;
    AtEnding = 0;
    DoorStatus = 0;
    SamusDoorData = 0;
    UpdtngPrjctl = 0;
    for (int x = 0; x < 0x100; x++) {
        if (x < 0x65) {
            Mem((uint16_t)(0x7A + x)) = 0; // $7A-$DE
        }
        if (x < 0xFF) {
            ObjAction[x] = 0; // $0300-$03FE
        }
    }
    ScreenOff();
    ClearNameTables();
    EraseAllSprites();
    uint8_t x = DestroyEnemies();
    DoorOnNameTable3 = x;
    DoorOnNameTable0 = x;
    x++;
    SpareMem30 = x;
    x++;
    ScrollDir = x; // left
    MapPosX = Read(0x95D7); // Samus's start position on the map
    MapPosY = Read(0x95D8);
    PalToggle = Read(0x95DA);
    RoomNumber = 0xFF;
    CopyPtrs();
    GetRoomNum();
    do {
        SetupRoom();
    } while ((uint8_t)(RoomNumber + 1) != 0);
    Mem(0x01) = CartRAMPtrUB;
    GenByte00 = CartRAMPtrLB;
    PPUCNT0ZP &= 0xFB;
    Hw::PpuCtrl(PPUCNT0ZP);
    Hw::PpuStatus();
    // Room RAM 0 ($6000) into name table 0 ($2000).
    Hw::PpuAddr(0x20);
    Hw::PpuAddr(0x00);
    for (int page = 0; page < 4; page++) {
        uint16_t src = (uint16_t)(GenByte00 | (Mem(0x01) << 8));
        for (int y = 0; y < 0x100; y++) {
            Hw::PpuData(Read((uint16_t)(src + y)));
        }
        Mem(0x01)++;
    }
    Mem(0x91) = 0;
    PalDataPending = 1;
    SpareMem30 = 1;
    MainRoutine++;
    ScreenOn();
}

// Copies the area's 7 data pointers ($959A-$95A7) to $3B-$48.
void CopyPtrs() {
    for (int x = 0x0D; x >= 0; x--) {
        RoomPtrTable[x] = Read((uint16_t)(0x959A + x));
    }
}

// Clears enemy RAM and lets the area clean up. Returns the X register the original leaves behind.
uint8_t DestroyEnemies() {
    for (int x = 0; x < 0x100; x++) {
        if (x < 0x48) {
            Mem((uint16_t)(0x97 + x)) = 0;
        }
        Enstatus[x] = 0;
    }
    MetroidOnSamus = 0;
    return AreaHook(0x95AB, { 0, 0, 0 }).x;
}

// --- $C8D1 SamusInit -----------------------------------------------------------------------------------

void SamusInit() {
    MainRoutine = 0x08;
    Timer3 = 0x2C; // 440 frames of fade-in
    IntroMusic();
    ObjAction = sa_FadeIn0;
    SamusBlink = 0;
    Mem(0x0728) = 0xFF;
    Mem(0x0730) = 0xFF;
    Mem(0x0732) = 0xFF;
    Mem(0x0738) = 0xFF;
    EndTimerLo = 0xFF; // no escape timer
    EndTimerHi = 0xFF;
    Mem(0x8B) = 0xFF;
    Mem(0x8E) = 0xFF;
    uint8_t mirror = 0x27;
    if ((InArea & 0x0F) != 0) {
        ScrollDir >>= 1; // outside Brinstar the start is a vertical shaft: scroll down
        mirror = 0x2F;
    }
    MirrorCntrl = mirror;
    MaxMissilePickup = mirror;
    MaxEnergyPickup = mirror;
    ObjectY = Read(0x95D9);
    ObjectX = 0x80;
    ObjectHi = PPUCNT0ZP & 0x01;
    HealthLo = 0x00; // 30 energy
    HealthHi = 0x03;
}

// --- $C92B GameEngine ------------------------------------------------------------------------------------

void GameEngine() {
    ScrollDoor();
    ScrollDoor(); // twice: doors scroll two pixels per frame
    if (NARPASSWORD != 0) {
        HealthHi = 0x03;
        SamusGear = 0xFF;
        MissileCount = 0x05;
    }
    UpdateWorld();
    if ((MiniBossKillDly | PowerUpDelay) != 0) {
        MiniBossKillDly = 0;
        PowerUpDelay = 0;
        SetTimer(0x18, 0x03); // 4 seconds, then back to the game engine
    }
    if (ObjAction != sa_Dead2 || AnimDelay != 0) {
        return;
    }
    SilenceMusic();
    if (MthrBrainStatus != 0x0A) {
        SetTimer(0x04, 0x04); // then game over
        return;
    }
    MainRoutine++;
}

// --- $C97E UpdateAge ----------------------------------------------------------------------------------

void UpdateAge() {
    if (GameMode != 0 || MainRoutine != 0x03 || FrameCount != 0) {
        return;
    }
    int x = 0;
    SamusAgeLo[x]++;
    if (SamusAgeLo < 0xD0) {
        return;
    }
    SamusAgeLo = 0;
    while (x < 3) {
        x++;
        if (++SamusAgeLo[x] != 0) {
            break;
        }
    }
}

// --- $C9A6 GameOver, PauseMode, GoPassword ---------------------------------------------------------

void GameOver() {
    TitleRoutine = 0x1C;
    SwitchPending = 0x01;
    ScreenOff();
}

void PauseMode() {
    // A and UP on controller 2 quit to the password screen (not during the escape).
    uint8_t a = (Joy2Status & 0x88) ^ 0x88;
    if (a != 0 || (uint8_t)(EndTimerHi + 1) != 0) {
        return;
    }
    GamePaused = a;
    MainRoutine++;
}

void GoPassword() {
    TitleRoutine = 0x19;
    SwitchPending = 0x01;
    NoiseSFXFlag |= 0x01;
    ScreenOff();
}

// --- $C9D7 SamusIntro ------------------------------------------------------------------------------

void SamusIntro() {
    EraseAllSprites();
    uint8_t y = ObjAction;
    uint8_t a = Timer3;
    if (a == 0) {
        ItemRmMusicSts = a;
        ObjAction = sa_Begin;
        StartMusic();
        SelectSamusPal();
        MainRoutine = 0x03;
        a = 0x03;
    }
    if (a >= 0x1F) {
        return; // not yet time to show Samus
    }
    static const uint16_t kSamusFadeTmTbl = 0xCA13;
    if (a == Read((uint16_t)(kSamusFadeTmTbl - 20 + y))) {
        ObjAction++;
        PalDataPending = y;
    }
    if (!(FrameCount & 1)) {
        return; // Samus blinks in on odd frames
    }
    SetSamusAnim(an_SamusFront);
    SpritePagePos = 0;
    PageIndex = 0;
    AnimDrawObject(0x00);
}

// --- $CA18 IsEngineRunning / SwitchBank -----------------------------------------------------------

void IsEngineRunning(uint8_t area) {
    uint8_t y = MainRoutine;
    if (y == 0x07 || y != 0x03) {
        return;
    }
    SwitchBank(area);
}

void SwitchBank(uint8_t area) {
    InArea = area;
    static const uint16_t kBankTable = 0xCA30;
    SwitchPending = Read((uint16_t)(kBankTable + (area & 0x0F)));
    CheckSwitch();
}

// --- $CAF5 ChooseEnding / $CB1C ClearScreenData ----------------------------------------------------

void ChooseEnding() {
    static const uint16_t kAgeTable = 0xCB18;
    uint8_t y = 1;
    for (;;) {
        if (SamusAgeHi != 0) {
            break;
        }
        if (SamusAgeMid >= Read((uint16_t)(kAgeTable - 1 + y))) {
            break;
        }
        y++;
        if (y == 5) {
            break;
        }
    }
    EndingType = y;
    JustInBailey = y >= 4 ? SUIT_OFF : 0;
}

void ClearScreenData() {
    ScreenOff();
    GenByte00 = 0xFF;
    ClearNameTable();
    EraseAllSprites();
}

// --- $CB29 UpdateWorld: one frame of the game world --------------------------------------------------

void UpdateWorld() {
    SpritePagePos = 0; // sprites are built from $0200 up
    UpdateEnemies();
    UpdateProjectiles();
    UpdateSamus();
    AreaRoutine();
    UpdateElevator();
    UpdateStatues();
    DestroyEnemyObjects(); // $FA9D
    UpdateMellows();       // $FC65
    UpdateObjectsF93B();   // $F93B
    UpdateSpinners();      // $FBDD
    SamusEnterDoor();
    DrawDoors(); // $8B79
    UpdateTiles();
    SamusEnemyCollisions(); // $F034
    DisplayBar();
    UpdateObjectsFAF2(); // $FAF2
    CheckMissileToggle();
    UpdateItems();
    UpdateObjectsFDE3(); // $FDE3
    // Hide the sprites not used this frame.
    uint8_t x = SpritePagePos;
    do {
        SpriteRAM[x] = 0xF4;
        x += 4;
    } while (x != 0);
}

// --- $CB73 SelectSamusPal ---------------------------------------------------------------------------

void SelectSamusPal() {
    // Palette 2..5: +1 with Varia, +2 when missiles are selected; +$17 for the suitless palettes.
    bool varia = SamusGear & gr_VARIA;
    uint8_t a = (uint8_t)((MissileToggle << 1) | (varia ? 1 : 0));
    bool carry = MissileToggle & 0x80; // shifted out of MissileToggle, as the original's ROL does
    a = Adc(a, 0x02, carry);
    if (JustInBailey != 0) {
        carry = false;
        a = Adc(a, 0x17, carry);
    }
    PalDataPending = a;
}

// --- $CB8E Sound effect and music triggers ------------------------------------------------------------
// Each sets a flag bit at $0680+X for the sound engine. They return X (the flag byte's index), which
// some callers carry on using.

uint8_t SFX_SetSoundFlag(uint8_t flags, uint8_t x) {
    Mem((uint16_t)(0x0680 + x)) |= flags;
    return x;
}
uint8_t SilenceMusic() {
    return SFX_SetSoundFlag(MUS_NONE, 0);
}
uint8_t PauseMusic() {
    return SFX_SetSoundFlag(MUS_PAUSE, 0);
}
uint8_t SFXSamusWalk() {
    return SFX_SetSoundFlag(0x08, 0);
}
uint8_t SFXBombExplode() {
    return SFX_SetSoundFlag(0x10, 0);
}
uint8_t SFXMissileLaunch() {
    return SFX_SetSoundFlag(0x20, 0);
}
uint8_t SFXEnemyRegen() {
    return SFX_SetSoundFlag(0x08, 1);
}
uint8_t SFXBombLaunch() {
    return SFX_SetSoundFlag(0x01, 3);
}
uint8_t SFX_SamusJump() {
    return SFX_SetSoundFlag(0x02, 1);
}
uint8_t SFX_EnemyHit() {
    return SFX_SetSoundFlag(0x04, 1);
}
uint8_t SFX_BulletFire() {
    return SFX_SetSoundFlag(0x10, 1);
}
uint8_t SFXMetal() {
    return SFX_SetSoundFlag(0x20, 1);
}
uint8_t SFX_EnergyPickup() {
    return SFX_SetSoundFlag(0x40, 1);
}
uint8_t SFX_MissilePickup() {
    return SFX_SetSoundFlag(SFX_MSL_PKUP, 1);
}
uint8_t SFX_WaveFire() {
    return SFX_SetSoundFlag(0x01, 1);
}
uint8_t SFX_ScrewAttack() {
    return SFX_SetSoundFlag(0x40, 0);
}
uint8_t SFX_BigEnemyHit() {
    return SFX_SetSoundFlag(0x04, 3);
}
uint8_t SFX_MetroidHit() {
    return SFX_SetSoundFlag(0x20, 3);
}
uint8_t SFX_BossHit() {
    return SFX_SetSoundFlag(0x02, 4);
}
uint8_t SFXDoor() {
    return SFX_SetSoundFlag(0x40, 3);
}
uint8_t SFX_SamusHit() {
    return SFX_SetSoundFlag(0x04, 4);
}
uint8_t SFX_SamusDie() {
    return SFX_SetSoundFlag(0x80, 3);
}
uint8_t SFX_SetX2(uint8_t flags) {
    return SFX_SetSoundFlag(flags, 2);
}
uint8_t SFX_SamusBall() {
    return SFX_SetSoundFlag(0x02, 3);
}
uint8_t SFXBeep() {
    return SFX_SetSoundFlag(0x08, 3);
}
uint8_t PowerUpMusic() {
    return SFX_SetSoundFlag(0x40, 4);
}
uint8_t IntroMusic() {
    return SFX_SetSoundFlag(0x80, 4);
}
uint8_t MotherBrainMusic() {
    return SFX_SetSoundFlag(0x02, 5);
}
uint8_t TourianMusic() {
    return SFX_SetSoundFlag(0x40, 5);
}

// --- $DB17 CheckMissileToggle -----------------------------------------------------------------------

void CheckMissileToggle() {
    if (MissileCount == 0 || ((Joy1Change | Joy1Retrig) & 0x20) == 0) {
        return; // no missiles, or SELECT not pressed
    }
    MissileToggle ^= 0x01;
    SelectSamusPal();
}

// --- Not translated yet ---------------------------------------------------------------------------

void UpdateStatues() {
    Original(0xD9D4);
}
void DisplayBar() {
    Original(0xE0C1);
}
void UpdateItems() {
    Original(0xDB37);
}

void GetRoomNum() {
    Original(0xE720);
}
void SetupRoom() {
    Original(0xEA2B);
}
void ScrollDoor() {
    Original(0xE1F1);
}



} // namespace Game
