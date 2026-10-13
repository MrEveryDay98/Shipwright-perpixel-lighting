// The sound engine ($B31B-$BCAF and $BF19-$BF55 in every bank). Routine names and addresses follow the
// community disassembly (nmikstas/metroid-disassembly, Bank01.asm).
//
// Banks 0-5 each carry a byte-identical copy of the engine's code and of its routine address tables;
// only the music data (InitMusicTbl at $BD31 and the songs) differs. The code is translated once here
// and every table (SFX register data at $B200, routine addresses, note periods and lengths, volume
// envelopes, songs) is read from whichever bank is mapped, through Read().

#include "game/Sound.h"

#include "game/Engine.h"
#include "game/Hw.h"
#include "game/Memory.h"
#include "game/Original.h"
#include "game/Ram.h"

namespace Game {

using namespace Vars;

namespace {

// Zero page pointers of the music player (no names in the defines).
constexpr Var MusicDataPtrE6{ 0x00E6 }; // Base address of the channel being processed.
constexpr Var VolDataPtrEC{ 0x00EC };   // Volume envelope of the channel being processed.

// Table addresses (the same in every bank).
constexpr uint16_t SFXDatTbl = 0xB200; // Four APU register values per SFX.
constexpr uint16_t SpitFlamesTbl = 0xB4FB;
constexpr uint16_t BossHitSFXDataTbl = 0xB63C;
constexpr uint16_t MslPickupSFXTbl = 0xB6A9;
constexpr uint16_t IceBeamSFXDataTbl = 0xB778;
constexpr uint16_t WvBmSFXPerLowTbl = 0xB7A7;
constexpr uint16_t WvBmSFXDisLnTbl = 0xB7A9;
constexpr uint16_t InitMusicIndexTbl = 0xBBFA;
constexpr uint16_t VolCntrlAddrTbl = 0xBCB0;
constexpr uint16_t InitMusicTbl = 0xBD31;
constexpr uint16_t MusicNotesTbl = 0xBE77;
constexpr uint16_t NoteLengths0Tbl = 0xBEF7;

void JumpToSFXRoutine();
void LoadSQ1SFXInitFlags();
void LoadMusicInitFlags();
void LoadCurMusFrameData();

// --- $B31B-$B363 The SFX cycles ----------------------------------------------------------------------

// $B4BD CheckSFXFlag: loads $E0-$E3 and ChannelType from the row of ChooseNextSFXRoutineTbl whose low
// address byte is tblLow. If a flag is set, $E2/$E3 get the routine for its highest set bit (from the
// table at $E0/$E1), otherwise they keep the row's routine for the next SFX cycle.
void CheckSFXFlag(uint8_t flags, uint8_t tblLow) {
    CurrentSFXFlags = flags;
    SFXPtrE4LB = tblLow;
    SFXPtrE4UB = 0xB2;
    uint16_t row = SFXPtrE4.Word();
    for (int i = 0; i < 4; i++) {
        Mem((uint16_t)(0x00E0 + i)) = Read((uint16_t)(row + i));
    }
    ChannelType = Read((uint16_t)(row + 4));
    // Looks for the highest set flag. The ASLs on CurrentSFXFlags are undone below.
    for (uint8_t y = 0; y != 0x10; y += 2) {
        if (flags & (0x80 >> (y / 2))) {
            uint16_t routines = SFXPtrE0.Word();
            SFXPtrE2LB = Read((uint16_t)(routines + y));
            SFXPtrE2UB = Read((uint16_t)(routines + y + 1));
            break;
        }
    }
    CurrentSFXFlags = flags; // RestoreSFXFlags
}

// $B337 GotoSFXCheckFlags
void GotoSFXCheckFlags(uint8_t flags, uint8_t tblLow) {
    CheckSFXFlag(flags, tblLow);
    JumpToSFXRoutine();
}

// $B31B LdNseSFXInitFlags (1st SFX cycle)
void LdNseSFXInitFlags() {
    GotoSFXCheckFlags(NoiseSFXFlag, 0x89);
}

// $B322 LoadNoiseSFXContFlags (2nd)
void LoadNoiseSFXContFlags() {
    GotoSFXCheckFlags(NoiseContSFX, 0x8E);
}

// $B329 LoadSQ1SFXInitFlags (5th)
void LoadSQ1SFXInitFlags() {
    GotoSFXCheckFlags(SQ1SFXFlag, 0x93);
}

// $B330 LoadSQ1SFXContFlags (6th)
void LoadSQ1SFXContFlags() {
    GotoSFXCheckFlags(SQ1ContSFX, 0x98);
}

// $B33D LdSTriSFXInitFlags (7th)
void LdSTriSFXInitFlags() {
    GotoSFXCheckFlags(TriangleSFXFlag, 0x9D);
}

// $B344 LoadTriangleSFXContFlags (8th)
void LoadTriangleSFXContFlags() {
    GotoSFXCheckFlags(TriangleContSFX, 0xA2);
}

// $BC53 FindMusicInitIndex: the number of the highest set bit of CurrentSFXFlags (bit 7 = 0).
void FindMusicInitIndex() {
    MusicInitIndex = 0xFF;
    uint8_t a = CurrentSFXFlags;
    if (a == 0) {
        return;
    }
    bool carry;
    do {
        MusicInitIndex += 1;
        a = Asl(a, carry);
    } while (!carry);
}

// $BC64 Add8
void Add8() {
    MusicInitIndex += 8;
}

// $B34B LdMultiSFXInitFlags (3rd): the multi-channel SFX flags also start some music.
void LdMultiSFXInitFlags() {
    CheckSFXFlag(MultiSFXFlag, 0xA7);
    FindMusicInitIndex();
    Add8();
    JumpToSFXRoutine();
}

// $B35C LoadMultiSFXContFlags (4th)
void LoadMultiSFXContFlags() {
    GotoSFXCheckFlags(MultiContSFX, 0xAC);
}

// $B364 LoadSQ1Flags
void LoadSQ1Flags() {
    LoadSQ1SFXInitFlags();
}

// --- $B368 Loading a channel's four registers from SFXDatTbl -----------------------------------------

// $B376 LoadSFXData / $B384 LoadSFXRegisters: copies four bytes at $B200+dataLow into the channel's
// registers $4000+channel..+3 through the pointers at $E0 and $E2.
void LoadSFXData(uint8_t channel, uint8_t dataLow) {
    SFXPtrE0LB = channel;
    SFXPtrE0UB = 0x40;
    SFXPtrE2LB = dataLow;
    SFXPtrE2UB = 0xB2;
    uint16_t dest = SFXPtrE0.Word();
    uint16_t src = SFXPtrE2.Word();
    for (int y = 0; y < 4; y++) {
        Hw::ApuWrite((uint16_t)(dest + y), Read((uint16_t)(src + y)));
    }
}

void LoadSQ1ChannelSFX(uint8_t dataLow) { // $B368
    LoadSFXData(0x00, dataLow);
}
void LoadTriChannelSFX(uint8_t dataLow) { // $B36C
    LoadSFXData(0x08, dataLow);
}
void LoadNoiseChannelSFX(uint8_t dataLow) { // $B370
    LoadSFXData(0x0C, dataLow);
}
void LoadSQ2ChannelSFX(uint8_t dataLow) { // $B374
    LoadSFXData(0x04, dataLow);
}

// --- $B38F Paused game ------------------------------------------------------------------------------

void ClearSounds();

// $B38F PauseSFX and the code before SoundEngine ($B399): while paused, plays the GamePaused SFX every
// fourth frame for the first #$12 frames.
void PausedSoundEngine() {
    if (SFXPaused == 0) {
        SFXPaused += 1;
        ClearSounds();
        PauseSFXStatus = 0x00;
        return;
    }
    if (PauseSFXStatus == 0x12) {
        return;
    }
    if ((PauseSFXStatus & 0x03) == 0x03) {
        LoadSQ1ChannelSFX(0x0D); // GamePausedSFXData
    }
    PauseSFXStatus += 1;
}

// --- $B3D6-$B451 Clearing sound state -----------------------------------------------------------------

// $B3D6 ClearSFXFlags
void ClearSFXFlags() {
    NoiseSFXFlag = 0;
    SQ1SFXFlag = 0;
    SQ2SFXFlag = 0;
    TriangleSFXFlag = 0;
    MultiSFXFlag = 0;
    MusicInitFlag = 0;
}

void InitSoundAddresses();

// $B3F0 CheckRepeatMusic: at the end of the music, restarts it if it repeats, else stops all sound.
void CheckRepeatMusic() {
    if (MusicRepeat != 0) {
        CrntMusicRepeat = CurrentMusic;
        return;
    }
    InitSoundAddresses();
}

// $B40E ClearSpecialAddrs
void ClearSpecialAddrs() {
    TriCounterCntrl = 0;
    SFXPaused = 0;
    CrntMusicRepeat = 0;
    MusicRepeat = 0;
}

// $B41D ClrMusAndSFXAddrs
void ClrMusAndSFXAddrs() {
    SQ1InUse = 0;
    SQ2InUse = 0;
    TriangleInUse = 0;
    WrtMultiChnDat = 0;
    NoiseContSFX = 0;
    SQ1ContSFX = 0;
    SQ2ContSFX = 0;
    TriangleContSFX = 0;
    MultiContSFX = 0;
    CurrentMusic = 0;
}

// $B43E ClearSounds
void ClearSounds() {
    Hw::ApuWrite(0x4000, 0x10); // SQ1Cntrl0
    Hw::ApuWrite(0x4004, 0x10); // SQ2Cntrl0
    Hw::ApuWrite(0x400C, 0x10); // NoiseCntrl0
    Hw::ApuWrite(0x4008, 0x00); // TriangleCntrl0
    Hw::ApuWrite(0x4011, 0x00); // DMCCntrl1
}

// $B404 InitSoundAddresses
void InitSoundAddresses() {
    ClrMusAndSFXAddrs();
    ClearSounds();
    ClearSpecialAddrs();
}

// $B3FC CheckMusicFlags: restarting the music already playing only clears the repeat state.
void CheckMusicFlags() {
    if (CurrentMusic == CurrentSFXFlags) {
        ClearSpecialAddrs();
        return;
    }
    InitSoundAddresses();
}

// --- $B452-$B4BC Common SFX helpers -------------------------------------------------------------------

// $B493 UpdateContFlags: the channel's continuation flags become CurrentSFXFlags.
void UpdateContFlags() {
    uint8_t x = ChannelType;
    NoiseContSFX[x] = (uint8_t)((NoiseContSFX[x] & 0x00) | CurrentSFXFlags);
}

// $B452 SelectSFXRoutine: starts an SFX of `length` frames on ChannelType's channel with the register
// data at $B200+dataLow.
void SelectSFXRoutine(uint8_t length, uint8_t dataLow) {
    uint8_t x = ChannelType;
    NoiseSFXLength[x] = length;
    switch (x) {
        case 0:
            LoadNoiseChannelSFX(dataLow);
            break;
        case 1:
            LoadSQ1ChannelSFX(dataLow);
            break;
        case 2: // MusicBranch00
            LoadSQ2ChannelSFX(dataLow);
            break;
        case 3: // MusicBranch01
            LoadTriChannelSFX(dataLow);
            break;
        default:
            return; // no channel
    }
    UpdateContFlags();
    x = ChannelType;
    NoiseInUse[x] = x; // marks the channel as used by an SFX
    ThisNoiseFrame[x] = 0;
    NoiseSFXData[x] = 0;
    MultiSFXData[x] = 0;
    ScrewAtkSFXData[x] = 0;
    WrtMultiChnDat = 0;
}

// $B4A2 ClrCrntSFXFlags: ends the SFX by clearing the channel's continuation flags.
void ClrCrntSFXFlags() {
    CurrentSFXFlags = 0;
    UpdateContFlags();
}

// $B4A9 IncrementSFXFrame: advances the channel's SFX frame, wrapping to 0 at the SFX length. Returns
// false (the original's Z flag set) when it wrapped.
bool IncrementSFXFrame() {
    uint8_t x = ChannelType;
    ThisNoiseFrame[x] += 1;
    if (ThisNoiseFrame[x] != NoiseSFXLength[x]) {
        return true;
    }
    ThisNoiseFrame[x] = 0;
    return false;
}

// --- $B4FB Noise SFX ------------------------------------------------------------------------------------

// $B58F EndNoiseSFX
void EndNoiseSFX() {
    ClrCrntSFXFlags();
    Hw::ApuWrite(0x400C, 0x10); // NoiseCntrl0: sound off
}

// $B50F SpitFlameSFXStart
void SpitFlameSFXStart() {
    SelectSFXRoutine(0x14, 0x21);
}

// $B516 SpitFlameSFXContinue
void SpitFlameSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndNoiseSFX();
        return;
    }
    Hw::ApuWrite(0x400C, Read((uint16_t)(SpitFlamesTbl + NoiseSFXData)));
    NoiseSFXData += 1;
}

