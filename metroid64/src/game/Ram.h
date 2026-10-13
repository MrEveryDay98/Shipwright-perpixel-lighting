#pragma once

// Metroid's RAM and cartridge RAM variables, named after the community disassembly
// (nmikstas/metroid-disassembly, Metroid_Defines.asm), at their original addresses.

#include "game/Memory.h"

namespace Game::Vars {

// --- General Purpose Variables
inline constexpr Var GenByte00{0x0000}; // General purpose byte.
inline constexpr Var GenWord00{0x0000}; // General use word.
inline constexpr Var GenWord00LB{0x0000}; // General use word, lower byte.
inline constexpr Var GenWord00UB{0x0001}; // General use word, upper byte.
inline constexpr Var GenPtr00{0x0000}; // General use pointer.
inline constexpr Var GenPtr00LB{0x0000}; // General use pointer, lower byte.
inline constexpr Var GenPtr00UB{0x0001}; // General use pointer, upper byte.
inline constexpr Var GenByte02{0x0002}; // General purpose byte.
inline constexpr Var GenPtr02{0x0002}; // General use pointer.
inline constexpr Var GenPtr02LB{0x0002}; // General use pointer, lower byte.
inline constexpr Var GenPtr02UB{0x0003}; // General use pointer, upper byte.
inline constexpr Var GenByte03{0x0003}; // General purpose byte.
inline constexpr Var GenByte04{0x0004}; // General purpose byte.
inline constexpr Var GenByte33{0x0033}; // General purpose byte.
// --- Variable Defines
inline constexpr Var PPUDestPtr{0x0000}; // PPU write destination pointer.
inline constexpr Var PPUDestPtrLB{0x0000}; // PPU write destination pointer, lower byte.
inline constexpr Var PPUDestPtrUB{0x0001}; // PPU write destination pointer, upper byte.
inline constexpr Var PPUSrcPtr{0x0002}; // PPU write source pointer.
inline constexpr Var PPUSrcPtrLB{0x0002}; // PPU write source pointer, lower byte.
inline constexpr Var PPUSrcPtrUB{0x0003}; // PPU write source pointer, upper byte.
inline constexpr Var CodePtr{0x000C}; // Pointer to an indirect function call.
inline constexpr Var CodePtrUB{0x000C}; // Pointer to an indirect function call, lower byte.
inline constexpr Var CodePtrLB{0x000D}; // Pointer to an indirect function call, upper byte.
inline constexpr Var Joy1Change{0x0012}; // Button changes since last frame, pad 1.
inline constexpr Var Joy2Change{0x0013}; // Button changes since last frame, pad 2.
inline constexpr Var Joy1Status{0x0014}; // Buttons currently being pressed, pad 1.
inline constexpr Var Joy2Status{0x0015}; // Buttons currently being pressed, pad 2.
inline constexpr Var Joy1Retrig{0x0016}; // Buttons that need to retrigger, pad 1.
inline constexpr Var Joy2Retrig{0x0017}; // Buttons that need to retrigger, pad 2.
inline constexpr Var RetrigDelay1{0x0018}; // Counter that control retriggering, pad 1.
inline constexpr Var RetrigDelay2{0x0019}; // Counter that control retriggering, pad 2.
inline constexpr Var NMIStatus{0x001A}; // 0=NMI in progress. anything else, NMI not in progress.
inline constexpr Var PPUDataPending{0x001B}; // 1=No PPU data pending, 1=data pending.
inline constexpr Var PalDataPending{0x001C}; // Pending palette data. Palette #=PalDataPending-1.
inline constexpr Var GameMode{0x001D}; // 0=Game is playing, 1=At title/password screen
inline constexpr Var MainRoutine{0x001E}; // 5=Game paused, 3=Game engine running
inline constexpr Var TitleRoutine{0x001F}; // Stores title routine number currently running.
inline constexpr Var NextRoutine{0x0020}; // Stores next routine to jump to after WaitTimer expires.
inline constexpr Var CurrentBank{0x0023}; // 0 thru 7. current memory page in lower memory block.
inline constexpr Var SwitchPending{0x0024}; // Switch memory page. Page #=SwitchPending-1.
inline constexpr Var MMCReg0Cntrl{0x0025}; // Stores bits to be loaded into MMC1 Register 0.
inline constexpr Var SwitchUprBits{0x0028}; // Used to store bits 3 and 4 for MMC1 register 3.  Bits
inline constexpr Var TimerDelay{0x0029}; // Count down from 9 to 0. Decremented every frame.
inline constexpr Var Timer1{0x002A}; // Decremented every frame after set.
inline constexpr Var Timer2{0x002B}; // Decremented every frame after set.
inline constexpr Var Timer3{0x002C}; // Decremented every 10 frames after set.
inline constexpr Var FrameCount{0x002D}; // Increments every frame(overflows every 256 frames).
inline constexpr Var RandomNumber1{0x002E}; // Random numbers used
inline constexpr Var RandomNumber2{0x002F}; // throughout the game.
inline constexpr Var SpareMem30{0x0030}; // Written to, but never accessed.
inline constexpr Var GamePaused{0x0031}; // #$00=Game running, #$01=Game paused.
inline constexpr Var RoomPtr{0x0033}; // Room pointer.
inline constexpr Var RoomPtrLB{0x0033}; // Room pointer, lower byte.
inline constexpr Var RoomPtrUB{0x0034}; // Room pointer, upper byte.
inline constexpr Var StructPtr{0x0035}; // Structure pointer.
inline constexpr Var StructPtrLB{0x0035}; // Structure pointer, lower byte.
inline constexpr Var StructPtrUB{0x0036}; // Structure pointer, upper byte.
inline constexpr Var CartRAMWorkPtr{0x0037}; // Pointer to current position in room RAM.
inline constexpr Var CartRAMWorkPtrLB{0x0037}; // Pointer to current position in room RAM, lower byte.
inline constexpr Var CartRAMWorkPtrUB{0x0038}; // Pointer to current position in room RAM, upper byte.
inline constexpr Var CartRAMPtr{0x0039}; // pointer to room RAM.
inline constexpr Var CartRAMPtrLB{0x0039}; // pointer to room RAM, lower byte.
inline constexpr Var CartRAMPtrUB{0x003A}; // pointer to room RAM, upper byte.
inline constexpr Var RoomPtrTable{0x003B}; // start of room pointer table.
inline constexpr Var RoomPtrTableLB{0x003B}; // start of room pointer table, lower byte.
inline constexpr Var RoomPtrTableUB{0x003C}; // start of room pointer table, upper byte.
inline constexpr Var StructPtrTable{0x003D}; // start of structure pointer table.
inline constexpr Var StructPtrTableLB{0x003D}; // start of structure pointer table, lower byte.
inline constexpr Var StructPtrTableUB{0x003E}; // start of structure pointer table, upper byte.
inline constexpr Var MacroPtr{0x003F}; // Pointer into macro definitions.
inline constexpr Var MacroPtrLB{0x003F}; // Pointer into macro definitions, lower byte.
inline constexpr Var MacroPtrUB{0x0040}; // Pointer into macro definitions, upper byte.
inline constexpr Var EnemyAnimPtr{0x0047}; // EnemyAnimIndexTbl pointer.
inline constexpr Var EnemyAnimPtrLB{0x0047}; // EnemyAnimIndexTbl pointer, lower byte.
inline constexpr Var EnemyAnimPtrUB{0x0048}; // EnemyAnimIndexTbl pointer, upper byte.
inline constexpr Var ScrollDir{0x0049}; // 0=Up, 1=Down, 2=Left, 3=Right.
inline constexpr Var TempScrollDir{0x004A}; // Stores ScrollDir when room is initially loaded.
inline constexpr Var PageIndex{0x004B}; // Index to object data.
inline constexpr Var ItemIndex{0x004C}; // #$00 or #$08. Added to PowerUpType addresses to determine if
inline constexpr Var SamusDir{0x004D}; // 0 = Right, 1 = Left.
inline constexpr Var SamusDoorDir{0x004E}; // Direction Samus passed through door.
inline constexpr Var MapPosY{0x004F}; // Current y position on world map.
inline constexpr Var MapPosX{0x0050}; // Current x position on world map.
inline constexpr Var SamusScrX{0x0051}; // Samus x position on screen.
inline constexpr Var SamusScrY{0x0052}; // Samus y position on screen.
inline constexpr Var WalkSoundDelay{0x0053};
inline constexpr Var IsSamus{0x0055}; // 1=Samus object being accessed, 0=not Samus.
inline constexpr Var DoorStatus{0x0056}; // 0=Not in door, 1=In right door, 2=In left door, 3=Scroll up
inline constexpr Var DoorScrollStatus{0x0057}; // #$01=Entered right hand door from horizontal area.
inline constexpr Var SamusDoorData{0x0058}; // The upper 4 bits store either 1 or 2. If 1 is stored(bit 4
inline constexpr Var DoorDelay{0x0059}; // Number of frames to delay when Samus entering/exiting doors.
inline constexpr Var RoomNumber{0x005A}; // Room number currently being loaded.
inline constexpr Var SpritePagePos{0x005B}; // Index into sprite RAM used to load object sprite data.
inline constexpr Var SamusInLava{0x0064}; // #$01=Samus in lava, #$00=She is not.
inline constexpr Var ObjectCounter{0x0065}; // Counts such things as object explosion time.
inline constexpr Var ObjectPal{0x0067}; // Attrib. table info for room object(#$00 thru #$03).
inline constexpr Var RoomPal{0x0068};
inline constexpr Var TempX{0x0069};
inline constexpr Var TempY{0x006A};
inline constexpr Var ObjectCntrl{0x006B}; // Controls object properties such as mirroring and color
inline constexpr Var DoorOnNameTable3{0x006C}; // The following two addresses are used to keep track of the
inline constexpr Var DoorOnNameTable0{0x006D}; // doors loaded on the name tables. The information is used
inline constexpr Var HealthLoChange{0x006E}; // Amount to add/subtract from HealthLo.
inline constexpr Var HealthHiChange{0x006F}; // Amount to add/subtract from HealthHi.
inline constexpr Var SamusBlink{0x0070};
inline constexpr Var UpdtngPrjctl{0x0071}; // #$01=Projectile update in process. #$00=not in process.
inline constexpr Var DmgPushDir{0x0072}; // #$00=Push Samus left when hit, #$01=Push right, #$FF=No push.
inline constexpr Var InArea{0x0074}; // #$10(or #$00)=Brinstar, #$11=Norfair, #$12=Kraid hideout,
inline constexpr Var SpareMem75{0x0075}; // Initialized to #$FF in AreaInit. Not used.
inline constexpr Var PalToggle{0x0076};
inline constexpr Var ItemRmMusicSts{0x0079}; // #$00=Item room music not playing.
inline constexpr Var OnFrozenEnemy{0x007D}; // #$01=Samus standing on frozen enemy, #$00=she is not.
// --- End routine specific
inline constexpr Var EndMsgWrite{0x007A}; // 0=don't write end message, 1=write end message.
inline constexpr Var IsCredits{0x007B}; // 0=credits not rolling, 1=credits rolling.
inline constexpr Var SpriteByteCntr{0x007C}; // Used to indicate when Samus sprite load complete.
inline constexpr Var SpritePtrIndex{0x007D}; // Index to proper Samus sprite graphics at end game.
inline constexpr Var SpriteAttribByte{0x007E}; // #$00.  Attribute byte of some sprites.
inline constexpr Var ColorCntIndex{0x007F}; // Index for finding count number for ClrChangeCounter.
inline constexpr Var CreditPageNumber{0x0080}; // Stores current page of credits(#$00 thru #$06).
inline constexpr Var HideShowEndMsg{0x0081}; // 0=show end message, 1=erase end message.
inline constexpr Var ClrChangeCounter{0x0082}; // When=#$00, change end Samus sprite colors.
inline constexpr Var WaveSpritePtr{0x0083}; // Address pointer to Samus hand waving sprites in end.
inline constexpr Var WaveSpriteCntr{0x0084}; // Stores length of wave sprite data (#$10).
inline constexpr Var MetroidOnSamus{0x0092}; // #$01=Metroid on Samus, #$00=Metroid not on Samus.
inline constexpr Var MaxMissilePickup{0x0093}; // Maximum missiles power-ups that can be picked up. Randomly
inline constexpr Var MaxEnergyPickup{0x0094}; // Maximum energy power-ups that can be picked up. Randomly
inline constexpr Var CrntMslePickups{0x0095}; // Number of missile power-ups currently collected by Samus
inline constexpr Var CrntEnrgyPickups{0x0096}; // Number of energy power-ups currently collected by Samus
inline constexpr Var MthrBrainStatus{0x0098}; // #$00=Mother brain not in room, #$01=Mother brain in room,
inline constexpr Var MotherBrainHits{0x0099}; // Number of times mother brain has been hit. Dies at #$20.
inline constexpr Var SpareMemB7{0x00B7}; // Written to in title routine and accessed by unsed routine.
inline constexpr Var SpareMemB8{0x00B8}; // Written to in title routine and accessed by unsed routine.
inline constexpr Var SpareMemBB{0x00BB}; // Written to in title routine, but never accessed.
inline constexpr Var First4SlowCntr{0x00BC}; // This address holds an 8 frame delay. when the delay is up,
inline constexpr Var Second4Delay{0x00BD}; // This address holds a 32 frame delay.  When the delay is
inline constexpr Var ScndCrshrSprts{0x00BF}; // #$01=Second crosshair sprites active in intro.
inline constexpr Var FlashScreen{0x00C0}; // #$01=Flash screen during crosshairs routine.
inline constexpr Var PalDataIndex{0x00C1};
inline constexpr Var ScrnFlashPalInd{0x00C2}; // Index to palette data to flash screen during intro.
inline constexpr Var IntroStarOffset{0x00C3}; // Contains offset into IntroStarPntr table for twinkle effect.
inline constexpr Var FadeDataIndex{0x00C4}; // Index to palette data to fade items in and out during intro.
inline constexpr Var SpareMemC5{0x00C5}; // Written to in title routine, but never accessed.
inline constexpr Var CrossDataIndex{0x00C6}; // #$00 thru #$04. Index to find cross sprite data.
inline constexpr Var DrawCross{0x00C7}; // #$01=Draw cross on screen during crosshairs routine.
inline constexpr Var SpriteLoadPend{0x00C8}; // Set to #$00 after sprite RAM load complete.
inline constexpr Var SpareMemC9{0x00C9}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemCB{0x00CB}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemCC{0x00CC}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemCD{0x00CD}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemCE{0x00CE}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemCF{0x00CF}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemD0{0x00D0}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemD1{0x00D1}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemD2{0x00D2}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemD3{0x00D3}; // Written to in title routine, but never accessed.
inline constexpr Var SpareMemD7{0x00D7}; // Written to in title routine, but never accessed.
inline constexpr Var IntroMusRstrt{0x00D8}; // After all title routines run twice, restarts intro music.
inline constexpr Var SFXPtrE0{0x00E0}; // Pointer used by SFX routines.
inline constexpr Var SFXPtrE0LB{0x00E0}; // Pointer used by SFX routines, lower byte.
inline constexpr Var SFXPtrE0UB{0x00E1}; // Pointer used by SFX routines, upper byte.
inline constexpr Var SFXPtrE2_{0x00E2}; // Pointer used by SFX routines.
inline constexpr Var SFXPtrE2{0x00E2}; // Pointer used by SFX routines.
inline constexpr Var SFXPtrE2LB{0x00E2}; // Pointer used by SFX routines, lower byte.
inline constexpr Var SFXPtrE2UB{0x00E3}; // Pointer used by SFX routines, upper byte.
inline constexpr Var SFXPtrE4{0x00E4}; // Pointer used by SFX routines.
inline constexpr Var SFXPtrE4LB{0x00E4}; // Pointer used by SFX routines, lower byte.
inline constexpr Var SFXPtrE4UB{0x00E5}; // Pointer used by SFX routines, upper byte.
inline constexpr Var ABStatus{0x00F0}; // Stores A and B button status in AreaInit. Never used.
inline constexpr Var MirrorCntrl{0x00FA}; // If bit 3 is set, PPU set to horizontal mirroring
inline constexpr Var ScrollY{0x00FC}; // Y value loaded into scroll register.
inline constexpr Var ScrollX{0x00FD}; // X value loaded into scroll register.
inline constexpr Var PPUCNT1ZP{0x00FE}; // Data byte to be loaded into PPU control register 1.
inline constexpr Var PPUCNT0ZP{0x00FF}; // Data byte to be loaded into PPU control register 0.
inline constexpr Var HealthLo{0x0106}; // Lower health digit in upper 4 bits.
inline constexpr Var HealthHi{0x0107}; // Upper health digit in lower 4 bits
inline constexpr Var MiniBossKillDly{0x0108}; // Initiate power up music and delay after Kraid/Ridley killed.
inline constexpr Var PowerUpDelay{0x0109}; // Initiate power up music and delay after item pickup.
inline constexpr Var EndTimerLo{0x010A}; // Lower byte of end game escape timer.
inline constexpr Var EndTimerHi{0x010B}; // Upper byte of end game escape timer.
inline constexpr Var MissileToggle{0x010E}; // 0=fire bullets, 1=fire missiles.
inline constexpr Var SpriteRAM{0x0200}; // Through $02FF. Sprite RAM.
// --- Object RAM
inline constexpr Var ObjAction{0x0300}; // Status of object. 0=object slot not in use.
inline constexpr Var ObjRadY{0x0301}; // Distance in pixels from object center to top or bottom.
inline constexpr Var ObjRadX{0x0302}; // Distance in pixels from object center to left or right side.
inline constexpr Var AnimFrame{0x0303}; // *2 = Index into FramePtrTable for current animation.
inline constexpr Var AnimDelay{0x0304}; // Number of frames to delay between animation frames.
inline constexpr Var AnimResetIndex{0x0305}; // Restart index-1 when AnimIndex finished with last frame.
inline constexpr Var AnimIndex{0x0306}; // Current index into ObjectAnimIndexTbl.
inline constexpr Var SamusOnElevator{0x0307}; // 0=Samus not on elevator, 1=Samus on elevator.
inline constexpr Var ObjVertSpeed{0x0308}; // MSB set=moving up(#$FA max), MSB clear=moving down(#$05 max).
inline constexpr Var ObjHorzSpeed{0x0309}; // MSB set=moving lft(#$FE max), MSB clear=moving rt(#$01 max).
inline constexpr Var SamusHit{0x030A}; // Samus hit by enemy.
inline constexpr Var ObjectOnScreen{0x030B}; // 1=Object on screen, 0=Object beyond screen boundaries.
inline constexpr Var ObjectHi{0x030C}; // 0=Object on nametable 0, 1=Object on nametable 3.
inline constexpr Var ObjectY{0x030D}; // Object y position in room(not actual screen position).
inline constexpr Var ObjectX{0x030E}; // Object x position in room(not actual screen position).
inline constexpr Var SamusJmpDsplcmnt{0x030F}; // Number of pixels vertically displaced from jump point.
inline constexpr Var VertCntrNonLinr{0x0310}; // Verticle movement counter. Exponential change in speed.
inline constexpr Var HorzCntrNonLinr{0x0311}; // Horizontal movement counter. Exponential change in speed.
inline constexpr Var VertCntrLinear{0x0312}; // Verticle movement counter. Linear change in speed.
inline constexpr Var HorzCntrLinear{0x0313}; // Horizontal movement counter. Linear change in speed.
inline constexpr Var SamusGravity{0x0314}; // Value used in calculating vertical acceleration on Samus.
inline constexpr Var SamusHorzAccel{0x0315}; // Value used in calculating horizontal acceleration on Samus.
inline constexpr Var SamusHorzSpdMax{0x0316}; // Used to calc maximum horizontal speed Samus can reach.
inline constexpr Var ElevatorStatus{0x0320}; // #$01=Elevator present, #$00=Elevator not present.
inline constexpr Var PowerUpAnimFrame{0x0343}; // *2 = Index into FramePtrTable for current animation.
inline constexpr Var PowerUpHi{0x034C}; // Name table power up item is located on.
inline constexpr Var PowerUpY{0x034D}; // Room Y coord of power up item.
inline constexpr Var PowerUpX{0x034E}; // Room x coord of power up item.
// --- Title routine specific
inline constexpr Var PasswordCursor{0x0320}; // Password write position (#$00 - #$17).
inline constexpr Var InputRow{0x0321}; // Password character select row (#$00 - #$04).
inline constexpr Var InputColumn{0x0322}; // Password character select column (#$00 - #$0C).
inline constexpr Var PasswordStat00{0x0324}; // Does not appear to have a function.
inline constexpr Var StartContinue{0x0325}; // 0=START selected, 1=CONTINUE selected.
// --- Enemy RAM
inline constexpr Var EnYRoomPos{0x0400}; // Enemy y position in room.(not actual screen position).
inline constexpr Var EnXRoomPos{0x0401}; // Enemy x position in room.(not actual screen position).
inline constexpr Var EnCounter{0x0406}; // Counts such things as explosion time.
inline constexpr Var EnDelay{0x0409}; // Delay counter between enemy actions.
inline constexpr Var EnHitPoints{0x040B}; // Current hit points of enemy.
inline constexpr Var EnSpecialAttribs{0x040F}; // Bit 7 set=tough version of enemy, bit 6 set=mini boss.
inline constexpr Var TileRoutine{0x0500};
inline constexpr Var TileAnimFrame{0x0503};
inline constexpr Var TileAnimDelay{0x0504};
inline constexpr Var TileAnimIndex{0x0506};
inline constexpr Var TileDelay{0x0507};
inline constexpr Var TileWRAMLo{0x0508};
inline constexpr Var TileWRAMHi{0x0509};
inline constexpr Var TileType{0x050A};
// --- Sound Engine Memory Addresses
inline constexpr Var Cntrl0Data{0x00EA}; // Temp storage for data of first address sound channel
inline constexpr Var VolCntrlAddress{0x00EB}; // Desired address number in VolumeCntrlAdressTbl
inline constexpr Var MusicSQ1PrdLow{0x0600}; // Loaded into SQ1Cntrl2 when playing music
inline constexpr Var MusicSQ1PrdHi{0x0601}; // Loaded into SQ1Cntrl3 when playing music
inline constexpr Var SFXPaused{0x0602}; // 0=Game not paused, 1=Game paused
inline constexpr Var PauseSFXStatus{0x0603}; // Plays PauseMusic SFX if less than #$12
inline constexpr Var MusicSQ2PeriodLo{0x0604}; // Loaded into SQ2Cntrl2 when playing music
inline constexpr Var MusicSQ2PeriodHi{0x0605}; // Loaded into SQ2Cntrl3 when playing music
inline constexpr Var WrtMultiChnDat{0x0607}; // 1=data needs to be written, 0=no data to write
inline constexpr Var MusTriPeriodLo{0x0608}; // Loaded into TriangleCntrl2 when playing music
inline constexpr Var MusTriPeriodHi{0x0609}; // Loaded into TriangleCntrl3 when playing music
inline constexpr Var TriPeriodLow{0x0610}; // Stores triangle SFX period low for processing
inline constexpr Var TriPeriodHigh{0x0611}; // Stroes triangle SFX period high for processing
inline constexpr Var TriChangeLow{0x0612}; // Stores triangle SFX change in period low
inline constexpr Var TriChangeHigh{0x0613}; // Stores triangle SFX change in period high
inline constexpr Var TriLoPercentage{0x0614}; // Stores percent to change period low by each frame
inline constexpr Var TriHiPercentage{0x0615}; // Stores percent to change period high by each frame
inline constexpr Var PercentDiff{0x0616}; // if=5, percent=1/5(20%), if=0A, percent=1/10(10%), etc
inline constexpr Var DivideData{0x0617}; // Used in DivideTrianglePeriods
inline constexpr Var HasBeamSFX{0x061F}; // Bit 7 set=has long beam, bit 0 set=has ice beam
inline constexpr Var SQ1FrmCountInit{0x0620}; // Holds number of frames to play sq1 channel data
inline constexpr Var SQ2FrmCountInit{0x0621}; // Holds number of frames to play sq2 channel data
inline constexpr Var TriFrmCountInit{0x0622}; // Holds number of frames to play triangle channel data
inline constexpr Var NseFrmCountInit{0x0623}; // Holds number of frames to play noise channel data
inline constexpr Var SQ1RepeatCounter{0x0624}; // Number of times to repeat SQ1 music loop
inline constexpr Var SQ2RepeatCounter{0x0625}; // Number of times to repeat SQ2 music loop
inline constexpr Var TriRepeatCounter{0x0626}; // Number of times to repeat Triangle music loop
inline constexpr Var NseRepeatCounter{0x0627}; // Number of times to repeat Noise music loop
inline constexpr Var SQ1DutyEnvelope{0x0628}; // Loaded into SQ1Cntrl0 when playing music
inline constexpr Var SQ2DutyEnvelope{0x0629}; // Loaded into SQ2Cntrl0 when playing music
inline constexpr Var TriLinearCount{0x062A}; // disable\enable counter, linear count length
inline constexpr Var NoteLenTblOffset{0x062B}; // Stores the offset to find proper note length table
inline constexpr Var MusicRepeat{0x062C}; // 0=Music does not repeat, Nonzero=music repeats
inline constexpr Var TriCounterCntrl{0x062D}; // $F0=disable length cntr, $00=long note, $0F=short note
inline constexpr Var SQ1VolumeCntrl{0x062E}; // Entry number in VolumeCntrlAdressTbl for SQ1
inline constexpr Var SQ2VolumeCntrl{0x062F}; // Entry number in VolumeCntrlAdressTbl for SQ2
inline constexpr Var SQ1LowBaseByte{0x0630}; // low byte of base address for SQ1 music data
inline constexpr Var SQ1HighBaseByte{0x0631}; // High byte of base address for SQ1 music data
inline constexpr Var SQ2LowBaseByte{0x0632}; // low byte of base address for SQ2 music data
inline constexpr Var SQ2HighBaseByte{0x0633}; // High byte of base address for SQ2 music data
inline constexpr Var TriLowBaseByte{0x0634}; // low byte of base address for Triangle music data
inline constexpr Var TriHighBaseByte{0x0635}; // High byte of base address for Triangle music data
inline constexpr Var NoiseLowBaseByte{0x0636}; // low byte of base address for Noise music data
inline constexpr Var NseHighBaseByte{0x0637}; // High byte of base address for Noise music data
inline constexpr Var SQ1MusicIdxIdx{0x0638}; // Index to find sQ1 sound data index. Base=$630,$631
inline constexpr Var SQ2MusicIdxIdx{0x0639}; // Index to find SQ2 sound data index. Base=$632,$633
inline constexpr Var TriMusicIdxIdx{0x063A}; // Index to find Tri sound data index. Base=$634,$635
inline constexpr Var NoiseMusicIdxIdx{0x063B}; // Index to find Noise sound data index. Base=$636,$637
inline constexpr Var SQ1LoopIndex{0x063C}; // SQ1 Loop start index
inline constexpr Var SQ2LoopIndex{0x063D}; // SQ2 loop start index
inline constexpr Var TriangleLoopIdx{0x063E}; // Triangle loop start index
inline constexpr Var NoiseLoopIndex{0x063F}; // Noise loop start index
inline constexpr Var SQ1MusicFrameCnt{0x0640}; // Decrements every sq1 frame. When 0, load new data
inline constexpr Var SQ2MusicFrameCnt{0x0641}; // Decrements every sq2 frame. when 0, load new data
inline constexpr Var TriMusicFrameCnt{0x0642}; // Decrements every triangle frame. When 0, load new data
inline constexpr Var NseMusicFrameCnt{0x0643}; // Decrements every noise frame. When 0, load new data
inline constexpr Var MusicSQ1Sweep{0x0648}; // Value is loaded into SQ1Cntrl1 when playing music
inline constexpr Var MusicSQ2Sweep{0x0649}; // Value is loaded into SQ2Cntrl1 when playing music
inline constexpr Var TriangleSweep{0x064A}; // Loaded into TriangleCntrl1(not used)
inline constexpr Var ThisSoundChannel{0x064B}; // Least sig. byte of current channel(00,04,08 or 0C)
inline constexpr Var CurrentSFXFlags{0x064D}; // Stores flags of SFX currently being processed.
inline constexpr Var NoiseInUse{0x0652}; // Noise in use? (Not used)
inline constexpr Var SQ1InUse{0x0653}; // 1=SQ1 channel being used by SFX, 0=not in use
inline constexpr Var SQ2InUse{0x0654}; // 2=SQ2 channel being used by SFX, 0=not in use
inline constexpr Var TriangleInUse{0x0655}; // 3=Triangle channel being used by SFX, 0=not in use
inline constexpr Var ChannelType{0x065C}; // Stores channel type being processed(0,1,2,3 or 4)
inline constexpr Var CrntMusicRepeat{0x065D}; // Stores flags of music to repeat
inline constexpr Var MusicInitIndex{0x065E}; // index for loading $62B thru $637(base=$BD31).
inline constexpr Var NoiseSFXLength{0x0660}; // Stores number of frames to play Noise SFX
inline constexpr Var SQ1SFXLength{0x0661}; // Stores number of frames to play SQ1 SFX
inline constexpr Var SQ2SFXLngth{0x0662}; // Stores number of frames to play SQ2 SFX
inline constexpr Var TriangleSFXLngth{0x0663}; // Stores number of frames to play Triangle SFX
inline constexpr Var MultiSFXLength{0x0664}; // Stores number of frames to play Multi SFX
inline constexpr Var ThisNoiseFrame{0x0665}; // Stores current frame number for noise SFX
inline constexpr Var ThisSQ1Frame{0x0666}; // Stores current frame number for sq1 SFX
inline constexpr Var ThisSQ2Frame{0x0667}; // Stores current frame number for SQ2 SFX
inline constexpr Var ThisTriFrame{0x0668}; // Stores current frame number for triangle SFX
inline constexpr Var ThisMultiFrame{0x0669}; // Stores current frame number for Multi SFX
inline constexpr Var SQ1VolumeIndex{0x066A}; // Stores index to SQ1 volume data in a volume data tbl
inline constexpr Var SQ2VolumeIndex{0x066B}; // Stores index to SQ2 volume data in a volume data tbl
inline constexpr Var SQ1VolumeData{0x066C}; // stores duty cycle and this frame volume data of SQ1
inline constexpr Var SQ2VolumeData{0x066D}; // Stores duty cycle and this frame volume data of SQ2
inline constexpr Var NoiseSFXData{0x0670}; // Stores additional info for Noise SFX
inline constexpr Var SQ1SFXData{0x0671}; // Stores additional info for SQ1 SFX
inline constexpr Var SQ2SFXData{0x0672}; // Stores additional info for SQ2 SFX
inline constexpr Var TriangleSFXData{0x0673}; // Stores additional info for triangle SFX
inline constexpr Var MultiSFXData{0x0674}; // Stores additional info for Multi SFX
inline constexpr Var SQ1SQ2SFXData{0x0675}; // Stores additional info for SQ1 and SQ2 SFX
inline constexpr Var ScrewAtkSFXData{0x0678}; // Contains extra data for screw attack SFX
inline constexpr Var SQ1SFXPeriodLow{0x0679}; // Period low data for processing multi SFX routines
inline constexpr Var NoiseSFXFlag{0x0680}; // Initialization flags for noise SFX
inline constexpr Var SQ1SFXFlag{0x0681}; // Initialization flags for SQ1 SFX
inline constexpr Var SQ2SFXFlag{0x0682}; // Initialization flags for SQ2 SFX(never used)
inline constexpr Var TriangleSFXFlag{0x0683}; // Initialization flags for triangle SFX
inline constexpr Var MultiSFXFlag{0x0684}; // Initialization Flags for SFX and some music
inline constexpr Var MusicInitFlag{0x0685}; // Music init flags
inline constexpr Var NoiseContSFX{0x0688}; // Continuation flags for noise SFX
inline constexpr Var SQ1ContSFX{0x0689}; // Continuation flags for SQ1 SFX
inline constexpr Var SQ2ContSFX{0x068A}; // Continuation flags for SQ2 SFX (never used)
inline constexpr Var TriangleContSFX{0x068B}; // Continuation flags for Triangle SFX
inline constexpr Var MultiContSFX{0x068C}; // Continuation flags for Multi SFX
inline constexpr Var CurrentMusic{0x068D}; // Stores the flag of the current music being played
inline constexpr Var PowerUpType{0x0748}; // Holds the byte describing what power-up is on name table.
inline constexpr Var PowerUpYCoord{0x0749}; // Y coordinate of the power-up.
inline constexpr Var PowerUpXCoord{0x074A}; // X coordiante of the power-up
inline constexpr Var PowerUpNameTable{0x074B}; // #$00 if on name table 0, #$01 if on name table 3.
inline constexpr Var PowerUpAnimIndex{0x074F}; // Entry into FramePtrTable for item animation.
inline constexpr Var PowerUpBType{0x0750}; // Holds the description byte of a second power-up(if any).
inline constexpr Var PowerUpBYCoord{0x0751}; // Y coordinate of second power-up.
inline constexpr Var PowerUpBXCoord{0x0752}; // X coordiante of second power-up.
inline constexpr Var PowerUpBNameTbl{0x0753}; // #$00 if on name table 0, #$01 if on name table 3.
inline constexpr Var PowerUpBAnimIdx{0x0757}; // Entry into FramePtrTable for item animation.
inline constexpr Var TileSize{0x0780}; // 4 MSBs=Y size of tile to erase.4 LSBs=X size of tile to erase.
inline constexpr Var TileInfo0{0x0781};
inline constexpr Var TileInfo1{0x0782};
inline constexpr Var TileInfo2{0x0783}; // Tile patterns to replace blasted tiles.
inline constexpr Var TileInfo3{0x0784};
inline constexpr Var TileInfo4{0x0785};
inline constexpr Var TileInfo5{0x0786};
inline constexpr Var PPUStrIndex{0x07A0}; // # of bytes of data in PPUDataString. #$4F bytes max.
inline constexpr Var PPUDataString{0x07A1}; // Thru $07F0. String of data bytes to be written to PPU.
// --- Hardware Defines
inline constexpr Var RoomRAMA{0x6000}; // Thru $63FF. Used to load room before it is put into the PPU.
inline constexpr Var RoomRAMB{0x6400}; // Thru $67FF. Used to load room before it is put into the PPU.
inline constexpr Var EndingType{0x6872}; // 1=worst ending, 5=best ending
inline constexpr Var SamusDataIndex{0x6875}; // Index for Samus saved game stats(not used). #$00, #$10, #$20.
inline constexpr Var SamusStat00{0x6876}; // Unused memory address for storing Samus info.
inline constexpr Var TankCount{0x6877}; // Number of energy tanks.
inline constexpr Var SamusGear{0x6878}; // Stores power-up items Samus has.
inline constexpr Var MissileCount{0x6879}; // Stores current number of missiles.
inline constexpr Var MaxMissiles{0x687A}; // Maximum amount of missiles Samus can carry
inline constexpr Var KraidStatueStat{0x687B}; // bit 0 set, the statues blink,
inline constexpr Var RidlyStatueStat{0x687C}; // bit 7 set, statues are up.
inline constexpr Var SamusAgeLo{0x687D}; // Low byte of Samus' age.
inline constexpr Var SamusAgeMid{0x687E}; // Mid byte of Samus' age.
inline constexpr Var SamusAgeHi{0x687F}; // High byte of Samus' age.
inline constexpr Var SamusStat01{0x6880}; // Unused memory address for storing Samus info.
inline constexpr Var SamusStat02{0x6881}; // SamusStat02 and 03 keep track of how many times Samus has
inline constexpr Var SamusStat03{0x6882}; // died, but this info is never accessed anywhere in the game.
inline constexpr Var AtEnding{0x6883}; // 1=End scenes playing, 0=Not at ending.
inline constexpr Var EraseGame{0x6884}; // MSB set=erase selected saved game(not used in password carts).
inline constexpr Var DataSlot{0x6885}; // #$00 thru #$02. Stored Samus data to load. Apparently a save
inline constexpr Var NumUniqueItems{0x6886}; // Counts number of power-ups and red doors
inline constexpr Var UnqItmHist{0x6887}; // Thru $68FC. History of Unique items collected.
inline constexpr Var EndItemHistory{0x68FC}; // Two bytes per item.
inline constexpr Var KrdRdlyPresent{0x6987}; // #$01=Kraid/Ridley present, #$00=Kraid/Ridley not present.
inline constexpr Var PasswordByte00{0x6988}; // Stores status of items 0 thru 7.
inline constexpr Var PasswordByte01{0x6989}; // Stores status of items 8 thru 15.
inline constexpr Var PasswordByte02{0x698A}; // Stores status of items 16 thru 23.
inline constexpr Var PasswordByte03{0x698B}; // Stores status of items 24 thru 31.
inline constexpr Var PasswordByte04{0x698C}; // Stores status of items 32 thru 39.
inline constexpr Var PasswordByte05{0x698D}; // Stores status of items 40 thru 47.
inline constexpr Var PasswordByte06{0x698E}; // Stores status of items 48 thru 55.
inline constexpr Var PasswordByte07{0x698F}; // Stores status of items 56 thru 58(bits 0 thru 2).
inline constexpr Var PasswordByte08{0x6990}; // start location(bits 0 thru 5), Samus suit status (bit 7).
inline constexpr Var PasswordByte09{0x6991}; // Stores SamusGear.
inline constexpr Var PasswordByte0A{0x6992}; // Stores MissileCount.
inline constexpr Var PasswordByte0B{0x6993}; // Stores SamusAgeLo.
inline constexpr Var PasswordByte0C{0x6994}; // Stores SamusAgeMid.
inline constexpr Var PasswordByte0D{0x6995}; // Stores SamusAgeHi.
inline constexpr Var PasswordByte0E{0x6996}; // Stores no data.
inline constexpr Var PasswordByte0F{0x6997}; // Stores Statue statuses(bits 4 thu 7).
inline constexpr Var PasswordByte10{0x6998}; // Stores value RandomNumber1.
inline constexpr Var PasswordByte11{0x6999}; // Stores sum of $6988 thru $6998(Checksum).
inline constexpr Var PasswordChar00{0x699A};
inline constexpr Var PasswordChar01{0x699B};
inline constexpr Var PasswordChar02{0x699C};
inline constexpr Var PasswordChar03{0x699D};
inline constexpr Var PasswordChar04{0x699E};
inline constexpr Var PasswordChar05{0x699F};
inline constexpr Var PasswordChar06{0x69A0};
inline constexpr Var PasswordChar07{0x69A1};
inline constexpr Var PasswordChar08{0x69A2};
inline constexpr Var PasswordChar09{0x69A3};
inline constexpr Var PasswordChar0A{0x69A4}; // These 18 memory addresses store the 18 characters
inline constexpr Var PasswordChar0B{0x69A5}; // of the password to be displayed on the screen.
inline constexpr Var PasswordChar0C{0x69A6};
inline constexpr Var PasswordChar0D{0x69A7};
inline constexpr Var PasswordChar0E{0x69A8};
inline constexpr Var PasswordChar0F{0x69A9};
inline constexpr Var PasswordChar10{0x69AA};
inline constexpr Var PasswordChar11{0x69AB};
inline constexpr Var PasswordChar12{0x69AC};
inline constexpr Var PasswordChar13{0x69AD};
inline constexpr Var PasswordChar14{0x69AE};
inline constexpr Var PasswordChar15{0x69AF};
inline constexpr Var PasswordChar16{0x69B0};
inline constexpr Var PasswordChar17{0x69B1};
inline constexpr Var NARPASSWORD{0x69B2}; // 0 = invinsible Samus not active, 1 = invinsible Samus active.
inline constexpr Var JustInBailey{0x69B3}; // 0 = Samus has suit, 1 = Samus is without suit.
inline constexpr Var ItmeHistory{0x69B4}; // Thru $6A73. Unique item history saved game data (not used).
// --- More enemy RAM
inline constexpr Var Enstatus{0x6AF4}; // Keeps track of enemy statuses. #$00=Enemy slot not in use,
inline constexpr Var EnRadY{0x6AF5}; // Distance in pixels from middle of enemy to top or botom.
inline constexpr Var EnRadX{0x6AF6}; // Distance in pixels from middle of enemy to left or right.
inline constexpr Var EnAnimFrame{0x6AF7}; // Index into enemy animation frame data.
inline constexpr Var EnAnimDelay{0x6AF8}; // Number of frames to delay between animation frames.
inline constexpr Var EnResetAnimIndex{0x6AF9}; // Index to beginning of animation sequence.
inline constexpr Var EnAnimIndex{0x6AFA}; // Index to current animation.
inline constexpr Var EnNameTable{0x6AFB}; // #$00=Enemy on name table 0, #$01=Enemy on name table 3.
inline constexpr Var EnDataIndex{0x6B02}; // Contains index into enemy data tables.
inline constexpr Var IntroStrSprt00{0x6E00}; // thru $6E9F. RAM used for storing intro star sprite data.
inline constexpr Var IntroSpr0YCoord{0x6EA0}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr0PattTbl{0x6EA1}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr0Cntrl{0x6EA2}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr0XCoord{0x6EA3}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr0Index{0x6EA4}; // Index to next sparkle sprite data byte.
inline constexpr Var IntroSpr0NxtCntr{0x6EA5}; // Decrements each frame. When 0, load new sparkle sprite data.
inline constexpr Var SprklSpr0YChange{0x6EA6}; // Sparkle sprite y coordinate change.
inline constexpr Var IntroSpr0XChange{0x6EA6}; // Intro sprite x total movement distance.
inline constexpr Var SprklSpr0XChange{0x6EA7}; // Sparkle sprite x coordinate change.
inline constexpr Var IntroSpr0YChange{0x6EA7}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr0ChngCnt{0x6EA8}; // decrements each frame from #$20. At 0, change sparkle sprite.
inline constexpr Var IntroSpr0ByteTyp{0x6EA9}; // #$00 or #$01. When #$01, next sparkle data byte uses all 8
inline constexpr Var IntroSpr0Comp{0x6EAA}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr0SpareB{0x6EAB}; // Not used.
inline constexpr Var IntroSpr0XRun{0x6EAC}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr0YRise{0x6EAD}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr0XDir{0x6EAE}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr0YDir{0x6EAF}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr1YCoord{0x6EB0}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr1PattTbl{0x6EB1}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr1Cntrl{0x6EB2}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr1XCoord{0x6EB3}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr1Index{0x6EB4}; // Index to next sparkle sprite data byte.
inline constexpr Var IntroSpr1NextCnt{0x6EB5}; // Decrements each frame. When 0, load new sparkle sprite data.
inline constexpr Var SparkleSpr1YChng{0x6EB6}; // Sparkle sprite y coordinate change.
inline constexpr Var IntroSpr1XChange{0x6EB6}; // Intro sprite x total movement distance.
inline constexpr Var SparkleSpr1XChng{0x6EB7}; // Sparkle sprite x coordinate change.
inline constexpr Var IntroSpr1YChange{0x6EB7}; // Intro sprite y total movement distance.
inline constexpr Var IntroSp1ChngCntr{0x6EB8}; // decrements each frame from #$20. At 0, change sparkle sprite.
inline constexpr Var IntroSpr1BtType{0x6EB9}; // #$00 or #$01. When #$01, next sparkle data byte uses all 8
inline constexpr Var IntroSpr1Comp{0x6EBA}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr1SpareB{0x6EBB}; // Not used.
inline constexpr Var IntroSpr1XRun{0x6EBC}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr1YRise{0x6EBD}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr1XDir{0x6EBE}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr1YDir{0x6EBF}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr2YCoord{0x6EC0}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr2PattTbl{0x6EC1}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr2Cntrl{0x6EC2}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr2XCoord{0x6EC3}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr2Spare5{0x6EC4}; // Not used.
inline constexpr Var IntroSpr2Spare6{0x6EC5}; // Not used.
inline constexpr Var IntroSpr2XChange{0x6EC6}; // Intro sprite x total movement distance.
inline constexpr Var IntroSpr2YChange{0x6EC7}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr2Spare8{0x6EC8}; // Not used.
inline constexpr Var IntroSpr2Spare9{0x6EC9}; // Not used.
inline constexpr Var IntroSpr2Comp{0x6ECA}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr2SpareB{0x6ECB}; // Not used.
inline constexpr Var IntroSpr2XRun{0x6ECC}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr2YRise{0x6ECD}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr2XDir{0x6ECE}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr2YDir{0x6ECF}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr3YCoord{0x6ED0}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr3PattTbl{0x6ED1}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr3Cntrl{0x6ED2}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr3XCoord{0x6ED3}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr3Spare5{0x6ED4}; // Not used.
inline constexpr Var IntroSpr3Spare6{0x6ED5}; // Not used.
inline constexpr Var IntroSpr3XChange{0x6ED6}; // Intro sprite x total movement distance.
inline constexpr Var IntroSpr3YChange{0x6ED7}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr3Spare8{0x6ED8}; // Not used.
inline constexpr Var IntroSpr3Spare9{0x6ED9}; // Not used.
inline constexpr Var IntroSpr3Comp{0x6EDA}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr3SpareB{0x6EDB}; // Not used.
inline constexpr Var IntroSpr3XRun{0x6EDC}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr3YRise{0x6EDD}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr3XDir{0x6EDE}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr3YDir{0x6EDF}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr4YCoord{0x6EE0}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr4PattTbl{0x6EE1}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr4Cntrl{0x6EE2}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr4XCoord{0x6EE3}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr4Spare5{0x6EE4}; // Not used.
inline constexpr Var IntroSpr4Spare6{0x6EE5}; // Not used.
inline constexpr Var IntroSpr4XChange{0x6EE6}; // Intro sprite x total movement distance.
inline constexpr Var IntroSpr4YChange{0x6EE7}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr4Spare8{0x6EE8}; // Not used.
inline constexpr Var IntroSpr4Spare9{0x6EE9}; // Not used.
inline constexpr Var IntroSpr4Comp{0x6EEA}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr4SpareB{0x6EEB}; // Not used.
inline constexpr Var IntroSpr4XRun{0x6EEC}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr4YRise{0x6EED}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr4XDir{0x6EEE}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr4YDir{0x6EEF}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr5YCoord{0x6EF0}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr5PattTbl{0x6EF1}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr5Cntrl{0x6EF2}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr5XCoord{0x6EF3}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr5Spare5{0x6EF4}; // Not used.
inline constexpr Var IntroSpr5Spare6{0x6EF5}; // Not used.
inline constexpr Var IntroSpr5XChange{0x6EF6}; // Intro sprite x total movement distance.
inline constexpr Var IntroSpr5YChange{0x6EF7}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr5Spare8{0x6EF8}; // Not used.
inline constexpr Var IntroSpr5Spare9{0x6EF9}; // Not used.
inline constexpr Var IntroSpr5Comp{0x6EFA}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr5SpareB{0x6EFB}; // Not used.
inline constexpr Var IntroSpr5XRun{0x6EFC}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr5YRise{0x6EFD}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr5XDir{0x6EFE}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr5YDir{0x6EFF}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr6YCoord{0x6F00}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr6PattTbl{0x6F01}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr6Cntrl{0x6F02}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr6XCoord{0x6F03}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr6Spare5{0x6F04}; // Not used.
inline constexpr Var IntroSpr6Spare6{0x6F05}; // Not used.
inline constexpr Var IntroSpr6XChange{0x6F06}; // Intro sprite x total movement distance.
inline constexpr Var IntroSpr6YChange{0x6F07}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr6Spare8{0x6F08}; // Not used.
inline constexpr Var IntroSpr6Spare9{0x6F09}; // Not used.
inline constexpr Var IntroSpr6Comp{0x6F0A}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr6SpareB{0x6F0B}; // Not used.
inline constexpr Var IntroSpr6XRun{0x6F0C}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr6YRise{0x6F0D}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr6XDir{0x6F0E}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr6YDir{0x6F0F}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var IntroSpr7YCoord{0x6F10}; // Loaded into byte 0 of sprite RAM(Y position).
inline constexpr Var IntroSpr7PattTbl{0x6F11}; // Loaded into byte 1 of sprite RAM(Pattern table index).
inline constexpr Var IntroSpr7Cntrl{0x6F12}; // Loaded into byte 2 of sprite RAM(Control byte).
inline constexpr Var IntroSpr7XCoord{0x6F13}; // Loaded into byte 3 of sprite RAM(X position).
inline constexpr Var IntroSpr7Spare5{0x6F14}; // Not used.
inline constexpr Var IntroSpr7Spare6{0x6F15}; // Not used.
inline constexpr Var IntroSpr7XChange{0x6F16}; // Intro sprite x total movement distance.
inline constexpr Var IntroSpr7YChange{0x6F17}; // Intro sprite y total movement distance.
inline constexpr Var IntroSpr7Spare8{0x6F18}; // Not used.
inline constexpr Var IntroSpr7Spare9{0x6F19}; // Not used.
inline constexpr Var IntroSpr7Comp{0x6F1A}; // #$01=sprite has completed its task, #$00 if not complete.
inline constexpr Var IntroSpr7SpareB{0x6F1B}; // Not used.
inline constexpr Var IntroSpr7XRun{0x6F1C}; // x displacement of sprite movement(run).
inline constexpr Var IntroSpr7YRise{0x6F1D}; // y displacement of sprite movement(rise).
inline constexpr Var IntroSpr7XDir{0x6F1E}; // MSB set=decrease sprite x pos, else increase sprite  x pos.
inline constexpr Var IntroSpr7YDir{0x6F1F}; // MSB set=decrease sprite y pos, else increase sprite  y pos.
inline constexpr Var WorldMapRAM{0x7000}; // Thru $73FF. The map is 1Kb in size (1024 bytes).
inline constexpr Var SamusData{0x77FE}; // Thru $782D. Samus saved game data (not used).
// --- MMC Registers

} // namespace Game::Vars
