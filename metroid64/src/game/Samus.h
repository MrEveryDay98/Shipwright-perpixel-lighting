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

bool SamusJump();
void JumpSteer();
void JumpFire();
void SetSamusRoll();
bool SamusRoll();
uint8_t StopVertMovement();
void CheckBombLaunch();
void SamusPntUp();
void FireWeapon();
bool FindProjectileSlot(uint8_t& y);
void FireHorizontal();
void FireUp();
void InitProjectile(uint8_t y);
void SetProjectileAnim(uint8_t x, uint8_t anim);
uint8_t PlaceProjectile(uint8_t y);
void CheckMissileLaunch(uint8_t y);
void CheckMissileLaunchUp(uint8_t y);
void LaunchMissile(uint8_t y, uint8_t anim);
void SetBulletAnim(uint8_t y, uint8_t anim);
void SetWaveBeam(uint8_t y, uint8_t dir);
void SetIceBeam(uint8_t y);
bool SamusDoor();
void MoveOutDoor();
bool SamusDead();
void SamusDead2();
bool SamusElevator();
void DoorEnemyCleanup();
void ToggleSamusHi();
bool CheckMoveUp();
uint8_t BitScan(uint8_t a);
void ClearHealthChange();

} // namespace Game