// $B52B ScrewAttackSFXStart
void ScrewAttackSFXStart() {
    SelectSFXRoutine(0x05, 0x11);
    NoiseSFXData = Read(0xB213);
}

// $B564 IncPeriodIndex
void IncPeriodIndex() {
    NoiseSFXData += 1;
    Hw::ApuWrite(0x400E, NoiseSFXData);
}

// $B539 ScrewAttackSFXContinue
void ScrewAttackSFXContinue() {
    if (ScrewAtkSFXData != 0x02) {
        if (!IncrementSFXFrame()) {
            ScrewAtkSFXData += 1;
        }
        return;
    }
    if (IncrementSFXFrame()) {
        IncPeriodIndex();
        return;
    }
    NoiseSFXData -= 3;
    MultiSFXData += 1;
    if (MultiSFXData == 0x0F) {
        EndNoiseSFX();
    }
}

// $B56E MissileLaunchSFXStart
void MissileLaunchSFXStart() {
    SelectSFXRoutine(0x18, 0x15);
    NoiseSFXData = 0x0A;
}

// $B57B MissileLaunchSFXContine
void MissileLaunchSFXContine() {
    if (IncrementSFXFrame()) {
        IncPeriodIndex();
        return;
    }
    EndNoiseSFX();
}

// $B583 BombExplodeSFXStart
void BombExplodeSFXStart() {
    SelectSFXRoutine(0x30, 0x19);
}

