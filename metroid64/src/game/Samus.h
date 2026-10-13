#pragma once

// Bank 7: Samus's action handlers, health and animation control.

#include <cstdint>

namespace Game {

void UpdateSamus();
bool GoSamusHandler();
void SamusStand();
void SetSamusExplode();
void SetSamusRun();
void SetRunAcceleration(uint8_t dir);
bool SamusRun();
bool SamusRunJump();
bool RunPointUpCheck(bool& left);
void RunFire();
bool SetSamusData(uint8_t animDelay);
void SetmirrorCntrlBit();
bool IsScrewAttackActive();
void CheckHealthStatus();
void CheckHealthBeep();
bool IsSamusDead();
void SubtractHealth();
void AddHealth();
void CheckHitPushback();
uint8_t ClearHorzMvmntData();
uint8_t ClearHorzMvmntDataY(uint8_t y);
void StopHorzMovement();
void ClearHrztAnimData();
uint8_t SetSamusAnim(uint8_t anim);
uint8_t SetSamusNextAnim(uint8_t anim);
void SetSamusPntUp();
uint8_t NoHorzMoveNoDelay();
void AirSteer();
uint8_t ClearHorzData();
void SetSamusJumpPntUp();
void SetSamusJump();

void SamusJump();
void SamusRoll();
void SamusPntUp();
void SamusDoor();
bool SamusDead();
void SamusDead2();
void SamusElevator();
void SetSamusRoll();
void StopVertMovement();
void L_D09C();
void FireWeapon();
uint8_t BitScan(uint8_t a);
void LavaAndMoveCheck();
void ClearHealthChange();

} // namespace Game
