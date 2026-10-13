#pragma once

// Routines of bank 7 (the game engine), named as in the community disassembly.

#include "game/Area.h"
#include "game/Collision.h"
#include "game/Enemies.h"
#include "game/Hud.h"
#include "game/Items.h"
#include "game/Scroll.h"
#include "game/Original.h"
#include "game/Projectiles.h"
#include "game/Room.h"
#include "game/Sound.h"
#include "game/Title.h"

#include <cstdint>

namespace Game {

uint8_t RandomNumbers();
void GoMainRoutine();
void ChooseRoutineEffects(uint16_t table, uint8_t index);
void UpdateTimer();
void CheckSwitch();
uint8_t ROMSwitch(uint8_t bank);
uint8_t MMCWriteReg3(uint8_t value);
void GoBankInit(uint8_t bank);

void ClearNameTables();
void ClearNameTable0();
void ClearNameTable();
void RemIntroSprts();
void ClearRAM_33_DF();
void ReadOnePad(int pad);
void AddYToPtr00(uint8_t y);
void AddYToPtr02(uint8_t y);
void ProcessPPUStr();
void SetPPUInc(bool by32);
bool WritePPUByte(uint8_t& x, uint8_t value);
bool NextPPUByte(uint8_t& x);
void EndPPUString(uint8_t x);
void EraseTile();
void PrepPPUPalStr();
uint8_t TwosCompliment(uint8_t a);
uint8_t Base10Add(uint8_t a, bool& carry);
uint8_t Base10Subtract(uint8_t a, bool& carry);
void WaitNMIPass();
void ScreenOff();
void WriteAndWait(uint8_t mask);
void ScreenOn();
void ScreenNmiOff();
void VBOffAndHorzWr();
void NmiOn();
void SetTimer(uint8_t tenths, uint8_t nextRoutine);
void SetPPUMirror(uint8_t mirrorCntrl);
void PrepPPUMirror();
void InitBank0();
void InitBank1();
void InitBank2();
void InitBank3();
void InitBank4();
void InitBank5();
void ClearSamusStats();
void InitEndGFX();
void InitTitleGFX();
void LoadSamusGFX();
void InitBrinstarGFX();
void InitNorfairGFX();
void InitTourianGFX();
void InitKraidGFX();
void InitRidleyGFX();
void InitGFX6();
void InitGFX7();
void LoadGFX(uint8_t entry);
void CopyGFXBlock();
void CopyPtrs();
uint8_t DestroyEnemies();
void IsEngineRunning(uint8_t area);
void SwitchBank(uint8_t area);
void ChooseEnding();
void ClearScreenData();
void UpdateWorld();
void SelectSamusPal();
void CheckMissileToggle();
uint8_t SFX_SetSoundFlag(uint8_t flags, uint8_t x);
uint8_t SilenceMusic();
uint8_t PauseMusic();
uint8_t SFXSamusWalk();
uint8_t SFXBombExplode();
uint8_t SFXMissileLaunch();
uint8_t SFXEnemyRegen();
uint8_t SFXBombLaunch();
uint8_t SFX_SamusJump();
uint8_t SFX_EnemyHit();
uint8_t SFX_BulletFire();
uint8_t SFXMetal();
uint8_t SFX_EnergyPickup();
uint8_t SFX_MissilePickup();
uint8_t SFX_WaveFire();
uint8_t SFX_ScrewAttack();
uint8_t SFX_BigEnemyHit();
uint8_t SFX_MetroidHit();
uint8_t SFX_BossHit();
uint8_t SFXDoor();
uint8_t SFX_SamusHit();
uint8_t SFX_SamusDie();
uint8_t SFX_SetX2(uint8_t flags);
uint8_t SFX_SamusBall();
uint8_t SFXBeep();
uint8_t PowerUpMusic();
uint8_t IntroMusic();
uint8_t MotherBrainMusic();
uint8_t TourianMusic();
void EraseAllSprites();
void PrepVertMirror();
void CheckPalWrite();
void CheckPPUWrite();
void WritePPUCtrl();
void WriteScroll();
void ReadJoyPads();
void UpdateAge();
void AreaInit();
void MoreInit();
void SamusInit();
void GameEngine();
void GameOver();
void PauseMode();
void GoPassword();
void SamusIntro();
void WaitTimer();

} // namespace Game