// $B58A NoiseSFXContinue (bomb explode and Samus walk)
void NoiseSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndNoiseSFX();
    }
}

// $B598 SamusWalkSFXStart: not while a missile launch, walk or spit flame SFX plays.
void SamusWalkSFXStart() {
    if (NoiseContSFX & 0x34) {
        return;
    }
    SelectSFXRoutine(0x03, 0x1D);
}

// --- $B5A5 Multi channel SFX --------------------------------------------------------------------------

// $B5A5 MultiSFXInit: SQ1 was loaded by the caller; loads SQ2 and takes both channels from the music.
void MultiSFXInit(uint8_t length, uint8_t sq2DataLow) {
    MultiSFXLength = length;
    LoadSQ2ChannelSFX(sq2DataLow);
    UpdateContFlags();
    SQ1InUse = 0x01;
    SQ2InUse = 0x02;
    SQ1ContSFX = 0;
    SQ1SFXData = 0;
    SQ1SQ2SFXData = 0;
    SQ1SFXPeriodLow = 0;
    ThisMultiFrame = 0;
    WrtMultiChnDat = 0;
}

// $B5CD EndMultiSFX
void EndMultiSFX() {
    Hw::ApuWrite(0x4000, 0x10); // SQ1Cntrl0
    Hw::ApuWrite(0x4004, 0x10); // SQ2Cntrl0
    Hw::ApuWrite(0x4001, 0x7F); // SQ1Cntrl1: sweep off
    Hw::ApuWrite(0x4005, 0x7F); // SQ2Cntrl1
    ClrCrntSFXFlags();
    SQ1InUse = 0;
    SQ2InUse = 0;
    WrtMultiChnDat += 1;
}

// $B5EC BossHitSFXStart
void BossHitSFXStart() {
    LoadSQ1ChannelSFX(0x2D);
    // A still holds 4 from the register copy loop: the SFX length.
    MultiSFXInit(0x04, 0x29);
}

// $B62C WriteSQ1SQ2PerLo
void WriteSQ1SQ2PerLo() {
    Hw::ApuWrite(0x4006, SQ1SQ2SFXData);   // SQ2Cntrl2
    Hw::ApuWrite(0x4002, SQ1SFXPeriodLow); // SQ1Cntrl2
}

// $B5F6 BossHitSFXContinue
void BossHitSFXContinue() {
    SQ1SFXData += 1;
    uint8_t a = Read((uint16_t)(BossHitSFXDataTbl + SQ1SFXData));
    Hw::ApuWrite(0x4000, a);
    Hw::ApuWrite(0x4004, a);
    a = SQ1SFXData;
    if (a == 0x14) {
        EndMultiSFX();
        return;
    }
    if (a >= 0x06) {
        a = (uint8_t)((RandomNumber1 | 0x10) & 0x7F);
        SQ1SFXPeriodLow = a;
        bool carry = true; // from the CMP #$06 above
        SQ1SQ2SFXData = Rol(a, carry);
    } else {
        SQ1SQ2SFXData += 2;
        SQ1SFXPeriodLow += 2;
    }
    WriteSQ1SQ2PerLo();
}

// $B650 SamusHitSFXContinue
void SamusHitSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndMultiSFX();
        return;
    }
    LoadSQ1ChannelSFX(0x25);
    Hw::ApuWrite(0x4002, RandomNumber1 & 0x0F);
    LoadSQ2ChannelSFX(0x25);
    Hw::ApuWrite(0x4006, (RandomNumber1 >> 2) & 0x0F);
}

// $B673 SamusHitSFXStart
void SamusHitSFXStart() {
    LoadSQ1ChannelSFX(0x25);
    Hw::ApuWrite(0x4002, RandomNumber1 & 0x0F);
    MultiSFXInit((uint8_t)((RandomNumber1 & 0x03) + 1), 0x25);
    Hw::ApuWrite(0x4006, (RandomNumber1 >> 2) & 0x0F);
}

// $B695 IncorrectPasswordSFXStart
void IncorrectPasswordSFXStart() {
    LoadSQ1ChannelSFX(0x31);
    MultiSFXInit(0x20, 0x35);
}

// $B6A1 IncorrectPasswordSFXContinue
void IncorrectPasswordSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndMultiSFX();
    }
}

// --- $B6A9 SQ1 SFX ------------------------------------------------------------------------------------

// $B6F2 EndSQ1SFX
void EndSQ1SFX() {
    Hw::ApuWrite(0x4000, 0x10); // SQ1Cntrl0: sound off
    SQ1InUse = 0;
    ClrCrntSFXFlags();
    WrtMultiChnDat += 1;
}

// $B6B0 MissilePickupSFXContinue
void MissilePickupSFXContinue() {
    if (IncrementSFXFrame()) {
        return;
    }
    uint8_t a = Read((uint16_t)(MslPickupSFXTbl + SQ1SFXData));
    if (a == 0) {
        EndSQ1SFX();
        return;
    }
    Hw::ApuWrite(0x4002, a);
    Hw::ApuWrite(0x4003, Read(0xB244));
    SQ1SFXData += 1;
}

// $B6CD MissilePickupSFXStart
void MissilePickupSFXStart() {
    SelectSFXRoutine(0x05, 0x41);
}

// $B6D3 EnergyPickupSFXContinue
void EnergyPickupSFXContinue() {
    if (IncrementSFXFrame()) {
        return;
    }
    SQ1SFXData += 1;
    if (SQ1SFXData == 0x03) {
        EndSQ1SFX();
        return;
    }
    LoadSQ1ChannelSFX(0x3D);
}

// $B6E7 EnergyPickupSFXStart
void EnergyPickupSFXStart() {
    SelectSFXRoutine(0x06, 0x3D);
}

// $B6ED SQ1SFXContinue (metal, enemy regenerate, enemy hit, Samus jump)
void SQ1SFXContinue() {
    if (!IncrementSFXFrame()) {
        EndSQ1SFX();
    }
}

// $B703 SamusJumpSFXStart: silent during the escape music.
void SamusJumpSFXStart() {
    if (CurrentMusic == 0x04) {
        return;
    }
    SelectSFXRoutine(0x0C, 0x51);
}

// $B710 EnemyHitSFXStart
void EnemyHitSFXStart() {
    SelectSFXRoutine(0x08, 0x55);
}

// $B75E HasIceBeamSFXStart
void HasIceBeamSFXStart() {
    SelectSFXRoutine(0x07, 0x61);
}

// $B716 BulletFireSFXStart
void BulletFireSFXStart() {
    if (HasBeamSFX & 0x01) {
        HasIceBeamSFXStart();
        return;
    }
    if (SQ1ContSFX & 0xCC) {
        return; // missile pickup, energy pickup, enemy regenerate or enemy hit playing
    }
    if (HasBeamSFX & 0x80) {
        SelectSFXRoutine(0x07, 0x49); // HasLongBeamSFXStart
    } else {
        SelectSFXRoutine(0x03, 0x4D); // short beam
    }
}

// $B735 MetalSFXStart
void MetalSFXStart() {
    SelectSFXRoutine(0x0B, 0x45);
}

// $B73C EnRegenSFXStart: the bomb ticking sound during the escape music.
void EnRegenSFXStart() {
    if (CurrentMusic == 0x04) {
        SelectSFXRoutine(0x07, 0x39);
    } else {
        SelectSFXRoutine(0x16, 0x59);
    }
}

// $B7A0 LoadSQ1PeriodLow
void LoadSQ1PeriodLow(uint8_t a) {
    Hw::ApuWrite(0x4002, a);
    SQ1SFXData += 1;
}

// $B765 HasIceBeamSFXContinue
void HasIceBeamSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndSQ1SFX();
        return;
    }
    // The original branches on the value being non-zero; both table entries are.
    LoadSQ1PeriodLow(Read((uint16_t)(IceBeamSFXDataTbl + (SQ1SFXData & 0x01))));
}

// $B74F BulletFireSFXContinue
void BulletFireSFXContinue() {
    if (HasBeamSFX & 0x01) {
        HasIceBeamSFXContinue();
        return;
    }
    if (!IncrementSFXFrame()) {
        EndSQ1SFX();
    }
}

// $B77A WaveBeamSFXStart
void WaveBeamSFXStart() {
    SelectSFXRoutine(0x08, 0x5D);
}

// $B781 WaveBeamSFXContinue
void WaveBeamSFXContinue() {
    if (IncrementSFXFrame()) {
        LoadSQ1PeriodLow(Read((uint16_t)(WvBmSFXPerLowTbl + (SQ1SFXData & 0x01))));
        return;
    }
    uint8_t y = SQ1SQ2SFXData;
    SQ1SQ2SFXData += 1;
    uint8_t a = Read((uint16_t)(WvBmSFXDisLnTbl + y));
    Hw::ApuWrite(0x4000, a);
    if (a == 0) {
        EndSQ1SFX();
    }
}

// --- $B7AC Triangle SFX -------------------------------------------------------------------------------

// $B978 IncreaseTriPeriods
void IncreaseTriPeriods() {
    bool carry = false;
    TriPeriodLow = Adc(TriPeriodLow, TriChangeLow, carry);
    TriPeriodHigh = Adc(TriPeriodHigh, TriChangeHigh, carry);
}

// $B98C DecreaseTriPeriods
void DecreaseTriPeriods() {
    bool carry = true;
    TriPeriodLow = Sbc(TriPeriodLow, TriChangeLow, carry);
    TriPeriodHigh = Sbc(TriPeriodHigh, TriChangeHigh, carry);
}

// $B9A0 DivideTriePeriods: TriLo/HiPercentage = the triangle period divided by PercentDiff, by shift
// and subtract; the periods themselves are left as they were. The carry the original rotates in first
// ends up back in the carry at the end, so it doesn't affect the results.
void DivideTriePeriods() {
    uint8_t savedLow = TriPeriodLow;
    uint8_t savedHigh = TriPeriodHigh;
    DivideData = 0;
    bool carry = false;
    TriPeriodLow = Rol(TriPeriodLow, carry);
    TriPeriodHigh = Rol(TriPeriodHigh, carry);
    for (int x = 0x10; x != 0; x--) {
        DivideData = Rol(DivideData, carry);
        carry = Cmp(DivideData, PercentDiff);
        if (carry) {
            DivideData = Sbc(DivideData, PercentDiff, carry);
        }
        TriPeriodLow = Rol(TriPeriodLow, carry);
        TriPeriodHigh = Rol(TriPeriodHigh, carry);
    }
    TriLoPercentage = TriPeriodLow;
    TriHiPercentage = TriPeriodHigh;
    TriPeriodHigh = savedHigh;
    TriPeriodLow = savedLow;
}

// $B869 WriteTriPeriods
void WriteTriPeriods() {
    Hw::ApuWrite(0x400A, TriPeriodLow);
    Hw::ApuWrite(0x400B, TriPeriodHigh | 0x08);
}

// $B896 EndTriangleSFX
void EndTriangleSFX() {
    Hw::ApuWrite(0x4008, 0x00); // TriangleCntrl0: sound off
    TriangleInUse = 0;
    Hw::ApuWrite(0x400B, 0x18);
    ClrCrntSFXFlags();
}

// $B7AC DoorOpenCloseSFXStart
void DoorOpenCloseSFXStart() {
    TriPeriodLow = Read(0xB287);
    TriPeriodHigh = Read(0xB288) & 0x07;
    TriChangeLow = 0x0F;
    TriChangeHigh = 0x00;
    SelectSFXRoutine(0x1F, 0x85);
}

// $B7CB DoorOpenCloseSFXContinue
void DoorOpenCloseSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndTriangleSFX();
        return;
    }
    DecreaseTriPeriods();
    WriteTriPeriods();
}

// $B7D9 BeepSFXStart: not while the bomb launch SFX plays.
void BeepSFXStart() {
    if (TriangleContSFX & 0x80) {
        return;
    }
    SelectSFXRoutine(0x03, 0x79);
}

// $B7E7 BeepSFXContinue
void BeepSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndTriangleSFX();
    }
}

// $B7EF BigEnemyHitSFXStart
void BigEnemyHitSFXStart() {
    TriChangeLow = 0x12;
    TriChangeHigh = 0x00;
    TriPeriodLow = Read(0xB27F);
    TriPeriodHigh = Read(0xB280) & 0x07;
    SelectSFXRoutine(0x0A, 0x7D);
}

// $B80E BigEnemyHitSFXContinue
void BigEnemyHitSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndTriangleSFX();
        return;
    }
    IncreaseTriPeriods();
    TriangleSFXData = RandomNumber1 & 0x3C;
    Hw::ApuWrite(0x400A, (TriPeriodLow & 0xC3) | TriangleSFXData);
    Hw::ApuWrite(0x400B, TriPeriodHigh | 0x40);
}

// $B834 SamusToBallSFXStart
void SamusToBallSFXStart() {
    SelectSFXRoutine(0x08, 0x6D);
    PercentDiff = 0x05;
    TriPeriodLow = Read(0xB26F);
    TriPeriodHigh = Read(0xB270) & 0x07;
}

// $B84F SamusToBallSFXContinue
void SamusToBallSFXContinue() {
    if (!IncrementSFXFrame()) {
        EndTriangleSFX();
        return;
    }
    DivideTriePeriods();
    TriChangeLow = TriLoPercentage;
    TriChangeHigh = TriHiPercentage;
    DecreaseTriPeriods();
    WriteTriPeriods();
}

// $B878 BombLaunchSFXStart
void BombLaunchSFXStart() {
    SelectSFXRoutine(0x04, 0x65);
}

// $B87F BombLaunchSFXContinue
void BombLaunchSFXContinue() {
    if (IncrementSFXFrame()) {
        return;
    }
    TriangleSFXData += 1;
    if (TriangleSFXData == 0x02) {
        EndTriangleSFX();
        return;
    }
    LoadTriChannelSFX(0x69); // second part
}

// $B8C3 RndTrianglePeriods
void RndTrianglePeriods() {
    uint8_t a = RandomNumber1 | 0x6C;
    Hw::ApuWrite(0x400A, a);
    Hw::ApuWrite(0x400B, (a & 0x01) | 0xF8);
}

// $B8A7 MetroidHitSFXStart
void MetroidHitSFXStart() {
    SelectSFXRoutine(0x03, 0x71);
    RndTrianglePeriods();
}

// $B8B1 MetroiHitSFXContinue
void MetroiHitSFXContinue() {
    if (!IncrementSFXFrame()) {
        RndTrianglePeriods();
        return;
    }
    TriangleSFXData += 1;
    if (TriangleSFXData == 0x09) {
        EndTriangleSFX();
    }
}

// $B8D2 SamusDieSFXStart
void SamusDieSFXStart() {
    InitSoundAddresses();
    SelectSFXRoutine(0x0E, 0x75);
    PercentDiff = 0x15;
    TriPeriodLow = Read(0xB277);
    TriPeriodHigh = 0x00;
}

// $B8ED SamusDieSFXContinue
void SamusDieSFXContinue() {
    if (IncrementSFXFrame()) {
        DivideTriePeriods();
        TriChangeLow = TriLoPercentage;
        TriChangeHigh = TriHiPercentage;
        IncreaseTriPeriods();
        WriteTriPeriods();
        return;
    }
    TriChangeLow = 0x20;
    TriChangeHigh = 0x00;
    DecreaseTriPeriods();
    TriangleSFXData += 1;
    if (TriangleSFXData == 0x06) {
        EndTriangleSFX();
    }
}

// $B921 StatueRaiseSFXStart
void StatueRaiseSFXStart() {
    TriPeriodLow = Read(0xB283);
    TriPeriodHigh = Read(0xB284) & 0x07;
    TriChangeHigh = 0x00;
    TriChangeLow = 0x0B;
    SelectSFXRoutine(0x06, 0x81);
}

// $B940 StatueRaiseSFXContinue
void StatueRaiseSFXContinue() {
    if (IncrementSFXFrame()) {
        DecreaseTriPeriods();
        WriteTriPeriods();
        return;
    }
    TriangleSFXData += 1;
    if (TriangleSFXData == 0x09) {
        EndTriangleSFX();
        return;
    }
    uint8_t savedLow = TriChangeLow;
    uint8_t savedHigh = TriChangeHigh;
    TriChangeLow = 0x25;
    TriChangeHigh = 0x00;
    IncreaseTriPeriods();
    TriChangeHigh = savedHigh;
    TriChangeLow = savedLow;
    WriteTriPeriods();
}

// --- $B9E4 Music player ------------------------------------------------------------------------------

// $B9E4 SetVolAndDisblSweep
void SetVolAndDisblSweep(uint8_t sq1Duty, uint8_t sq2Duty) {
    MusicSQ1Sweep = 0x7F;
    MusicSQ2Sweep = 0x7F;
    SQ1DutyEnvelope = sq1Duty;
    SQ2DutyEnvelope = sq2Duty;
}

// $B9F3 ResetVolumeIndex: restarts the volume envelope at the beginning of a note.
void ResetVolumeIndex() {
    if (SQ1MusicFrameCnt == 0x01) {
        SQ1VolumeIndex = 0x01;
    }
    if (SQ2MusicFrameCnt == 0x01) {
        SQ2VolumeIndex = 0x01;
    }
}

// $BA08 LoadSQ1SQ2Periods: after an SFX released SQ1/SQ2, rewrites the music's registers.
void LoadSQ1SQ2Periods() {
    if (WrtMultiChnDat == 0) {
        return;
    }
    WrtMultiChnDat = 0;
    Hw::ApuWrite(0x4001, MusicSQ1Sweep);
    Hw::ApuWrite(0x4002, MusicSQ1PrdLow);
    Hw::ApuWrite(0x4003, MusicSQ1PrdHi);
    Hw::ApuWrite(0x4005, MusicSQ2Sweep);
    Hw::ApuWrite(0x4006, MusicSQ2PeriodLo);
    Hw::ApuWrite(0x4007, MusicSQ2PeriodHi);
}

// $BA41 WriteSQCntrl0: applies this frame of the channel's volume envelope (x: 0 SQ1, 1 SQ2).
void WriteSQCntrl0(uint8_t x) {
    uint8_t a = SQ1VolumeCntrl[x];
    if (a == 0) {
        return; // no volume envelope
    }
    VolCntrlAddress = a;
    LoadSQ1SQ2Periods();
    uint8_t y;
    if (SQ1VolumeData[x] == 0x10) {
        y = 0x10; // channel silent
    } else {
        y = 0;
        for (;;) {
            VolCntrlAddress -= 1;
            if (VolCntrlAddress == 0) {
                break;
            }
            y += 2;
            if (y == 0) {
                break;
            }
        }
        VolDataPtrEC[0] = Read((uint16_t)(VolCntrlAddrTbl + y));
        VolDataPtrEC[1] = Read((uint16_t)(VolCntrlAddrTbl + y + 1));
        a = Read((uint16_t)(VolDataPtrEC.Word() + SQ1VolumeIndex[x]));
        Cntrl0Data = a;
        if (a == 0xFF) {
            // MusicBranch05: end of the envelope, back to the channel's own volume. A duty/volume of 0
            // falls into MusicBranch06.
            y = SQ1DutyEnvelope[x];
            if (y == 0) {
                y = 0x10;
            }
            goto write; // the index is not advanced
        }
        if (a == 0xF0) {
            y = 0x10; // MusicBranch06: silent until the next note
            goto write;
        }
        y = (uint8_t)((SQ1DutyEnvelope[x] & 0xF0) | Cntrl0Data);
    }
    SQ1VolumeIndex[x] += 1;
write:
    if (SQ1InUse[x] != 0) {
        return; // the channel plays an SFX
    }
    if (x == 0) {
        Hw::ApuWrite(0x4000, y); // WriteSQ1Cntrl0
    } else {
        Hw::ApuWrite(0x4004, y); // WriteSQ2Cntrl0
    }
}

// $BA37 LoadSQ1SQ2Channels
void LoadSQ1SQ2Channels() {
    WriteSQCntrl0(0);
    WriteSQCntrl0(1);
}

// $BBA8 LdNewMusFrameCount
void LdNewMusFrameCount(uint8_t x) {
    SQ1MusicFrameCnt[x] = SQ1FrmCountInit[x];
}

// $BBDE LdNseChannelMusic: the noise channel plays the drum beats at $B200+y, unless a noise SFX plays.
void LdNseChannelMusic(uint8_t y) {
    if ((NoiseContSFX & 0xFC) == 0) {
        Hw::ApuWrite(0x400C, Read((uint16_t)(SFXDatTbl + y)));
        Hw::ApuWrite(0x400E, Read((uint16_t)(SFXDatTbl + 1 + y)));
        Hw::ApuWrite(0x400F, Read((uint16_t)(SFXDatTbl + 2 + y)));
    }
    LdNewMusFrameCount(3);
}

// $BB37 LdSndDatIndexIndex: the next byte of the channel's data.
uint8_t LdSndDatIndexIndex(uint8_t x) {
    uint8_t y = SQ1MusicIdxIdx[x];
    SQ1MusicIdxIdx[x] += 1;
    return Read((uint16_t)(MusicDataPtrE6.Word() + y));
}

// $BB40: plays note y (an index into MusicNotesTbl, or the drum beat for the noise channel) on
// channel x (0 SQ1, 1 SQ2, 2 triangle, 3 noise).
void PlayMusicNote(uint8_t x, uint8_t y) {
    if (x == 0x03) {
        LdNseChannelMusic(y);
        return;
    }
    uint8_t regs = ThisSoundChannel;
    uint8_t a = Read((uint16_t)(MusicNotesTbl + 1 + y));
    if (a != 0) {
        MusicSQ1PrdLow[regs] = a;
        a = Read((uint16_t)(MusicNotesTbl + y)) | 0x08;
        MusicSQ1PrdHi[regs] = a;
    }
    if (a == 0) {
        // NoPeriodInformation: a rest.
        Cntrl0Data = 0x00;
        if (x != 0x02) {
            Cntrl0Data = 0x10;
        }
    } else {
        Cntrl0Data = SQ1DutyEnvelope[x]; // PeriodInformationFound (TriLinearCount for the triangle)
    }
    SQ1InUse[x] -= 1;
    if (x == SQ1InUse[x]) {
        SQ1InUse[x] += 1; // SQ1SQ2InUse: an SFX has the channel
        LdNewMusFrameCount(x);
        return;
    }
    SQ1InUse[x] += 1;
    uint8_t y2 = ThisSoundChannel;
    if (x == 0x02 || SQ1VolumeCntrl[x] == 0) {
        Hw::ApuWrite((uint16_t)(0x4000 + y2), Cntrl0Data);
    }
    SQ1VolumeData[x] = Cntrl0Data;
    Hw::ApuWrite((uint16_t)(0x4002 + y2), MusicSQ1PrdLow[y2]);
    Hw::ApuWrite((uint16_t)(0x4003 + y2), MusicSQ1PrdHi[y2]);
    Hw::ApuWrite((uint16_t)(0x4001 + y2), MusicSQ1Sweep[x]);
    LdNewMusFrameCount(x);
}

// $BBB7 LoadTriangleCntrl0: the triangle's linear counter for a note of `length` frames.
void LoadTriangleCntrl0(uint8_t length) {
    uint8_t a = TriCounterCntrl & 0x0F;
    if (a != 0) {
        // short note: the low bits are the count
    } else if (TriCounterCntrl & 0xF0) {
        a = 0xFF; // play until the next note
    } else {
        // AddTriangleLength
        a = (uint8_t)((uint8_t)(length - 1) << 2);
        if (a >= 0x3C) {
            a = 0x3C;
        }
    }
    TriLinearCount = a;
}

// $BB1C LoadMusicChannel: a note length byte ($Bx) followed by a note, or a note alone.
void LoadMusicChannel(uint8_t x, uint8_t y) {
    if ((y & 0xB0) == 0xB0) {
        uint8_t idx = (uint8_t)((y & 0x0F) + NoteLenTblOffset);
        uint8_t length = Read((uint16_t)(NoteLengths0Tbl + idx));
        SQ1FrmCountInit[x] = length;
        if (x == 0x02) {
            LoadTriangleCntrl0(length);
        }
        y = LdSndDatIndexIndex(x);
    }
    PlayMusicNote(x, y);
}

// $BADC LdNextChnlIndexDat: reads the channel's data until a note is played. Returns false when the
// music has ended (the original then leaves LoadCurMusFrameData).
bool LdNextChnlIndexDat(uint8_t x) {
    for (;;) {
        uint8_t y = SQ1MusicIdxIdx[x];
        SQ1MusicIdxIdx[x] += 1;
        uint8_t a = Read((uint16_t)(MusicDataPtrE6.Word() + y));
        if (a == 0) {
            CheckRepeatMusic(); // GotoCheckRepeatMusic
            return false;
        }
        y = a;
        if (a == 0xFF) {
            // RepeatMusicLoop
            if (SQ1RepeatCounter[x] == 0) {
                continue;
            }
            SQ1RepeatCounter[x] -= 1;
            SQ1MusicIdxIdx[x] = SQ1LoopIndex[x];
            if (SQ1MusicIdxIdx[x] != 0) {
                continue;
            }
            // A loop starting at index 0 falls into StartNewMusicLoop with Y = #$FF.
        } else if ((a & 0xC0) != 0xC0) {
            LoadMusicChannel(x, y);
            return true;
        }
        // StartNewMusicLoop
        SQ1RepeatCounter[x] = y & 0x3F;
        SQ1RepeatCounter[x] -= 1;
        SQ1LoopIndex[x] = SQ1MusicIdxIdx[x];
    }
}

// $BAA5 LoadCurMusFrameData: runs the four music channels for this frame.
void LoadCurMusFrameData() {
    ResetVolumeIndex();
    ThisSoundChannel = 0x00;
    for (uint8_t x = 0;;) {
        uint8_t x2 = (uint8_t)(x * 2);
        MusicDataPtrE6[0] = SQ1LowBaseByte[x2];
        MusicDataPtrE6[1] = SQ1HighBaseByte[x2];
        if (SQ1HighBaseByte[x2] != 0) { // the channel has data
            SQ1MusicFrameCnt[x] -= 1;
            if (SQ1MusicFrameCnt[x] == 0) {
                if (!LdNextChnlIndexDat(x)) {
                    return;
                }
            }
        }
        // IncToNextChannel
        x++;
        if (x == 0x04) {
            LoadSQ1SQ2Channels(); // GotoLoadSQ1SQ2Channels
            return;
        }
        ThisSoundChannel += 0x04;
    }
}

// $BF19 InitializeMusic
void InitializeMusic() {
    CheckMusicFlags();
    CurrentMusic = CurrentSFXFlags;
    uint8_t y = Read((uint16_t)(InitMusicIndexTbl + MusicInitIndex));
    for (uint8_t x = 0; x != 0x0D; x++, y++) {
        NoteLenTblOffset[x] = Read((uint16_t)(InitMusicTbl + y)); // $062B-$0637
    }
    SQ1MusicFrameCnt = 0x01;
    SQ2MusicFrameCnt = 0x01;
    TriMusicFrameCnt = 0x01;
    NseMusicFrameCnt = 0x01;
    SQ1MusicIdxIdx = 0x00;
    SQ2MusicIdxIdx = 0x00;
    TriMusicIdxIdx = 0x00;
    NoiseMusicIdxIdx = 0x00;
}

// $BC8B XYMusicInit (Music00Init-Music05Init end here with the SQ1 and SQ2 duty/volume)
void XYMusicInit(uint8_t sq1Duty, uint8_t sq2Duty) {
    SetVolAndDisblSweep(sq1Duty, sq2Duty);
    InitializeMusic();
    LoadCurMusFrameData();
}

// --- $BC36 Music cycles --------------------------------------------------------------------------

// $BC36 LoadMusicTempFlags (9th SFX cycle): restarts repeating music.
void LoadMusicTempFlags() {
    CheckSFXFlag(CrntMusicRepeat, 0xB6);
    FindMusicInitIndex();
    JumpToSFXRoutine();
}

// $BC3D LoadMusicInitFlags (10th)
void LoadMusicInitFlags() {
    CheckSFXFlag(MusicInitFlag, 0xB1);
    FindMusicInitIndex();
    JumpToSFXRoutine();
}

// $BC4B ContinueMusic (11th)
void ContinueMusic() {
    if (CurrentMusic != 0) {
        LoadCurMusFrameData();
    }
}

// JMP ($00E2): the routine CheckSFXFlag chose, from the routine tables at $B2BB-$B31A and
// $BC06-$BC35 (read from the ROM), or the next SFX cycle.
void JumpToSFXRoutine() {
    uint16_t routine = SFXPtrE2.Word();
    switch (routine) {
        case 0xB4EE: // RTS: no sound
            break;

        // SFX cycles
        case 0xB322:
            LoadNoiseSFXContFlags();
            break;
        case 0xB330:
            LoadSQ1SFXContFlags();
            break;
        case 0xB344:
            LoadTriangleSFXContFlags();
            break;
        case 0xB35C:
            LoadMultiSFXContFlags();
            break;
        case 0xB364:
            LoadSQ1Flags();
            break;
        case 0xBC3D:
            LoadMusicInitFlags();
            break;
        case 0xBC4B:
            ContinueMusic();
            break;

        // Noise
        case 0xB50F:
            SpitFlameSFXStart();
            break;
        case 0xB516:
            SpitFlameSFXContinue();
            break;
        case 0xB52B:
            ScrewAttackSFXStart();
            break;
        case 0xB539:
            ScrewAttackSFXContinue();
            break;
        case 0xB56E:
            MissileLaunchSFXStart();
            break;
        case 0xB57B:
            MissileLaunchSFXContine();
            break;
        case 0xB583:
            BombExplodeSFXStart();
            break;
        case 0xB58A:
            NoiseSFXContinue();
            break;
        case 0xB598:
            SamusWalkSFXStart();
            break;

        // Multi channel
        case 0xB5EC:
            BossHitSFXStart();
            break;
        case 0xB5F6:
            BossHitSFXContinue();
            break;
        case 0xB650:
            SamusHitSFXContinue();
            break;
        case 0xB673:
            SamusHitSFXStart();
            break;
        case 0xB695:
            IncorrectPasswordSFXStart();
            break;
        case 0xB6A1:
            IncorrectPasswordSFXContinue();
            break;

        // SQ1
        case 0xB6B0:
            MissilePickupSFXContinue();
            break;
        case 0xB6CD:
            MissilePickupSFXStart();
            break;
        case 0xB6D3:
            EnergyPickupSFXContinue();
            break;
        case 0xB6E7:
            EnergyPickupSFXStart();
            break;
        case 0xB6ED:
            SQ1SFXContinue();
            break;
        case 0xB703:
            SamusJumpSFXStart();
            break;
        case 0xB710:
            EnemyHitSFXStart();
            break;
        case 0xB716:
            BulletFireSFXStart();
            break;
        case 0xB735:
            MetalSFXStart();
            break;
        case 0xB73C:
            EnRegenSFXStart();
            break;
        case 0xB74F:
            BulletFireSFXContinue();
            break;
        case 0xB77A:
            WaveBeamSFXStart();
            break;
        case 0xB781:
            WaveBeamSFXContinue();
            break;

        // Triangle
        case 0xB7AC:
            DoorOpenCloseSFXStart();
            break;
        case 0xB7CB:
            DoorOpenCloseSFXContinue();
            break;
        case 0xB7D9:
            BeepSFXStart();
            break;
        case 0xB7E7:
            BeepSFXContinue();
            break;
        case 0xB7EF:
            BigEnemyHitSFXStart();
            break;
        case 0xB80E:
            BigEnemyHitSFXContinue();
            break;
        case 0xB834:
            SamusToBallSFXStart();
            break;
        case 0xB84F:
            SamusToBallSFXContinue();
            break;
        case 0xB878:
            BombLaunchSFXStart();
            break;
        case 0xB87F:
            BombLaunchSFXContinue();
            break;
        case 0xB8A7:
            MetroidHitSFXStart();
            break;
        case 0xB8B1:
            MetroiHitSFXContinue();
            break;
        case 0xB8D2:
            SamusDieSFXStart();
            break;
        case 0xB8ED:
            SamusDieSFXContinue();
            break;
        case 0xB921:
            StatueRaiseSFXStart();
            break;
        case 0xB940:
            StatueRaiseSFXContinue();
            break;

        // Music (Music00Start-Music05Start jump to Music00Init-Music05Init)
        case 0xBC77: // Music00Init
            XYMusicInit(0x92, 0x96);
            break;
        case 0xBC7A: // Music01Init
            XYMusicInit(0xB6, 0xF6);
            break;
        case 0xBC7D: // Music02Init
            XYMusicInit(0xF4, 0xF4);
            break;
        case 0xBC80: // Music03Init
            XYMusicInit(0x34, 0x34);
            break;
        case 0xBC83: // Music04Init
            XYMusicInit(0xB3, 0xB3);
            break;
        case 0xBC86: // Music05Init
            XYMusicInit(0xF5, 0xF6);
            break;

        default: // not reachable with the game's tables
            Original(routine);
            break;
    }
}

} // namespace

// --- $B3B4 SoundEngine ----------------------------------------------------------------------------
// Runs the SFX cycles in this order: noise init, noise continue, multi init, multi continue, SQ1 init,
// SQ1 continue, triangle init, triangle continue, music repeat, music init, then the music itself.

void SoundEngine() {
    Hw::ApuWrite(0x4017, 0xC0); // APUCommonCntrl1: 5-step sequence, no frame interrupt
    if (NoiseSFXFlag & 0x01) {  // silence music
        InitSoundAddresses();
        ClearSFXFlags();
        return;
    }
    if (MainRoutine == 0x05) {
        PausedSoundEngine();
        return;
    }
    SFXPaused = 0x00;
    LdNseSFXInitFlags();
    LdMultiSFXInitFlags();
    LdSTriSFXInitFlags();
    LoadMusicTempFlags();
    ClearSFXFlags();
}

} // namespace Game
